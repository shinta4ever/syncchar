/* ===============================================
 * listbm.c
 * synthetic linked list microbenchmark
 * Attempts to maintain essentially the same
 * approach as CTM HLLNode ubm--on each iteration
 * use two random numbers which determine which
 * nodes are written, while all others are read.
 * Approach adapted from ramadan@cs.utexas.edu
 * maintainer: rossbach@cs.utexas.edu
 * ===============================================
 */
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
#define inc_stat(x)

#ifdef CONFIG_TX_LISTBM
#define RMAX          65535
#define DEF_NUM_THREADS 32
#define DEF_POUND_OPS 100
#define SYNC_SPINLOCK   0
#define SYNC_XSPINLOCK  1
#define SYNC_CXSPINLOCK  2
#define OSA_OUTBUF_SIZE  64
#define DEF_NUM_NODES    1024
#define DEF_SYNC_TYPE   SYNC_XSPINLOCK
#define DEF_TRAVERSALS  24
#define UPDATE_RATIO_PCT  20
#define ALG_SEARCH_N_SWAP 5435
#define ALG_RANDOM_ALTER  5436
#define ALG_SEARCH_INSDEL 5437
#define DEFAULT_ALG       ALG_SEARCH_INSDEL
#define SPARSE_FACTOR     3
#define WASTOID_THRESH    1000

// make sure absence of various
// primitives doesn't break the 
// build for listbm. 
#ifndef CONFIG_TX_CXSPIN
#define cx_atomic(x)  spin_lock(x)
#define cx_end(x)     spin_unlock(x)
#endif
#ifndef CONFIG_TX
#define xspin_lock(x)   spin_lock(x)
#define xspin_unlock(x) spin_unlock(x)
#endif

// implement a linked list. Reinvent the wheel, since
// the kernel's linked list is doubly linked. Moreover,
// the micro benchmark involves nothing more than
// than traversing a static list and incrementing
// random members of the list, meaning support for a
// rich variety of list primitives is unnecessary. 
typedef struct lbmnode_t {
  int val;
  struct lbmnode_t * next;
} lbmlist;
void lbm_list_add(lbmlist * l, int val) {
  lbmlist * p = l;
  lbmlist * newnode = NULL;
  while(p->next)
    p = p->next;
  newnode = kmalloc(sizeof(lbmlist), GFP_KERNEL);
  newnode->val = val;
  newnode->next = NULL;
  p->next = newnode;
}

// re-use the magic prng stuff from
// the pipebm. We should move all this
// into a shared micro-benchmark utils
// area some day. For now, it's duplicated
// here under different identifiers, since
// inclusion of each bm is controlled by
// a separate kconfig option.
typedef struct listbm_randparms_t {
  int tid;
} listbm_privprng;
void listbm_initrand(listbm_privprng * p, ulong s) {}
void listbm_nextstate(listbm_privprng * prng) {}
ulong listbm_lrand(listbm_privprng * p) { return get_osa_random() % RMAX; }

// global data--defaults and the
// shared list variable, along with 
// synchronization variables and stats
static volatile int gpoundops = DEF_POUND_OPS;
static volatile int gtcount = DEF_NUM_THREADS;
static volatile int gnnodes = DEF_NUM_NODES;
static volatile int gsynctype = DEF_SYNC_TYPE;
static volatile int gntraversals = DEF_TRAVERSALS;
static volatile int galgorithm = DEFAULT_ALG;
static volatile int gupdaterat = UPDATE_RATIO_PCT;
static volatile lbmlist * glbmlist = NULL;
static volatile int listbm_totcomplete = 0;
static volatile int listbm_totreads = 0;
static volatile int listbm_totwrites = 0;
static volatile int listbm_awake_count = 0;
spinlock_t listbm_sharelock;
spinlock_t listbm_tcountlock;
spinlock_t listbm_awakecountlock;
static struct semaphore listbm_sem_bm_done;
static struct semaphore listbm_sem_all_awake;
struct task_struct ** listbm_threads;
listbm_privprng * listbm_prngs = NULL;

// encapsulate mutual exclusion primitives:
// cspins, xspins, and cx primitives
void listbm_atomic_begin(spinlock_t * x) {
  switch(gsynctype) {
  case SYNC_SPINLOCK: cspin_lock(x); return;
  case SYNC_XSPINLOCK: xspin_lock(x); return;
  case SYNC_CXSPINLOCK: cx_atomic(x); return;
  default: printk(KERN_ERR "XXX! WTF! unsupported sync type!\n");
  }
}
void listbm_atomic_end(spinlock_t * x) {
  switch(gsynctype) {
  case SYNC_SPINLOCK: cspin_unlock(x); return;
  case SYNC_XSPINLOCK: xspin_unlock(x); return;
  case SYNC_CXSPINLOCK: cx_end(x); return;
  default: printk(KERN_ERR "XXX! WTF! unsupported sync type!\n");
  }
}

/** 
 * listbm_init()
 * build a static list with
 * the number of nodes specified by 
 * the nnodes parameter of the bm.
 */
