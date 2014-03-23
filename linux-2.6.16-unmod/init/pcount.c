/*============================================================================== 
 * pcount.c
 * shared counter benchmark for comparison of contention performance
 * transactional memory v spinlocks. Create the specified number of kernel
 * threads, wait for them to come up, and then let them each increment a 
 * shared counter collectively until it reaches the specified threshold.
 * -----------
 * Maintainer: 
 *  Chris Rossbach  rossbach@cs.utexas.edu
 *  Operating Systems and Architecture Lab, UT Austin 
 *===========================================================================*/
#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#if defined(MODVERSIONS)
#include <linux/modversions.h>
#endif
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <asm/semaphore.h>
#include <linux/spinlock.h>
#include <linux/pcount.h>
#include <linux/osamagic.h>
#include <linux/syscalls.h>
#include <linux/random.h>

#define OSA_OUTBUF_SIZE 64

#ifdef CONFIG_INDEPENDENT_COUNTER
// Make sure we are on different cache lines
struct counter {
  char pad1[64];
  volatile unsigned long counter;
  char pad2[64];
};
#endif

// #define KTHCBM_VERBOSE
#define KTHCBM_BACKOFF_WAIT     50
#define KTHCBM_STATUS_INTERVAL  500
#ifdef CONFIG_TX_KTHREAD_BENCHMARK
static struct semaphore sem;
spinlock_t gKmbmLock;
static struct semaphore semdone;
static struct semaphore scsembarrier;
static struct task_struct * gvThreads[512];
#ifdef CONFIG_INDEPENDENT_COUNTER
struct counter *gnSharedCounter;
#else
volatile unsigned long gnSharedCounter;
#endif

static unsigned long gnTargetValue;
static unsigned long gnThreadCount;
static unsigned long gnThreadsComplete;
unsigned long gnSyncType;
static unsigned long gnAwakeCount;
static int kmbm_thread_proc(void * data);
static int kmbm_modulate_prio(void);
static atomic_t atomic_counter;
static int modulate_priority;

/**
 * locked_inc()
 * Provide a mechanism to do a bus-locked
 * atomic increment instruction. The insightful
 * reader will notice there is no significant 
 * difference between locked_inc() and the asm-i386
 * atomic_increment; However, we need this function
 * to do the same thing regardless of what CONFIG_TX
 * flags are set: the function is here for experimentation
 * with bus-locking, and some build configurations will
 * turn atomic_inc into a transaction!
 */
static __inline__ void locked_inc(atomic_t *v)
{
   __asm__ __volatile__(
      LOCK "incl %0"
      :"=m" (v->counter)
      :"m" (v->counter));
}
/** 
 * unlocked_inc()
 * increment an atomic_t without locking the bus.
 * What for you ask? Well, outside a simulator, the answer
 * is "for no reason". In a simulator, we're curious whether
 * there's any difference in behavior from the locked version.
 */
static __inline__ void unlocked_inc(atomic_t *v)
{
   __asm__ __volatile__(
      "incl %0"
      :"=m" (v->counter)
      :"m" (v->counter));
}
/**
 * kmbm_modulate_prio()
 * modulate/perturb the priority of the current thread
 * by some random amount. It might be useful to convert
 * some processes to RT priority as well. 
 */
int kmbm_modulate_prio(void){
   struct sched_param prio;
   if(modulate_priority){
     int randval = get_random_int()%50;
     if(randval > 40){
       /* turn the thread into a real time process */
       prio.sched_priority = SCHED_RR;
       sys_sched_setscheduler(0,(randval*11)%99,&prio);
       return 0;
     }
     /* just change the process priority, leaving
      * it with it's current scheduler. */
     randval -= 20;
     sys_setpriority(PRIO_PROCESS,0,randval);
     return 0;
   }

   // Don't modulate priority
   /* turn the thread into a real time process at highest priority*/
   prio.sched_priority = SCHED_RR;
   sys_sched_setscheduler(0,99,&prio);
   return 0;
}
/** 
 * init_module()
 * load the module 
 * create new kernel threads
 */
