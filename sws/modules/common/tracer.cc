// MetaTM Project
// File Name: tracer.cc
//
// Description: header file for tracer object 
// Tracer has circular buffer-like behavior, but is specialized to log
// messages: we're never going to deque off the front, so there's no
// sense implementing full circular buffer semantics. Just return the
// next stringstream, and once we wrap around, cap the number of valid
// entries.
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006-2009. All Rights Reserved.
// See LICENSE file for license terms.
#include "tracer.h"

tracer::tracer(int max_entries) {
   m_max = max_entries;
   m_v = new stringstream[max_entries];
   m_pos = 0;
   m_valid = 0;
}
tracer::~tracer() {
   delete [] m_v;
}
stringstream* tracer::msg_buf() {
   if(m_pos >= m_max) {
      m_pos = 0;
      m_valid = m_max;
   } else {
      if(m_valid < m_max) 
         m_valid++;
   }
   empty(m_pos);
   return &m_v[m_pos++];
}
void tracer::commit(ostream* s, bool erase) {
   int i, j, spos;
   spos = (m_valid < m_max)?0:(m_pos%m_max);
   for(i=spos, j=0; j<m_valid; i++, j++) {
      *s << m_v[i%m_max].str();
      if(erase) 
         empty(i);
   }
   s->flush();
   if(erase) {
      m_pos = 0;
      m_valid = 0;
   }
}
void tracer::empty() {
   int i, j;
   int spos = (m_valid < m_max)?0:(m_pos%m_max);
   for(i=spos, j=0; j<m_valid; i++, j++) {
      empty(i);
   }   
   m_pos = 0;
   m_valid = 0;
}
void tracer::empty(int i) {
   stringbuf * s = m_v[i%m_max].rdbuf();
   m_v[i%m_max].seekp(0);
   s->str("");
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
