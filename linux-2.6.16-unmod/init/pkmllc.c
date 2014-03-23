/*============================================================================== 
 * pkmllc.c
 * kmalloc stress-test benchmark for comparison of contention performance
 * transactional memory v spinlocks. Create the specified number of kernel
 * threads, wait for them to come up, and then let them each call kmalloc
 * and kfree until the specified number of calls has been reached. 
 * Vary the size of the allocations by some not-so-random but not completely
 * obvious amount per call. The benchmark is designed to make sure the
 * xspin/cspin locks in the SLOB allocator are thoroughly exercised. 
 * -----------
 * Maintainer: 
 *  Chris Rossbach  rossbach@cs.utexas.edu
 *  Operating System and Architecture Lab, UT Austin 
 *============================================================================*/
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
#include <asm/atomic.h>
#include <linux/osamagic.h>

// #define KMLLC_VERBOSE

#define KMLLC_BACKOFF_WAIT      50
#define KMLLC_STATUS_INTERVAL   500
#define KMLLC_ARRAY_SIZE        100
#define KMLLC_BATCH_SIZE        100

#define KMLLC_SLOPE             16807
#define KMLLC_INTCPT            0
#define KMLLC_MOD               ((1u<<31) - 1)

#ifdef CONFIG_TX_KERNALLOC_BENCHMARK
struct task_struct * gvKmThreads[512];

atomic_t gKmAwakeCount;
atomic_t nKmSharedCounter;
atomic_t gKmDone;

int gKmThreadCount;
int gKmAllocations;

struct semaphore kmsemdone;

#define KMLLC_NSIZES 37
int km_sizes[] = {32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
                  100, 100, 100, 100, 100, 100,
                  300, 300, 300, 300, 300, 300, 300, 300, 300, 300, 300, 300,
                  300, 300, 300, 300, 300, 300, 300,
                  1024,
                  4096};

static int kmllc_thread_proc(void * data);

/** 
 * init_kmllc_module()
 * load the module 
 * create new kernel threads
 */
int 
init_kmllc_module( int nThreads,
                   int nAllocations,
                   int nInitSize ) {
  int i;
  int nSize;

  atomic_set(&gKmDone, 0);
  atomic_set(&gKmAwakeCount, 0);
  atomic_set(&nKmSharedCounter, 0);

  gKmThreadCount = nThreads;
  gKmAllocations = nAllocations;

  sema_init(&kmsemdone, 0);
  nSize = sizeof( struct task_struct * );

  for( i=0; i <nThreads; i++ ) {
    gvKmThreads[i] = 
      kthread_run(kmllc_thread_proc, (void*)i, "kmllc_thread_proc");
  }

  return 0;
}

/**
 * wait_kmllc_complete()
 * wait for all the spawned threads
 * to complete
 */
int
wait_kmllc_complete( void ) {
#ifdef KMLLC_VERBOSE
  printk(KERN_ERR "Waiting for kernalloc threads to complete\n" );
#endif
  down(&kmsemdone);
  printk(KERN_ERR "All %d threads complete!\n", atomic_read(&gKmDone) );
  return 0;
}

/** 
 * cleanup_kmllc_module()
 * remove the module 
 * terminate the kernel threads
 */
int 
cleanup_kmllc_module(void) {
  int i;
  printk( KERN_ERR "Cleaning up kmllcbm.." );
	for( i=0; i<gKmThreadCount; i++ ) {
    kthread_stop(gvKmThreads[i]);
    printk( KERN_ERR "..." );
  }
  printk( KERN_ERR "\ndone.\n" );
  return 0;
}

/** 
 * thread_proc()
 * thread function executing 
 * parallel increment of a shared counter. 
 */
