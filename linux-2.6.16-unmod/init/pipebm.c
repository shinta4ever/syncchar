/* ===============================================
 * pipebm.c
 * synthetic phased workload benchmark
 * maintainer: rossbach@cs.utexas.edu
 * ===============================================
 */
#undef LOCAL_DEBUG
#ifndef LOCAL_DEBUG
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
#else
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "pthread.h"
#define atomic_begin(x) 
#define atomic_end(x)
#define inc_stat(x) (x++)
#define printk printf
#define kmalloc malloc
#define kfree free
#endif

#define RMAX          65535
#define DEF_NUM_THREADS 32
#define DEF_NUM_MEMOPS  1000000
#define DEF_NUM_PIPE_STAGES  8
#define DEF_SP_RATIO_PCT 100
#define DEF_RW_RATIO_PCT 75
#define DEF_BUFSIZE   16384
#define DEF_STAGE_OPS 128000
#define DEF_POUND_OPS 100
#define SYNC_SPINLOCK   0
#define SYNC_XSPINLOCK  1
#define SYNC_CXSPINLOCK  2
#define OSA_OUTBUF_SIZE  64

#ifdef CONFIG_TX_PIPEBM
int stageops = DEF_STAGE_OPS;
int poundops = DEF_POUND_OPS;
int tcount = DEF_NUM_THREADS;
int pipestages = DEF_NUM_PIPE_STAGES;
int spr = DEF_SP_RATIO_PCT;
int rwr = DEF_RW_RATIO_PCT;
int memops = DEF_NUM_MEMOPS;
int bufsize = DEF_BUFSIZE;
int memopcount = 0;
unsigned int * buf; 
int totspurious = 0;
int totcomplete = 0;
int totreads = 0;
int totwrites = 0;
int synctype = SYNC_SPINLOCK;
int awake_count = 0;
spinlock_t sharelock;
spinlock_t tcountlock;
spinlock_t awakecountlock;
static struct semaphore sem_bm_done;
static struct semaphore sem_all_awake;

#ifndef LOCAL_DEBUG
void atomic_begin(spinlock_t * x) {
  if(!synctype) {
    cspin_lock(x);
    return;
  }
  xspin_lock(x);
}
void atomic_end(spinlock_t * x) {
  if(!synctype) {
    cspin_unlock(x);
    return;
  }
  xspin_unlock(x);
}
#endif

/** 
 * in order to generate random numbers
 * in a transaction we need to privatize 
 * the state that the prng keeps! Otherwise,
 * all threads wind up writing and reading
 * the prng state, causing structural conflict.
 * The prng code below is adapted from: 
 * Yukihiro Matsumoto's Ruby random.c 
 * implementation, simplified for our needs,
 * and altered to keep prng state per-thread.
 */
#define N 624
#define M 397
#define STRANGE_ASS_NUMBER 19650218UL
#define MATRIX_A 0x9908b0dfUL   /* constant vector a */
#define UMASK 0x80000000UL /* most significant w-r bits */
#define LMASK 0x7fffffffUL /* least significant r bits */
#define MIXBITS(u,v) ( ((u) & UMASK) | ((v) & LMASK) )
#define TWIST(u,v) ((MIXBITS(u,v) >> 1) ^ ((v)&1UL ? MATRIX_A : 0UL))

/* Of course, private prngs are cool and all, 
 * but they take up a _lot_ of memory
 * and don't really contribute to the fidelity
 * of the micro-bnc. It's possible to use  
 * a magic instruction to get the same result
 * at a fraction of the cost. 
 */
#define MAGIC_PRNG
#ifdef MAGIC_PRNG
typedef struct randparms_t {
  int tid;
} privprng;
void initrand(privprng * p, ulong s) {}
void nextstate(privprng * prng) {}
ulong lrand(privprng * p) { return get_osa_random() % RMAX; }
#else
/* per-thread state for prng */
#ifdef LOCAL_DEBUG
typedef unsigned long ulong;
#endif
typedef struct randparms_t {
  int tid;
  ulong state[N];
  int left;
  int initf;
  ulong * next;
} privprng;

/**
 * initrand
 * accept a seed and initialize
 * the per-thread state array.
 */
