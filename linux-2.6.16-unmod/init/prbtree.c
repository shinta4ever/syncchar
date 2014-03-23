/*============================================================================== 
 * prbtree.c
 * binary tree benchmark for comparison of scaling of spinlocks
 * vs. transactional memory.  Create a set of kernel threads
 * wait for them to come up, and then let them each perform a
 * share of a set some random rb tree operations "concurrently" until they are finished.
 * Despite the name, this is just a simple binary tree (for now).
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
#include <linux/prbtree.h>
#include <linux/osamagic.h>
#include <linux/syscalls.h>
#include <linux/random.h>

#define OSA_OUTBUF_SIZE 64
#define RMAX          65535

// #define KTHCBM_VERBOSE
#define KTHCBM_BACKOFF_WAIT     50
#define KTHCBM_STATUS_INTERVAL  500

// Probability of insert vs. delete (%)
//#define INSERT_DELETE_BALANCE 75
static int insert_delete_balance;
// Max value + 1 (min is 0)
//#define MAX_VAL 200
static int max_val;
static int successful_deletes = 0;

#ifdef CONFIG_TX_RBTREE_BENCHMARK
static struct semaphore sem;
spinlock_t gKrbtreeLock;
static struct semaphore semdone;
static struct semaphore scsembarrier;
static struct task_struct * gvThreads[256];

static unsigned long gnTargetValue;
static unsigned long gnThreadCount;
static unsigned long gnThreadsComplete;
static unsigned long gnAwakeCount;
static int krbtree_thread_proc(void * data);
static int krbtree_modulate_prio(void);
static int modulate_priority;

typedef struct tree_entry{
  int val;
  struct rb_node rb_node;
} ____cacheline_aligned_in_smp tree_entry_t;

static struct rb_root root;

#ifdef CONFIG_TX_RBTREE_USES_XSPINLOCK
#define splock xspin_lock
#define spunlock xspin_unlock
#else
#define splock spin_lock
#define spunlock spin_unlock
#endif 

void insert(int val){
  struct rb_node **p;
  struct rb_node *parent = NULL;
  tree_entry_t *__rq;

  // Do the allocation before we start the main critical section
  tree_entry_t *new_node = kmalloc(sizeof(tree_entry_t), 1);
  new_node->val = val;

  splock(&gKrbtreeLock);
  p = &root.rb_node;

  while (*p) {
    parent = *p;
    __rq = rb_entry(parent, tree_entry_t, rb_node);

    if (val < __rq->val)
      p = &(*p)->rb_left;
    else if (val > __rq->val)
      p = &(*p)->rb_right;
    else {
      spunlock(&gKrbtreeLock);
      // Someone else put this in already
      kfree(new_node);
      return;
    }
  }

  rb_link_node(&new_node->rb_node, parent, p);
  rb_insert_color(&new_node->rb_node, &root);
  spunlock(&gKrbtreeLock);
  return;
}

void remove (int val){
  struct rb_node *n;
  tree_entry_t *rq;

  splock(&gKrbtreeLock);
  n = root.rb_node;

  while (n) {
    rq = rb_entry(n, tree_entry_t, rb_node);

    if (val < rq->val)
      n = n->rb_left;
    else if (val > rq->val)
      n = n->rb_right;
    else { 
      rb_erase(n, &root);
      spunlock(&gKrbtreeLock);
      kfree(rq);
      // Yes, this is out of the lock.  It should give us a ballpark
      successful_deletes++;
      return;
    }
  }
  spunlock(&gKrbtreeLock);
  return;
}

/**
 * krbtree_modulate_prio()
 * modulate/perturb the priority of the current thread
 * by some random amount. It might be useful to convert
 * some processes to RT priority as well. 
 */
int krbtree_modulate_prio(void){
   struct sched_param prio;
   unsigned int randval;
   if(modulate_priority){
     randval = get_osa_random()%50;
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
init_krbtree_module( unsigned long nThreadCount, 
		     unsigned long nTargetValue,
		     int balance, int max) {
  int i;
  gnAwakeCount = 0;
  gnThreadsComplete = 0;
  gnThreadCount = nThreadCount;
  gnTargetValue = nTargetValue;
  insert_delete_balance = balance;
  max_val = max;
  // Don't really plan to use this
  modulate_priority = 0;
  spin_lock_init( &gKrbtreeLock );
  OSA_REGISTER_SPINLOCK(&gKrbtreeLock, "gKrbtreeLock", 13);
  printk( KERN_ERR "spinlock address: 0x%8x", (unsigned int) &gKrbtreeLock);
  sema_init( &sem, 1 );
  sema_init( &semdone, 1 );
  sema_init( &scsembarrier, 1 );
  for( i=0; i<gnThreadCount; i++ ) {
#ifdef KTHCBM_VERBOSE
    printk( KERN_ERR "Spawning thread %d\n", i ); 
#endif
    gvThreads[i] = kthread_run(krbtree_thread_proc, (void *)i, "krbtree_thread_proc"); 
  }
  return 0;
}

/** 
 * cleanup_module()
 * remove the module 
 * terminate the kernel threads
 */
int 
cleanup_krbtree_module(void) {
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
 * wait_krbtree_complete()
 * wait for all the spawned threads
 * to complete
 */
int
wait_krbtree_complete( void ) {
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
krbtree_thread_proc(void * data) {
#if defined (KTHCBM_VERBOSE)
  int nstnum = (int) data;
#endif
  int done = 0;
  int my_share = gnTargetValue / gnThreadCount;
  int nFinishNumber = 0;
  unsigned int randval1, randval2;
  char buf[OSA_OUTBUF_SIZE];
  krbtree_modulate_prio();
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

    // Work goes here
    randval1 = (unsigned int) (max_val * (get_osa_random()%RMAX) / (RMAX + 1));
    randval2 = (unsigned int) (100 * (get_osa_random()%RMAX) / (RMAX + 1));
    
    if(randval2 > insert_delete_balance){
      remove(randval1);
    } else {
      insert(randval1);
    }

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
	snprintf(buf, OSA_OUTBUF_SIZE, "rbtree microbenchmark: %lu threads performing %lu ops, %d deletes",
		 gnThreadCount, gnTargetValue, successful_deletes);
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
sys_rbtree_bm(  unsigned long nThreads, 
		unsigned long nTargetValue,
		int balance,
		int max) {
   printk(KERN_ERR "Invoking rbtree high-contention benchmark...\n" );
   printk(KERN_ERR "Thread count:\t%d\n", (int)nThreads );
   printk(KERN_ERR "Target value:\t%d\n", (int)nTargetValue );
   printk(KERN_ERR "Insert/delete balance (%%):\t%d\n", (int)balance );
   printk(KERN_ERR "Max value (%%):\t%d\n", (int)max );
   init_krbtree_module( nThreads, nTargetValue, balance, max );
   wait_krbtree_complete();
   cleanup_krbtree_module();
   printk( KERN_ERR "rbtree benchmark complete!\n" );
   return 0;
}
#else
asmlinkage long
sys_rbtree_bm(  unsigned long nThreads,
		unsigned long nTargetValue,
		int balance,
		int max) {
  printk( KERN_ERR "Build does not support rbtree \
                    high-contention benchmark...\n" );
  return 0;
}
#endif