int 
init_kmbm_module( unsigned long nThreadCount, 
                  unsigned long nTargetValue,
                  unsigned long nSyncType,
		  int modulate_prio) {
  int i;
  gnAwakeCount = 0;
#ifdef CONFIG_INDEPENDENT_COUNTER
  gnSharedCounter = kmalloc(sizeof(struct counter) * nThreadCount, GFP_KERNEL);
  for( i=0; i<gnThreadCount; i++ ) {
    gnSharedCounter[i].counter = 0;
  }
#else
  gnSharedCounter = 0;
#endif
  gnThreadsComplete = 0;
  gnSyncType = nSyncType;
  gnThreadCount = nThreadCount;
  gnTargetValue = nTargetValue;
  modulate_priority = modulate_prio;
#ifdef CONFIG_INDEPENDENT_COUNTER
  // Factor down the target value by a factor of NPROC
  gnTargetValue /= nThreadCount;
#endif
  spin_lock_init( &gKmbmLock );
  atomic_counter.counter = 0;
  OSA_REGISTER_SPINLOCK(&gKmbmLock, "gKmbmLock", 9);
  printk( KERN_ERR "spinlock address: 0x%8x", (unsigned int) &gKmbmLock);
  sema_init( &sem, 1 );
  sema_init( &semdone, 1 );
  sema_init( &scsembarrier, 1 );
  for( i=0; i<gnThreadCount; i++ ) {
#ifdef KTHCBM_VERBOSE
    printk( KERN_ERR "Spawning thread %d\n", i ); 
#endif
    gvThreads[i] = kthread_run(kmbm_thread_proc, (void *)i, "kmbm_thread_proc"); 
  }
  return 0;
}

/** 
 * cleanup_module()
 * remove the module 
 * terminate the kernel threads
 */
int 
cleanup_kmbm_module(void) {
  int i;
  printk( KERN_ERR "Cleaning up.." );
	for( i=0; i<gnThreadCount; i++ ) {
    kthread_stop(gvThreads[i]);
    printk( KERN_ERR ".." );
  }
  printk( KERN_ERR "\ndone.\n" );
  return 0;
}

/**
 * wait_kmbm_complete()
 * wait for all the spawned threads
 * to complete
 */
int
wait_kmbm_complete( void ) {
  int nDone = 0;
#ifdef KTHCBM_VERBOSE
  printk(KERN_ERR "Waiting for threads to complete\n" );
#endif
  while( !nDone ) {
    down( &semdone );
    nDone = ( gnThreadsComplete >= gnThreadCount );
    up( &semdone );
    msleep(50);
  }
  printk(KERN_ERR "All %d threads complete!\n", (int)gnThreadCount );
  return 0;
}

/** 
 * thread_proc()
 * thread function executing 
 * parallel increment of a shared counter. 
 */