void initrand(privprng * p, ulong s) {
  int j;
  ulong * tstate = p->state;
  tstate[0]= s & 0xffffffffUL;
  for (j=1; j<N; j++) {
    tstate[j] = (1812433253UL * (tstate[j-1] ^ (tstate[j-1] >> 30)) + j); 
    tstate[j] &= 0xffffffffUL;  
  }
  p->left = 1; 
  p->initf = 1;
}

/**
 * nextstate
 * bump the next pointer for the
 * given threads prng according to
 * some really arcane looking bit-munging
 */
void nextstate(privprng * prng) {
  ulong *p=prng->state;
  int j;
  prng->left = N;
  prng->next = prng->state;    
  for (j=N-M+1; --j; p++) 
    *p = p[M] ^ TWIST(p[0], p[1]);
  for (j=M; --j; p++) 
    *p = p[M-N] ^ TWIST(p[0], p[1]);
  *p = p[M-N] ^ TWIST(p[0], prng->state[0]);
}

/**
 * lrand()
 * generate a random number in
 * the [0,0xffffffff]-interval 
 * return a random number. When we are debugging
 * on a machine that doesn's support HTM, just use rand().
 * When on an HTM-machine, use a privatized prng--Linux
 * get_random_int has side effects which is going to cause
 * conflicts between tx that we don't want.
 */
ulong lrand(privprng * p) {
  ulong y;
  if(!--p->left) nextstate(p);
  y = *(p->next);
  p->next++;
  y ^= (y >> 11);
  y ^= (y << 7) & 0x9d2c5680UL;
  y ^= (y << 15) & 0xefc60000UL;
  y ^= (y >> 18);
  return y;
}
#endif // MAGIC_PRNG

#ifdef LOCAL_DEBUG
/**
 * pusage()
 * print usage paramters for pipeBM
 */
void pusage(char ** argv) {
  printk("%s -[options] <sync> x\n", argv[0]);
  printk("\ttc = thread count(32)\n");
  printk("\tmc = memop count(1000000)\n");
  printk("\tps = pipe stages(8)\n");
  printk("\tnsr = normal:spurious ratio (pct) (90)\n");
  printk("\trwr = read:write ratio (pct) (80)\n");
  printk("\tbs = shared buf size (pct) (16384)\n");
  printk("\tsync = primitive(no default):\n");
  printk("\t\tspinlock\n");
  printk("\t\txspinlock\n");
  printk("\t\tcxspinlock\n");
}

/** 
 * parseopt
 * get the benchmark parameters from the 
 * command line. The only required option is 
 * the sync primitive to use. Return non-zero 
 * to indicate a failure, after printing usage.
 */
int parseopt(int argc, char **argv) {
  int i;
  if(argc < 2) {
    pusage(argv);
    return 1;
  }
  for(i=1;i<argc;i++) {
    if(!strcmp(argv[i],"spinlock"))
      synctype = SYNC_SPINLOCK;
    else if(!strcmp(argv[i], "xspinlock")) 
      synctype = SYNC_XSPINLOCK;
    else if(!strcmp(argv[i], "cxspinlock"))
      synctype = SYNC_CXSPINLOCK;
    else if(!strcmp(argv[i], "-tc"))
      tcount = atoi(argv[++i]);
    else if(!strcmp(argv[i], "-mc"))
      memops = atoi(argv[++i]);
    else if(!strcmp(argv[i], "-ps"))
      pipestages = atoi(argv[++i]);
    else if(!strcmp(argv[i], "-nsr"))
      spr = atoi(argv[++i]);
    else if(!strcmp(argv[i], "-rwr"))
      rwr= atoi(argv[++i]);
    else if(!strcmp(argv[i], "-bs"))
      bufsize = atoi(argv[++i]);
    else {
      pusage(argv);
      return 2;
    }
  }
  printk("PipeBM Parameters:\n");
  printk("synctype = %d\n", synctype);
  printk("thread count = %d\n", tcount);
  printk("memop count = %d\n", memops);
  printk("pipe stages = %d\n", pipestages);
  printk("normal:spurious = %d\n", spr);
  printk("read:write = %d\n", rwr);
  printk("bufsize = %d\n", bufsize);
  return 0; // nice
}
#endif 

