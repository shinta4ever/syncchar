#ifndef LOCK_STM_H
#define LOCK_STM_H

#define STM_NO_BARRIERS
#define OSA_MAX_STM_THREADS 1
#define ORDERTM_SINGLE_COUNTER
#define ORDERTM_BEGIN_ORDER(protocol, critsec) critsec; protocol;
#define ORDERTM_END_ORDER(protocol, critsec) protocol; critsec;

#define STM_THREAD_T void
#define STM_SELF Self

#define STM_STARTUP() do{}while(0)
#define STM_SHUTDOWN() do{}while(0)

#define STM_NEW_THREAD() NULL

#define STM_INIT_THREAD(self, id) do{}while(0)

#define STM_FREE_THREAD(self) do{}while(0)

#define STM_BEGIN_WR() spin_lock(&globalLock);
#define STM_BEGIN_RO() spin_lock(&globalLock);
#define STM_END() spin_unlock(&globalLock);
#define STM_RESTART() assert(0)

#endif