void listbm_init(void) {
  int i = 0;
  volatile lbmlist * p = NULL;
  glbmlist = kmalloc(sizeof(lbmlist),GFP_KERNEL);
  if(!glbmlist) {
    printk(KERN_ERR "ListBM: OOM!\n");
    return;
  }
  glbmlist->val = 0;
  glbmlist->next = NULL;
  p = glbmlist;
  while(++i<gnnodes) {
    p->next = kmalloc(sizeof(lbmlist),GFP_KERNEL);
    p->next->next = NULL;
    // give each node it's own initial value
    // so that searching for values can be 
    // interesting. 
    if(galgorithm == ALG_SEARCH_INSDEL) {
      // populate the list with sparse 
      // values, so some searches fail
      p->next->val = i*SPARSE_FACTOR;
    } else {
      p->next->val = i;
    }
    p = p->next;
  }
}

/**
 * listbm_deinit()
 * free all the list nodes allocated
 * for the static list. 
 */ 
void listbm_deinit(void) {
  if(glbmlist) {
    volatile lbmlist * pfree = glbmlist;
    volatile lbmlist * pnext = glbmlist->next;
    while(pnext) {
      kfree((void*)pfree);
      pfree = pnext;
      pnext = pfree->next;
    } 
  }
}

/** 
 * listbm_do_stage()
 * at each stage, we're going to simply traverse
 * the list from beginning to end, and increment
 * the val field of any nodes whose index matches
 * one of two random numbers, mod list length.
 * NB: these are going to be damn short tx--we 
 * may need to introduce some artificial tx lengthening.
 */
void listbm_do_stage(listbm_privprng *p) {
  int h;
  volatile int wastoid;
  volatile lbmlist * l = glbmlist;
  if(galgorithm == ALG_SEARCH_INSDEL) {
    // with 20% probability, insert a random value (preserve sort order)
    // with 20% probability, delete a value at a random index
    // with 60% probability, just search for a random value
    // the list has been pre-populated with sparse ascending-sorted values
    char buf[256];
    int op = listbm_lrand(p) % 5;
    int val = (int)((listbm_lrand(p)*gnnodes*SPARSE_FACTOR)/(RMAX+1));
    // snprintf(buf, 256, "listbm tid: %d op: %d val: %d", p->tid, op, val);
    // OSA_PRINT(buf,0);
    switch(op) {
    case 0: 
      // insert a random value.
      // we know 0 will be at the front of the list, so
      // no biggie. 
      while(l && l->next) {
	if(l->val == val) 
	  break; // already in the list. bail.
	if(l->val < val && l->next->val > val) {
	  lbmlist * pnew = kmalloc(sizeof(lbmlist),GFP_KERNEL);
	  pnew->val = val;
	  pnew->next = l->next->next;
	  l->next = pnew;
	  break; // done
	}
	l = l->next;
      }
      break;  

    case 1: 
      // delete a random value if we can find it.
      // corner case the head
      if(l->val == val) {
	volatile lbmlist * temp = l;
	glbmlist = l->next;
	kfree((void*)temp);
      }
      while(l && l->next) {
	if(l->next->val == val) {
	  lbmlist * temp = l->next;
	  l->next = l->next->next;
	  kfree(temp);
	  break;
	}
	l = l->next;
      }
      break;

    case 2: case 3: case 4: default: 
      // just search for val
      while(l && l->val != val) {
	l = l->next;
      }    
      break;
    }
  } else if(galgorithm == ALG_SEARCH_N_SWAP) {
    // we are going to search for the element
    // i in all cases. If we are doing an update
    // (upd) then we are going to swap that 
    // element with the head of the list.
    int i = (int)((listbm_lrand(p)*gnnodes)/(RMAX+1));
    int upd = ((listbm_lrand(p)%RMAX)<(RMAX-((RMAX/100)*gupdaterat)));
    for(h=0; h<gnnodes-1; h++) {
      if(l && l->next) { 
	inc_stat(listbm_totreads);
	if(i == l->next->val) {
	  if(upd) {
	    volatile lbmlist * temp = l->next;
	    l->next = l->next->next;
	    temp->next = (lbmlist*)glbmlist;
	    glbmlist = temp;
	  }
	  break; // done
	}
	l = l->next;
      }
    }    
  } else {
    int i = (int)((listbm_lrand(p)*gnnodes)/(RMAX+1));
    int j = (int)((listbm_lrand(p)*gnnodes)/(RMAX+1));
    for(h=0; h<gnnodes; h++) {
      if(l) { 
	inc_stat(listbm_totreads);
	if(h==i || h==j) {
	  inc_stat(listbm_totwrites);
	  l->val++;
	}
	l = l->next;
      }
    }
  }
  // need some kind of think time, eh?
  for(h=0; h<WASTOID_THRESH; h++) {
    wastoid += h;
    asm volatile("" : : : "memory");
  }
  for(h=WASTOID_THRESH; h; h--) {
    wastoid -= h;
    asm volatile("" : : : "memory");
  }
  inc_stat(totcomplete);
}

