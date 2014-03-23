#ifndef TM_ASSERT_H
#define TM_ASSERT_H

#include </usr/include/assert.h>

#ifdef ORDERTM

#include "tm.h"

static inline void __assert_tm_fail (__const char *__assertion,
      __const char *__file, unsigned int __line, __const char *__function) {
   if(_xgettxid())
      _xretry(NEED_EXCLUSIVE);
   else
      __assert_fail(__assertion, __file, __line, __function);
}

#undef assert
#define assert(expr) \
   (__builtin_expect(!!(expr), 1) \
    ? __ASSERT_VOID_CAST (0) \
    : __assert_tm_fail (__STRING(expr), __FILE__, __LINE__, __ASSERT_FUNCTION))

#endif

#endif
