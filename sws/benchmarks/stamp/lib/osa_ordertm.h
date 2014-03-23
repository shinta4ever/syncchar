#ifndef ORDERTM_H
#define ORDERTM_H

#ifdef ORDERTM

#include <string.h>
#include "stm.h"
#include "thread.h"

#ifndef OSA_MAX_STM_THREADS
#define OSA_MAX_STM_THREADS 32
#endif

void ordertm_stm_protocol_begin(STM_THREAD_T* Self);
void ordertm_stm_protocol_end(STM_THREAD_T* Self);

void ordertm_htm_protocol_begin(STM_THREAD_T* Self);
void ordertm_htm_protocol_end(STM_THREAD_T* Self);

#ifndef ORDERTM_BEGIN_ORDER
#define ORDERTM_BEGIN_ORDER(protocol, critsec) protocol; critsec
#endif
#ifndef ORDERTM_END_ORDER
#define ORDERTM_END_ORDER(protocol, critsec) critsec; protocol
#endif

#define ORDERTM_BEGIN() \
   do { \
      unsigned int status = _xbegin(1); \
      if(status & NEED_EXCLUSIVE) { \
         _xend(); \
         ORDERTM_BEGIN_ORDER(ordertm_stm_protocol_begin(Self), STM_BEGIN_WR());\
      }  \
      else { \
         ordertm_htm_protocol_begin(Self); \
      } \
   } while(0)

#define ORDERTM_END() { \
   if(_xgettxid()) { \
      ordertm_htm_protocol_end(Self); \
      _xend(); \
   } \
   else { \
      ORDERTM_END_ORDER(ordertm_stm_protocol_end(Self), STM_END()); \
   } \
} \

#ifdef STM_NO_BARRIERS
#  define ORDERTM_READ(var)    (var)
#  define ORDERTM_READ_P(var)         (var)
#  define ORDERTM_READ_F(var)         (var)

#  define ORDERTM_WRITE(var, val)     ({var = val; var;})
#  define ORDERTM_WRITE_P(var, val)   ({var = val; var;})
#  define ORDERTM_WRITE_F(var, val)   ({var = val; var;})

#  define ORDERTM_LOCAL_WRITE(var, val)      ({var = val; var;})
#  define ORDERTM_LOCAL_WRITE_P(var, val)    ({var = val; var;})
#  define ORDERTM_LOCAL_WRITE_F(var, val)    ({var = val; var;})
#else
#  define ORDERTM_READ(var)            (_xgettxid()?((intptr_t)var):STM_READ(var))
#  define ORDERTM_READ_P(var)          (_xgettxid()?(var):STM_READ_P(var))
#  define ORDERTM_READ_F(var)           (_xgettxid()?(var):STM_READ_F(var))

#  define ORDERTM_WRITE(var, val)     ({ if(!_xgettxid()) { STM_WRITE((var), val); } else { var = val; }})
#  define ORDERTM_WRITE_P(var, val)   ({ if(!_xgettxid()) { STM_WRITE_P((var), val); } else { var = val;}})
#  define ORDERTM_WRITE_F(var, val)   ({ if(!_xgettxid()) { STM_WRITE_F((var), val); } else { var = val;}})

#  define ORDERTM_LOCAL_WRITE(var, val)      ({ if(!_xgettxid()) { STM_LOCAL_WRITE(var, val); } else { var = val;}})
#  define ORDERTM_LOCAL_WRITE_P(var, val)    ({ if(!_xgettxid()) { STM_LOCAL_WRITE_P(var, val); } else { var = val;}})
#  define ORDERTM_LOCAL_WRITE_F(var, val)    ({ if(!_xgettxid()) { STM_LOCAL_WRITE_F(var, val); } else { var = val;}})
#endif



#if 0
#define fastcall __attribute__((regparm(3)))

intptr_t fastcall _do_txload(volatile intptr_t *var, STM_THREAD_T *self);
void fastcall _do_txstore(volatile intptr_t *var, intptr_t val,
      STM_THREAD_T *self);

static inline intptr_t read_var(volatile intptr_t *var, STM_THREAD_T *self) {
   intptr_t ret;
   asm volatile(
         /* Jump to the stm read barrier section */
         ".byte 0x0F,0x38\n\t"
         "jmp 2f\n\t"
         "mov (%[addr]), %[ret]\n\t"
         "1:\n\t"

         /* stm read barrier section */
         ".subsection 1\n\t"
         ".ifndef stm.barrier.section\n\t"
         "stm.barrier.section:\n\t"
         ".endif\n\t"
         "2:\n\t"

         /* Call TxLoad */
         "push %%ecx\n\t"
         "push %%edx\n\t"
         "mov %[self], %%edx\n\t"
         "call _do_txload\n\t"
         "pop %%edx\n\t"
         "pop %%ecx\n\t"
         "jmp 1b\n\t"

         ".previous\n\t"

         : [ret]"=a"(ret) : [addr]"a"(var), [self]"g"(self) : "cc");
   return ret;
}

static inline void write_var(volatile intptr_t *var, intptr_t val,
      STM_THREAD_T *self) {
   asm volatile(
         /* Jump to the stm read barrier section */
         ".byte 0x0F,0x38\n\t"
         "jmp 2f\n\t"
         "mov %[val], (%[addr])\n\t"
         "1:\n\t"

         /* stm read barrier section */
         ".subsection 1\n\t"
         ".ifndef stm.barrier.section\n\t"
         "stm.barrier.section:\n\t"
         ".endif\n\t"
         "2:\n\t"

         /* Call TxStore */
         "push %%eax\n\t"
         "push %%ecx\n\t"
         "push %%edx\n\t"
         "mov %[self], %%ecx\n\t"
         "call _do_txstore\n\t"
         "pop %%edx\n\t"
         "pop %%ecx\n\t"
         "pop %%eax\n\t"
         "jmp 1b\n\t"

         ".previous\n\t"
         : : [addr]"a"(var), [val]"d"(val), [self]"g"(self) : "cc");
}
#endif

#endif /* ORDERTM */

#endif /* ORDERTM_H */
