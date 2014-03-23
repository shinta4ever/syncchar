/*============================================================================== 
 * prw.c
 * Microbenchmark where some threads are writers and the others are readers
 * -----------
 * Maintainer: 
 *  Don Porter porterde@cs.utexas.edu
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
#include <linux/prw.h>
#include <linux/osamagic.h>
#include <linux/syscalls.h>
#include <linux/random.h>

#define OSA_OUTBUF_SIZE 64

//#define KTHCBM_VERBOSE
#define KTHCBM_BACKOFF_WAIT     50
#define KTHCBM_STATUS_INTERVAL  500

typedef struct scratch_space {
  int x;
} ____cacheline_aligned_in_smp scratch_space_t;

// Number of writers
static int num_writers;
// Shared value
static volatile int shared_variable;

// Scratch space to write to - avoids stupid gcc optimizations and
// makes the transaction longer
static volatile scratch_space_t * local_scratch;

#ifdef CONFIG_TX_RW_BENCHMARK
static struct semaphore sem;
spinlock_t gKrwLock;
static struct semaphore semdone;
static struct semaphore scsembarrier;
static struct task_struct * gvThreads[256];

static unsigned long gnTargetValue;
static unsigned long gnThreadCount;
static unsigned long gnThreadsComplete;
static unsigned long gnAwakeCount;
static int writer_proc(void * data);
static int reader_proc(void * data);
static int krw_modulate_prio(void);
static int modulate_priority;

#ifdef CONFIG_TX_RW_USES_XSPINLOCK
#define splock xspin_lock
#define spunlock xspin_unlock
#else
#define splock spin_lock
#define spunlock spin_unlock
#endif 

/**
 * krw_modulate_prio()
 * modulate/perturb the priority of the current thread
 * by some random amount. It might be useful to convert
 * some processes to RT priority as well. 
 */
