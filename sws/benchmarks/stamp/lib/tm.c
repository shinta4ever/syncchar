#include "tm.h"
#include "thread.h"

char osa_print_buf[OSA_PRINT_BUF_LEN+1] = {0};
char osa_line_buf[OSA_PRINT_BUF_LEN+1+6] = "OSAP: ";

/* This code only appears relevant for Owen's overflow experiments */
#ifdef ORDERTM

#ifdef ORDERTM_SINGLE_COUNTER
#define TID(x) 0
#else
#define TID(x) (*((long*)x))
#endif

// per thread ticket counters. 
volatile long vtickets[OSA_MAX_STM_THREADS] = { 0 };

// every thread gets a private snapshot buffer
volatile long vsnapshots[OSA_MAX_STM_THREADS*OSA_MAX_STM_THREADS] =  { 0 };

void init_ordertm_local(long tid) {
  OSA_PRINT("init_ordertm_local, tid", tid);
}

void ordertm_stm_protocol_begin(TM_ARGDECL_ALONE) { 
  vtickets[TID(Self)]++;
  osa_print("Start", vtickets[TID(Self)]);
  osa_print("\tgtid", TID(Self));
  magic_instruction(OSA_USER_OVERFLOW_BEGIN);
}

void ordertm_stm_protocol_end(TM_ARGDECL_ALONE) { 
  magic_instruction(OSA_USER_OVERFLOW_END);
  vtickets[TID(Self)]++;
  osa_print("End", vtickets[TID(Self)]);
  osa_print("\tgtid", TID(Self));
}

void ordertm_htm_protocol_begin(TM_ARGDECL_ALONE) {}

void ordertm_htm_protocol_end(TM_ARGDECL_ALONE) {
#ifdef ORDERTM_SINGLE_COUNTER
   _xpush();
   long t = vtickets[0];
   _xpop();
   if(t & 1) {
      _xpush();
      while(vtickets[0] <= t);
      _xpop();
   }
#else
   asm volatile (
         XPUSH_STR

         /* Copy tickets into snapshots */
         "mov %[max_threads], %%ecx\n\t"
         "mov %[vtickets], %%esi\n\t"
         "mov %[vsnap], %%edi\n\t"
         "rep; movsl\n\t"

         "movl %[vtickets], %%esi\n\t"

         /* Test each snapshot for low bit.  ecx is now zero */
         "1: movl (%[vsnap], %%ecx, 4), %%eax\n\t"
         "testl $0x1, %%eax\n\t"
         "jz 3f\n\t"

         /* If set, wait for ticket to increase */
         "2: cmpl (%%esi, %%ecx, 4), %%eax\n\t"
         "jge 2b\n\t"

         /* Loop until ecx == OSA_MAX_STM_THREADS */
         "3: inc %%ecx\n\t"
         "cmpl %[max_threads], %%ecx\n\t"
         "jne 1b\n\t"

         XPOP_STR
         :
         : [tid]"d"(TID(Self)),
           [vsnap]"b"(&vsnapshots[TID(Self)*OSA_MAX_STM_THREADS]),
           [vtickets]"i"(vtickets),
           [max_threads]"i"(OSA_MAX_STM_THREADS)
         : "eax", "ecx", "esi", "edi", "memory", "cc");
#endif
}

/*
void ordertm_htm_protocol_end(TM_ARGDECL_ALONE) {
   register int tid = TID(Self);
   register int i;
   _xpush();
   volatile long * p = vtickets;
   volatile long * psn = &vsnapshots[tid*OSA_MAX_STM_THREADS];
   while(p < vtickets+OSA_MAX_STM_THREADS) {
     *psn++ = *p++;
   }
   for(i = 0; i < OSA_MAX_STM_THREADS; i++) {
      if(psn[i] & 0x1)
         while(p[i] <= psn[i]);
   }
   _xpop();
   osa_print("Commit", 0);
}
*/

#endif

spinlock_t globalLock = { 1 };

/*
unsigned int _xgettxid() {
   int ret;
   __asm__ __volatile__(
         "jmp 1f\n\t"
         ".align 4096\n\t"
         ".rept 4090\n\t"
         "nop\n\t"
         ".endr\n\t"
         "1:\n\t"
         ".byte 0x0F\n\t.byte 0x1A\n\t"
         : "=a"(ret) : : "memory");
   return ret;
}
*/

#if 0
intptr_t fastcall _do_txload(vintp *var, STM_THREAD_T *self) {
#ifdef STM
   return TxLoad(self, var);
#else
   return *var;
#endif
}

void fastcall _do_txstore(vintp *var, intptr_t val, STM_THREAD_T *self) {
#ifdef STM
   TxStore(self, var, val);
#else
   *var = val;
#endif
}
#endif