/** 
 * do_stage()
 * at each stage, we're going to pound on the
 * set of addresses that are specific to our 
 * stage, generating occasional spurious reads
 * or writes into neighboring stages
 */
void do_stage(privprng *p, int stage) {
  int i, j;
  int spf, spb;
  int wr, idx;
  int opcount = stageops;
  while(opcount--) {
    spf = (lrand(p)%RMAX) < (RMAX-((RMAX/100)*spr));
    spb = (lrand(p)%RMAX) > (((RMAX/100)*spr));
    wr = ((lrand(p)%RMAX)<(RMAX-((RMAX/100)*rwr)));
    // for smaller random values it's best to use
    // the upper bits rather than the lower bits:
    idx = (int)((lrand(p)*bufsize)/(RMAX+1));
    idx = idx - (idx%(pipestages));
    idx += stage;
    if(spb && idx) {
      inc_stat(totspurious);
      idx--;
    } else if(spf && idx < pipestages-1) {
      inc_stat(totspurious);
      idx++;
    }
    // beat the snot out of this 
    // memory cell for a while
    if(wr) {
      inc_stat(totwrites);
      for(i=0; i<poundops; i++) 
	*(&buf[idx]) = stage;
    } else {
      inc_stat(totreads);
      for(i=0; i<poundops; i++)
	j = *(&buf[idx]);
    }
    inc_stat(totcomplete);
  }
}

/**
 * pipebmthreadproc
 * iterate over stages, pounding on
 * the shared buffer cells allocated to 
 * that stage--synchronize according to 
 * the user-specified option. 
 */ 
int pipebmthreadproc(void * prng) {
  char buf[OSA_OUTBUF_SIZE];
  privprng* p = (privprng*) prng;
  int tid = p->tid;
  int mystage = 0;
  printk(KERN_ERR "tid:%d alive...about to incr awake_count(%d)\n", tid,awake_count);
  down(&sem_all_awake);
  if(++awake_count == tcount) {
    OSA_MAGIC(OSA_CLEAR_STAT);
  }
  up(&sem_all_awake);
  printk(KERN_ERR "tid:%d alive...waiting(awake=%d)\n", tid, awake_count);
  while(awake_count < tcount) {
    msleep(50);
  }
  printk(KERN_ERR "tid:%d GO!\n", tid);
  while(mystage < pipestages) {
    /* at each stage, we're going to do a
       fixed amount of work. The set of addresses
       touched is a function of the stage, with the
       exception that randomly, an address in the
       previous or next stage will be written
    */
    atomic_begin(&sharelock);
    do_stage(p, mystage);
    atomic_end(&sharelock);
    mystage++;
    printk(KERN_ERR "tid:%d: entering stage:%d\n", tid, mystage);
#if 0
    snprintf(buf, OSA_OUTBUF_SIZE, "snapshot pipebm (tid:%d stage:%d) ", tid, mystage);
    OSA_PRINT(buf, strlen(buf));
    OSA_MAGIC(OSA_OUTPUT_STAT);
#endif
  }
  down(&sem_bm_done);
  if(!--tcount) {
    snprintf(buf, OSA_OUTBUF_SIZE, "Data pipebm  ");
    OSA_PRINT(buf, strlen(buf));
    OSA_MAGIC(OSA_OUTPUT_STAT);
  }  
  up(&sem_bm_done);
  printk(KERN_ERR "Thread %d finished\n", tid);
  while( !kthread_should_stop() )
    schedule_timeout_uninterruptible(1);
  return 0;
}