int 
kmllc_thread_proc(void * data) {
  int bDone = 0;
  int nFinishNumber = -1;

  unsigned int seed = (unsigned int) data;
  unsigned int state = (seed*KMLLC_SLOPE + KMLLC_INTCPT) % KMLLC_MOD;

  void *p[KMLLC_ARRAY_SIZE];
  int j;
  for(j=0; j<KMLLC_ARRAY_SIZE; j++)
     p[j] = NULL;

#ifdef KMLLC_VERBOSE
  printk( KERN_ERR "Awaiting sync for kernalloc thread %d\n", nstnum );
#endif

  atomic_inc(&gKmAwakeCount);
  OSA_PRINT("Thread waiting", seed);
  while( atomic_read(&gKmAwakeCount) < gKmThreadCount )
     schedule_timeout_uninterruptible(1);
  OSA_PRINT("Thread start", seed);

#ifdef KMLLC_VERBOSE
  printk( KERN_ERR "Barrier crossed kernalloc kthread #%d\n", nstnum );
#endif

  while( !kthread_should_stop() && !bDone) {
    unsigned int allocs, i, pos, size;

    /* bite off a chunk of the allocations */
    allocs = atomic_add_return(KMLLC_BATCH_SIZE, &nKmSharedCounter);
    if(allocs > gKmAllocations) {
       bDone = 1;
       break;
    }

    // OSA_PRINT("Thread", seed);
    // OSA_PRINT("   at", allocs);

    /* allocate our chunk */
    i = 0;
    while(i < KMLLC_BATCH_SIZE) {
       pos = state % KMLLC_ARRAY_SIZE;
       size = km_sizes[state % KMLLC_NSIZES];

       if(p[pos] == NULL) {
          p[pos] = kmalloc(size, 1);
          i++;
       }
       else {
          kfree(p[pos]);
          p[pos] = NULL;
       }

       state = (state*KMLLC_SLOPE + KMLLC_INTCPT) % KMLLC_MOD;
    }

#ifdef KMLLC_VERBOSE      
    if((atomic_read(&nKmSharedCounter)%KMLLC_STATUS_INTERVAL)==0) {
      printk( KERN_ERR "kmllc allocation#: %d of %d (size = %d,done=%d)\n", 
              myValue, 
              gKmAllocations,
              nSize,
              bDone);
    }
#endif              

  }

#ifdef KMLLC_VERBOSE
  printk( KERN_ERR "kernalloc thread #%d done\n", nFinishNumber );
#endif

  if( bDone ) {
#ifdef KMLLC_VERBOSE      
     printk( KERN_ERR "thread finishing up...\n" );
#endif

     nFinishNumber = atomic_add_return(1, &gKmDone);
     // OSA_PRINT("Thread done", seed);

#ifdef KMLLC_VERBOSE
     printk( KERN_ERR "done flag %d set...\n", nFinishNumber );
#endif
  }

  /* are we the last to finish?  if so wake up the master */
  if(nFinishNumber == gKmThreadCount)
     up(&kmsemdone);
     
  while( !kthread_should_stop() )
    schedule_timeout_uninterruptible(1);
  return 0;
}

/**
 * sys_kernalloc_bm()
 * A system call which spawns the number of threads
 * specified in nThreads parameter, and lets them
 * contend for access to the shared counter in collectively
 * incrementing the value up to nTargetValue.
 */
asmlinkage long
sys_kernalloc_bm( unsigned long nThreads,
                  unsigned long nAllocations,
                  unsigned long nInitSize ) {
  printk( KERN_ERR "Invoking kernalloc benchmark...\n" );
  printk( KERN_ERR "Thread count:\t%d\n", (int)nThreads );
  printk( KERN_ERR "Allocation count:\t%d\n", (int)nAllocations );
  printk( KERN_ERR "Initial Size:\t%d\n", (int)nInitSize );
  init_kmllc_module( nThreads, nAllocations, nInitSize );
  wait_kmllc_complete();
  cleanup_kmllc_module();
  printk( KERN_ERR "kernalloc benchmark complete!\n" );
  return 0;
}
#else
asmlinkage long
sys_kernalloc_bm( unsigned long nThreads,
                  unsigned long nAllocations,
                  unsigned long nInitSize ) {
  printk( KERN_ERR "Build does not support kernalloc benchmark...\n" );
  return 0;
}

#endif

