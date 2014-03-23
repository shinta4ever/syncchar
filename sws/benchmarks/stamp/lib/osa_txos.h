#ifndef TXOS_H
#define TXOS_H

#ifdef TXOS

#include "../../../../linux-2.6.22.6/include/asm/unistd.h"
#include <sys/syscall.h>
#include <unistd.h>

#define xend()                         syscall(__NR_xend)
#define xabort()                       syscall(__NR_xabort)

#define OSA_OP_SET_USER_SYSCALL_BIT 1219
#define OSA_OP_GET_USER_SYSCALL_BIT 1220

#define XSET_USER_TM_SYSCALL_BIT(bit) \
	asm volatile ("xchg %%bx, %%bx" : : "a"(bit), "S"(OSA_OP_SET_USER_SYSCALL_BIT))

static inline int XGET_SYSCALL_BIT() {
  int bit;
  __asm__ __volatile__ ("xchg %%bx, %%bx" : "=a"(bit) : "S"(OSA_OP_GET_USER_SYSCALL_BIT));
  return bit;
}

static __inline__ void TXOS_BEGIN() {
   if(XGET_SYSCALL_BIT())
     xabort();
}

static __inline__ int TXOS_END() {
  if(XGET_SYSCALL_BIT()){
    xend();
    XSET_USER_TM_SYSCALL_BIT(0);
    return 1;
  } 
  return 0;
}

#else

#define TXOS_BEGIN()
#define TXOS_END() 0

#endif /* TXOS */

#endif /* TXOS_H */