int 
kmbm_thread_proc(void * data) {
#if defined (KTHCBM_VERBOSE) || defined(CONFIG_INDEPENDENT_COUNTER)
  int nstnum = (int) data;
#endif
  int done = 0;
  int myValue = 0;
  int nFinishNumber = 0;
  int my_share = gnTargetValue / gnThreadCount;
  char buf[OSA_OUTBUF_SIZE];
  kmbm_modulate_prio();
  down( &scsembarrier );
  gnAwakeCount++;
  // Reset the stats when the last process is ready to go
  if(gnAwakeCount == gnThreadCount){
    OSA_MAGIC(OSA_CLEAR_STAT);
  }
  up( &scsembarrier );

#ifdef KTHCBM_VERBOSE
  printk( KERN_ERR "Awaiting sync for shared counter thread %d\n", nstnum );
#endif
  while( gnAwakeCount < gnThreadCount ) {
    msleep(KTHCBM_BACKOFF_WAIT);
  }
#ifdef KTHCBM_VERBOSE
  printk( KERN_ERR "Barrier crossed shared counter kthread #%d\n", nstnum );
#endif
  while( !kthread_should_stop() && !done ) {
    if( gnSyncType == KMBM_SYNC_TYP_SEMAPHORE ) {
#ifdef CONFIG_TX_SEMAPHORE
#define txdown xdown
#define txup xup
#else 
#define txdown down
#define txup up
#endif
      txdown( &sem );
#ifdef CONFIG_INDEPENDENT_COUNTER
      myValue = ++gnSharedCounter[nstnum].counter;
#else
      myValue = ++gnSharedCounter;
#endif
      if( myValue % KTHCBM_STATUS_INTERVAL == 0 ) {
#ifdef KTHCBM_VERBOSE
        printk(KERN_ERR "gnSharedCounter: %d...\n\n", myValue);
#endif
      }        
#ifdef CONFIG_INDEPENDENT_COUNTER
      done = gnSharedCounter[nstnum].counter >= gnTargetValue; 
#else
#ifdef CONFIG_ROBUSTO_COUNTER     
      done = !(--my_share);
#else
      done = ((!--my_share && gnThreadsComplete < gnThreadCount-1) ||
              (gnSharedCounter >= gnTargetValue )); 
#endif
#endif
      txup( &sem );
    } else if(gnSyncType == KMBM_SYNC_TYP_LOCKED_INC) {
      locked_inc(&atomic_counter);

#ifdef CONFIG_ROBUSTO_COUNTER     
      done = !(--my_share);
#else
      done = (--my_share == 0 || atomic_counter.counter >= gnTargetValue);
#endif
    } else if(gnSyncType == KMBM_SYNC_TYP_UNLOCKED) {         
      unlocked_inc(&atomic_counter);
#ifdef CONFIG_ROBUSTO_COUNTER     
      done = !(--my_share);
#else
      done = (--my_share == 0 || atomic_counter.counter >= gnTargetValue);
#endif
    } else { // gnSyncType == KMBM_SYNC_TYP_SPINLOCK, default
#ifdef CONFIG_TX_KTHCBM_USES_XSPINLOCK
#define splock xspin_lock
#define spunlock xspin_unlock
#else
#define splock spin_lock
#define spunlock spin_unlock
#endif 
      int i, j;
      splock( &gKmbmLock );
      for (i = 0 ; i < 11 ; i++)  {
#ifdef CONFIG_INDEPENDENT_COUNTER
   	  myValue = ++gnSharedCounter[nstnum].counter; 
#else
   	  myValue = ++gnSharedCounter; 
#endif
      }
      for (j = 0 ; j < 10 ; j++) {
#ifdef CONFIG_INDEPENDENT_COUNTER
   	  myValue = --gnSharedCounter[nstnum].counter;
#else
   	  myValue = --gnSharedCounter;
#endif
      }
      // we want to try to balance the load across
      // the threads. Every thread gets an equal share
      // of the space by dividing the target value by the 
      // thread count. The very last thread alive gets the
      // privelege of doing more than their share if necessary
      // due to rounding error in the division.
#ifdef CONFIG_INDEPENDENT_COUNTER
      done = gnSharedCounter[nstnum].counter >= gnTargetValue; 
#else
#ifdef CONFIG_ROBUSTO_COUNTER     
      done = !(--my_share);
#else
      done = ((!--my_share && gnThreadsComplete < gnThreadCount-1) ||
              (gnSharedCounter >= gnTargetValue )); 
#endif

#endif
      spunlock( &gKmbmLock );
#ifdef KTHCBM_VERBOSE
      if( myValue % KTHCBM_STATUS_INTERVAL == 0 ) {
        printk(KERN_ERR "gnSharedCounter: %d...\n\n", myValue);
      }
      snprintf(buf, OSA_OUTBUF_SIZE, "snapshot counterbm");
      OSA_PRINT(buf, strlen(buf));
      OSA_MAGIC(OSA_OUTPUT_STAT);
#endif    
    }
    if( done ) {
      txdown( &semdone );
      nFinishNumber = ++gnThreadsComplete;
      txup( &semdone );

      // Stop the stats as soon as we are finished - avoid counting
      // tear down costs
      if(gnThreadsComplete >= gnThreadCount){
	snprintf(buf, OSA_OUTBUF_SIZE, "Shared counter: %lu threads counting to %lu", 
		 gnThreadCount, gnTargetValue);
	OSA_PRINT(buf, strlen(buf));
	OSA_MAGIC(OSA_OUTPUT_STAT);
      }
    }
  }
#ifdef KTHCBM_VERBOSE
  printk(KERN_ERR "Thread %d finished\n", nstnum);
#endif
  while( !kthread_should_stop() )
    schedule_timeout_uninterruptible(1);
  return 0;
}

