// MetaTM Project
// File Name: osaassert.h
//
// Description: 
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006, 2007. All Rights Reserved.
// See LICENSE file for license terms.


#ifndef OSAASSERT_H
#define OSAASSERT_H
#include "simulator.h"
using namespace std;

#define OSA_MAX_LOG_MSG 256
#define OSA_RING_SIZE 1024000

void osa_log(char *fmt, ...);
#define OSA_LOG_WORKS 1
#if OSA_LOG_WORKS
#define OSA_log(fmt, ...) osa_log(fmt, __VA_ARGS__)
#else
#define OSA_log(fmt, ...) do{}while(0)
#endif

extern "C" {
void osa_assert_fail(const char *assertion, const char *file, unsigned int line, struct _osamod_t *osamod);
}
#define OSA_assert(expr, osamod) \
  (static_cast<void> ((expr) ? 0 : (osa_assert_fail (__STRING(expr), \
  __FILE__, __LINE__, (osamod)), 0)))


#endif

/*
 * Local variables:
 *  c-indent-level: 3
 *  c-basic-offset: 3
 *  indent-tabs-mode: nil
 *  tab-width: 3
 * End:
 *
 * vim: ts=3 sw=3 expandtab
 */
