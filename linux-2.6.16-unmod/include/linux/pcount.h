#ifndef _PCOUNT_H_TX_
#define _PCOUNT_H_TX_

#define KMBM_SYNC_TYP_SPINLOCK  0
#define KMBM_SYNC_TYP_SEMAPHORE 1
#define KMBM_SYNC_TYP_LOCKED_INC 2
#define KMBM_SYNC_TYP_UNLOCKED 3
#ifdef CONFIG_TX_KTHREAD_BENCHMARK

/** 
 * init_module()
 * load the module 
 * create new kernel threads
 */
int 
init_kmbm_module( unsigned long nThreadCount, 
                  unsigned long nTargetValue,
                  unsigned long nSyncType,
		  int modulate_prio);

/** 
 * cleanup_module()
 * remove the module 
 * terminate the kernel threads
 */
int cleanup_kmbm_module(void);

#endif

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
		       int modulate_prio); 

#endif