/**
 * sys_shared_counter_bm()
 * A system call which spawns the number of threads
 * specified in nThreads parameter, and lets them
 * contend for access to the shared counter in collectively
 * incrementing the value up to nTargetValue.
 */
asmlinkage long
sys_shared_counter_bm( unsigned long nThreads,
                       unsigned long nTargetValue,
                       unsigned long nSyncType,
		       int modulate_prio) {
   printk(KERN_ERR "Invoking shared-counter high-contention benchmark...\n" );
   printk(KERN_ERR "Thread count:\t%d\n", (int)nThreads );
   printk(KERN_ERR "Target value:\t%d\n", (int)nTargetValue );
   printk(KERN_ERR "Modulate Priority:\t%d\n", (int)modulate_prio );
   printk(KERN_ERR "Sync type: ");
   switch(nSyncType){
   case KMBM_SYNC_TYP_SEMAPHORE: printk(KERN_ERR "Semaphores\n"); break;
   case KMBM_SYNC_TYP_LOCKED_INC: printk(KERN_ERR "Atomic Inc\n"); break;
   case KMBM_SYNC_TYP_UNLOCKED: printk(KERN_ERR "No locking\n"); break;
   default: 
   case KMBM_SYNC_TYP_SPINLOCK: printk(KERN_ERR "Spinlocks\n");  break;
   }

#ifdef CONFIG_ROBUSTO_COUNTER     
   if ((nTargetValue % nThreads) != 0)
   {
       // New requirement for robust-counter, must be a multiple of threads..
       printk(KERN_ERR "TargetValue not a multiple of thread count!\n"); 
       return -1;
   }
#endif
   init_kmbm_module( nThreads, nTargetValue, nSyncType, modulate_prio );
   wait_kmbm_complete();
   cleanup_kmbm_module();
   printk( KERN_ERR "shared counter benchmark complete!\n" );
#ifndef CONFIG_INDEPENDENT_COUNTER
#ifdef CONFIG_ROBUSTO_COUNTER     
   /* we only need to do the robustness this check for shared counter */
   if (gnSharedCounter == gnTargetValue)
   {
       printk( KERN_ERR "shared counter benchmark SUCCESS (gnSharedCounter=%li), retcode = 0!\n", gnSharedCounter);
       return 0;
   }
   else
   {
       printk( KERN_ERR "shared counter benchmark FAILURE (gnSharedCounter=%li) retcode = -1!\n", gnSharedCounter);
       return -1;
   }
#endif
#endif
   return 0;
}
#else
asmlinkage long
sys_shared_counter_bm( unsigned long nThreads,
                       unsigned long nTargetValue,
                       unsigned long nSyncType, 
		       int modulate_prio) {
  printk( KERN_ERR "Build does not support shared-counter \
                    high-contention benchmark...\n" );
  return 0;
}
#endif