int krw_modulate_prio(void){
   struct sched_param prio;
   unsigned int randval;
   if(modulate_priority){
     randval = get_osa_random() % 50;
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
init_krw_module( unsigned long nThreadCount, 
		 unsigned long nTargetValue,
		 int modulate_prio, 
		 int writers) {
  int i;
  gnAwakeCount = 0;
  gnThreadsComplete = 0;
  gnThreadCount = nThreadCount;
  gnTargetValue = nTargetValue;
  modulate_priority = modulate_prio;
  num_writers = writers;
  shared_variable = 0;
  local_scratch = kmalloc(nThreadCount*sizeof(scratch_space_t), GFP_KERNEL);
  spin_lock_init( &gKrwLock );
  OSA_REGISTER_SPINLOCK(&gKrwLock, "gKrwLock", 9);
  printk( KERN_ERR "spinlock address: 0x%8x", (unsigned int) &gKrwLock);
  sema_init( &sem, 1 );
  sema_init( &semdone, 1 );
  sema_init( &scsembarrier, 1 );
  for( i=0; i<gnThreadCount; i++ ) {
#ifdef KTHCBM_VERBOSE
    printk( KERN_ERR "Spawning thread %d\n", i ); 
#endif
    if(i < num_writers){
      gvThreads[i] = kthread_run(writer_proc, (void *)i, "writer_proc"); 
    } else {
      gvThreads[i] = kthread_run(reader_proc, (void *)i, "reader_proc"); 
    }
  }
  return 0;
}

/** 
 * cleanup_module()
 * remove the module 
 * terminate the kernel threads
 */
int 
cleanup_krw_module(void) {
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
 * wait_krw_complete()
 * wait for all the spawned threads
 * to complete
 */
int
wait_krw_complete( void ) {
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
 * reader_proc()
 * thread function executing 
 * parallel increment of a shared counter. 
 */
int 
reader_proc(void * data) {
  int nstnum = (int) data;
  int done = 0;
  int my_share = gnTargetValue / gnThreadCount;
  int nFinishNumber = 0;
  char buf[OSA_OUTBUF_SIZE];
  krw_modulate_prio();
  down( &scsembarrier );
  gnAwakeCount++;
  // Reset the stats when the last process is ready to go
  if(gnAwakeCount == gnThreadCount){
    OSA_MAGIC(OSA_CLEAR_STAT);
  }
  up( &scsembarrier );

#ifdef KTHCBM_VERBOSE
  printk( KERN_ERR "Awaiting sync for rw thread %d\n", nstnum );
#endif
  while( gnAwakeCount < gnThreadCount ) {
    msleep(KTHCBM_BACKOFF_WAIT);
  }
#ifdef KTHCBM_VERBOSE
  printk( KERN_ERR "Barrier crossed rw kthread #%d\n", nstnum );
#endif
  while( !kthread_should_stop() && !done ) {


    // Work goes here
    // If we are a writer, write.  If we are a reader, read
    splock(&gKrwLock);
    local_scratch[nstnum].x = shared_variable;
    while(local_scratch[nstnum].x > 0){
      local_scratch[nstnum].x /= 32;
    }

    spunlock(&gKrwLock);
    done = !--my_share;

    // yield after roughly every 30 iterations
    // This number was picked because we are trying to use a window of
    // 1024 and 32 threads.  1024/32 = 32, 30 gives us some wiggle
    // room
    if(done % 30 == 0){
      yield();
    }

    if( done ) {
      down( &semdone );
      nFinishNumber = ++gnThreadsComplete;
      up( &semdone );

      // Stop the stats as soon as we are finished - avoid counting
      // tear down costs
      if(gnThreadsComplete >= gnThreadCount){
	snprintf(buf, OSA_OUTBUF_SIZE, "rw microbenchmark: %lu threads performing %lu ops",
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
 * writer_proc()
 * thread function executing 
 * parallel increment of a shared counter. 
 */
int 
writer_proc(void * data) {
#ifdef KTHCBM_VERBOSE
  int nstnum = (int) data;
#endif
  int done = 0;
  int my_share = gnTargetValue / gnThreadCount;
  int nFinishNumber = 0;
  char buf[OSA_OUTBUF_SIZE];
  krw_modulate_prio();
  down( &scsembarrier );
  gnAwakeCount++;
  // Reset the stats when the last process is ready to go
  if(gnAwakeCount == gnThreadCount){
    OSA_MAGIC(OSA_CLEAR_STAT);
  }
  up( &scsembarrier );

#ifdef KTHCBM_VERBOSE
  printk( KERN_ERR "Awaiting sync for rw thread %d\n", nstnum );
#endif
  while( gnAwakeCount < gnThreadCount ) {
    msleep(KTHCBM_BACKOFF_WAIT);
  }
#ifdef KTHCBM_VERBOSE
  printk( KERN_ERR "Barrier crossed rw kthread #%d\n", nstnum );
#endif
  while( !kthread_should_stop() && !done ) {


    // Work goes here
    // If we are a writer, write.  If we are a reader, read
    splock(&gKrwLock);
    shared_variable = 2;
    while(shared_variable < 65536){
      shared_variable *= shared_variable;
    }
    spunlock(&gKrwLock);
    done = !--my_share;

    // yield after roughly every 30 iterations
    // This number was picked because we are trying to use a window of
    // 1024 and 32 threads.  1024/32 = 32, 30 gives us some wiggle
    // room
    if(done % 30 == 0){
      yield();
    }

    if( done ) {
      down( &semdone );
      nFinishNumber = ++gnThreadsComplete;
      up( &semdone );

      // Stop the stats as soon as we are finished - avoid counting
      // tear down costs
      if(gnThreadsComplete >= gnThreadCount){
	snprintf(buf, OSA_OUTBUF_SIZE, "rw microbenchmark: %lu threads performing %lu ops",
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
sys_rw_bm(  unsigned long nThreads, 
	    unsigned long nTargetValue,
	    int modulate_prio, 
	    int writers) {
   printk(KERN_ERR "Invoking rw high-contention benchmark...\n" );
   printk(KERN_ERR "Thread count:\t%d\n", (int)nThreads );
   printk(KERN_ERR "Target value:\t%d\n", (int)nTargetValue );
   printk(KERN_ERR "Modulate Priority:\t%d\n", (int)modulate_prio );
   printk(KERN_ERR "Number of writers:\t%d\n", (int)writers );
   init_krw_module( nThreads, nTargetValue, modulate_prio, writers );
   wait_krw_complete();
   cleanup_krw_module();
   printk( KERN_ERR "rw benchmark complete!\n" );
   return 0;
}
#else
asmlinkage long
sys_rw_bm(  unsigned long nThreads,
	      unsigned long nTargetValue,
	      int modulate_prio,
	      int writers) {
  printk( KERN_ERR "Build does not support rw \
                    high-contention benchmark...\n" );
  return 0;
}
#endif