/**
 * listbmthreadproc
 * iterate over stages, pounding on
 * the shared buffer cells allocated to 
 * that stage--synchronize according to 
 * the user-specified option. 
 */ 
int listbmthreadproc(void * prng) {
  char buf[OSA_OUTBUF_SIZE];
  listbm_privprng* p = (listbm_privprng*) prng;
  int tid = p->tid;
  int mytraversals = gntraversals;
  printk(KERN_ERR "tid:%d alive\n", tid);
  down(&listbm_sem_all_awake);
  if(++listbm_awake_count == gtcount) {
    OSA_MAGIC(OSA_CLEAR_STAT);
  }
  up(&listbm_sem_all_awake);
  while(listbm_awake_count < gtcount) {
    msleep(50);
  }
  printk(KERN_ERR "tid:%d GO!\n", tid);
  while(mytraversals) {
    listbm_atomic_begin(&listbm_sharelock);
    listbm_do_stage(p);
    listbm_atomic_end(&listbm_sharelock);
    mytraversals--;
    /*
    snprintf(buf, OSA_OUTBUF_SIZE, "snapshot listbm (tid:%d trav:%d) ", tid, mytraversals);
    OSA_PRINT(buf, strlen(buf));
    OSA_MAGIC(OSA_OUTPUT_STAT);
    */
  }
  down(&listbm_sem_bm_done);
  if(!--gtcount) {
    snprintf(buf, OSA_OUTBUF_SIZE, "Data listbm  ");
    OSA_PRINT(buf, strlen(buf));
    OSA_MAGIC(OSA_OUTPUT_STAT);
  }  
  up(&listbm_sem_bm_done);
  printk(KERN_ERR "Thread %d finished\n", tid);
  while( !kthread_should_stop() )
    schedule_timeout_uninterruptible(1);
  return 0;
}

long listbm(unsigned long nthreads,
	    unsigned long nnodes,
	    unsigned long ntraversals,
	    unsigned long nsynctype) {
  int i, savetcount;
  printk(KERN_ERR "List BM!\n");
  printk(KERN_ERR "nthreads(%ul)\nnnodes(%ul)\nntraversals(%ul)\nnsynctype(%ul)\n",
	 (unsigned int) nthreads, 
	 (unsigned int) nnodes, 
	 (unsigned int) ntraversals,
	 (unsigned int) nsynctype);

  gtcount = nthreads;
  gnnodes = nnodes;
  gntraversals = ntraversals;
  gsynctype = nsynctype;
  savetcount = nthreads;
  spin_lock_init(&listbm_sharelock);
  spin_lock_init(&listbm_tcountlock);
  spin_lock_init(&listbm_awakecountlock);
  sema_init(&listbm_sem_bm_done, 1);
  sema_init(&listbm_sem_all_awake, 1);
  listbm_init();
  listbm_threads = kmalloc(gtcount * sizeof(struct task_struct*),GFP_KERNEL);
  if(!listbm_threads) {
    printk(KERN_ERR "cannot allocate thread array-buffer!\n");
    return 1;
  }
  memset(listbm_threads, 0, gtcount * sizeof(struct task_struct*));
  listbm_prngs = kmalloc(gtcount * sizeof(listbm_privprng),GFP_KERNEL);
  if(!listbm_prngs) {
    printk(KERN_ERR "cannot allocate thread-local prngs!\n");
    return 1;
  }
  memset(listbm_prngs, 0, gtcount * sizeof(listbm_privprng));
  for(i=0;i<gtcount; i++) {
    listbm_prngs[i].tid = i;
    printk(KERN_ERR "about to create thread #%d\n",i);
    listbm_threads[i] = kthread_run(listbmthreadproc, 
				    (void *)&listbm_prngs[i], 
				    "listbmthreadproc"); 
    printk(KERN_ERR "created thread #%d\n",i);
  }	
  printk(KERN_ERR "main thread awaiting gtcount == 0\n");  
  while(gtcount) {
    msleep(50);
  }
  for(i=0;i<savetcount;i++) 
    kthread_stop(listbm_threads[i]);
  printk(KERN_ERR "List BM Complete!\n");
  listbm_deinit(); 
  kfree(listbm_threads);
  kfree(listbm_prngs);
  return 0;
}

asmlinkage long
sys_list_bm(unsigned long nthreads,
	    unsigned long nnodes,
	    unsigned long ntraversals,
	    unsigned long nsynctype) {
  return listbm(nthreads, 
		nnodes,
		ntraversals,
		nsynctype);
}

#else /* CONFIG_TX_LISTBM */
asmlinkage long
sys_list_bm(unsigned long nthreads,
	    unsigned long nnodes,
	    unsigned long ntraversals,
	    unsigned long nsynctype) {
  printk( KERN_ERR "Build does not support list \
                    contention micro-benchmark...\n" );
  return 0;
}
#endif

