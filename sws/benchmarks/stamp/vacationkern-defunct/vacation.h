// Vacation.h  created by H.R.

#ifndef _VACATION_H_
#define _VACATION_H_
#include "tm.h"

struct semaphore vacation_sem_bm_done;
struct semaphore vacation_sem_all_awake;
volatile int gtcount = 0;
volatile int vacation_awake_count = 0;
#endif
