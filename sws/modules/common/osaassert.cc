// MetaTM Project
// File Name: osaassert.cc
//
// Description: 
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006, 2007. All Rights Reserved.
// See LICENSE file for license terms.

#include <stdarg.h>
#include "osacommon.h"
#include "osaassert.h"
#include "simulator.h"
using namespace std;

char osa_ring_buf[OSA_RING_SIZE+OSA_MAX_LOG_MSG+1];
int osa_ring_pos = -1;

void osa_log(char *fmt, ...) {
   va_list ap;

   // initialize log buffer
   if(osa_ring_pos == -1) {
      memset(osa_ring_buf, 0, OSA_RING_SIZE+OSA_MAX_LOG_MSG+1);
      osa_ring_pos = 0;
   }

   // generate log message
   va_start(ap, fmt);
   osa_ring_pos += vsnprintf(&osa_ring_buf[osa_ring_pos], OSA_MAX_LOG_MSG,
         fmt, ap);
   osa_ring_buf[osa_ring_pos++] = '\n';

   if(osa_ring_pos >= OSA_RING_SIZE) {
      osa_ring_buf[osa_ring_pos] = '\0';
      osa_ring_pos = 0;
   }
}

void osa_dump_log(osamod_t *osamod) {
   if(osamod == NULL || osa_ring_pos == -1)
      return;

   // print from osa_ring_pos until end, then
   // from 0 until osa_ring_pos
   *osamod->pStatStream << "DUMPING LAST " << OSA_RING_SIZE << " LOG BYTES:"
      << endl;
   *osamod->pStatStream << &osa_ring_buf[osa_ring_pos];
   osa_ring_buf[osa_ring_pos] = 0;
   *osamod->pStatStream << osa_ring_buf;
}
extern "C"{
void osa_assert_fail(const char *assertion, const char *file, unsigned int line, struct _osamod_t *osamod = NULL)
{

   if(osamod != NULL){
      osa_dump_log(osamod);
   }

   stringstream ss;
   ss << "OSA_ASSERT FAILURE : " << assertion << " " << file << ":" << line;
   ss << "  " << OSA_get_sim_cpu()->name << " at " <<
      osa_get_sim_cycle_count(OSA_get_sim_cpu()) << endl;
   if (osamod != NULL)
   {
      OSA_break_simulation(ss.str().c_str(), osamod);
   }
   else
   {
      // What can we do, he didn't give us an osamod, so we do what we can..
      osa_break_simulation(ss.str().c_str());
   }
}
}
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