long pipebm(unsigned long nthreads,
	    unsigned long nstages,
	    unsigned long nbufsize, 
	    unsigned long nsynctype) {
  int i, savetcount;
  struct task_struct ** threads;
  privprng * prngs;

  printk(KERN_ERR "PipeBM!\n");
  printk(KERN_ERR "nthreads(%ul)\nnstages(%ul)\nnbufsize(%ul)\nnsynctype(%ul)\n",
	 (unsigned int) nthreads, 
	 (unsigned int) nstages, 
	 (unsigned int) nbufsize, 
	 (unsigned int) nsynctype);

  tcount = nthreads;
  pipestages = nstages;
  bufsize = nbufsize;
  synctype = nsynctype;
  savetcount = tcount;

  spin_lock_init(&sharelock);
  spin_lock_init(&tcountlock);
  spin_lock_init(&awakecountlock);
  sema_init(&sem_bm_done, 1);
  sema_init(&sem_all_awake, 1);

  buf = kmalloc(bufsize*sizeof(int),GFP_KERNEL);
  if(!buf) {
    printk(KERN_ERR "cannot allocate shared buffer!\n");
    return 1;
  }
  memset(buf, 0, bufsize*sizeof(int));
  threads = kmalloc(tcount * sizeof(struct task_struct*),GFP_KERNEL);
  if(!threads) {
    printk(KERN_ERR "cannot allocate thread array-buffer!\n");
    return 1;
  }
  memset(threads, 0, tcount * sizeof(struct task_struct*));
  prngs = kmalloc(tcount * sizeof(privprng),GFP_KERNEL);
  if(!prngs) {
    printk(KERN_ERR "cannot allocate thread-local prngs!\n");
    return 1;
  }

  memset(prngs, 0, tcount * sizeof(privprng));
  printk(KERN_ERR "synctype = %d\n", synctype);
  printk(KERN_ERR "thread count = %d\n", tcount);
  printk(KERN_ERR "memop count = %d\n", memops);
  printk(KERN_ERR "pipe stages = %d\n", pipestages);
  printk(KERN_ERR "normal:spurious = %d\n", spr);
  printk(KERN_ERR "read:write = %d\n", rwr);
  printk(KERN_ERR "bufsize = %d\n", bufsize);
  for(i=0;i<tcount; i++) {
    initrand(&prngs[i],STRANGE_ASS_NUMBER +7*i);
    prngs[i].tid = i;
    printk(KERN_ERR "about to create thread #%d\n",i);
    threads[i] = kthread_run(pipebmthreadproc, 
			     (void *)&prngs[i], 
			     "pipebmthreadproc"); 
    printk(KERN_ERR "created thread #%d\n",i);
  }	
  printk(KERN_ERR "main thread awaiting tcount == 0\n");  
  while(tcount) {
    msleep(50);
  }
  for(i=0;i<savetcount;i++) 
    kthread_stop(threads[i]);
#ifdef LOCAL_DEBUG
  int idx = 0; 
  while(idx < bufsize) {
    if(idx % pipestages == (pipestages-1)) 
      printk(KERN_ERR "\t%d\n", buf[idx]);
    else 
      printk(KERN_ERR "\t%d", buf[idx]);
    idx++;
  }
  printk(KERN_ERR "Total memops = %d\n", totcomplete);
  printk(KERN_ERR "Total reads = %d\n", totreads);
  printk(KERN_ERR "Total writes = %d\n", totwrites);
  printk(KERN_ERR "Total spurious = %d\n", totspurious);
  printk(KERN_ERR "PipeBM complete!\n");
#endif
  printk(KERN_ERR "PipeBM Complete!\n");
  kfree(threads);
  kfree(buf);
  kfree(prngs);
  return 0;
}

#ifdef LOCAL_DEBUG
int main(int argc, char ** argv) {
#ifdef LOCAL_DEBUG
  if(parseopt(argc, argv))
    return 1;
#endif
  return pipebm(tcount, 
		pipestages,
		bufsize,
		synctype);
}
#endif

/* per don's request... */
asmlinkage long sys_don_rsrv1(void) { return 0; }
asmlinkage long sys_don_rsrv2(void) { return 0; }
asmlinkage long
sys_pipeline_bm(unsigned long nthreads,
		unsigned long nstages,
		unsigned long nbufsize, 
		unsigned long nsynctype) {
  return pipebm(nthreads, 
		nstages,
		nbufsize,
		nsynctype);
}

#else /* CONFIG_TX_PIPEBM */
asmlinkage long
sys_pipeline_bm(unsigned long nthreads,
		unsigned long nstages,
		unsigned long nbufsize, 
		unsigned long nsynctype) {
  printk( KERN_ERR "Build does not support pipeline \
                    contention benchmark...\n" );
  return 0;
}
#endif

