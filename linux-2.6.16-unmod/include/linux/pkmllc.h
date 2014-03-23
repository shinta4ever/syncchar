#ifndef _PKMLLC_H_TX_
#define _PKMLLC_H_TX_

#ifdef CONFIG_TX_KERNALLOC_BENCHMARK

/** 
 * init_kmllc_module()
 * load the module 
 * create new kernel threads
 */
int 
init_kmllc_module( int nThreads,
                   int nAllocations,
                   int nInitSize );

/** 
 * cleanup_module()
 * remove the module 
 * terminate the kernel threads
 */
int cleanup_kmllc_module(void);

#endif // CONFIG_TX_KERNALLOC_BENCHMARK

asmlinkage long
sys_kernalloc_bm( unsigned long nThreads,
                  unsigned long nAllocations,
                  unsigned long nInitSize ); 

#endif // _PKMLLC_H_TX_

