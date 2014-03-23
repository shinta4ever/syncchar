#ifndef _PRBTREE_H_TX_
#define _PRBTREE_H_TX_

#ifdef CONFIG_TX_RBTREE_BENCHMARK

/** 
 * init_module()
 * load the module 
 * create new kernel threads
 */
int 
init_krbtree_module(  unsigned long nThreads,
		      unsigned long nTargetValue,
                      int balance, int max);

/** 
 * cleanup_module()
 * remove the module 
 * terminate the kernel threads
 */
int cleanup_krbtree_module(void);

#endif

/**
 * sys_shared_counter_bm()
 * A system call which spawns the number of threads
 * specified in nThreads parameter, and lets them
 * contend for access to the shared counter in collectively
 * incrementing the value up to nTargetValue.
 */
asmlinkage long
sys_rbtree_bm( unsigned long nThreads,
	       unsigned long nTargetValue,
	       int balance, int max); 

#endif
