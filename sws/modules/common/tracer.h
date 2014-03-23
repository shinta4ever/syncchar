// MetaTM Project
// File Name: tracer.h
//
// Description: header file for tracer object
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006-2009. All Rights Reserved.
// See LICENSE file for license terms.
#ifndef _TRACER_H_
#define _TRACER_H_
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <ostream>
using namespace std;

class tracer {
 public:
   tracer(int max_entries);
   virtual ~tracer();
   void commit(ostream* s, bool empty=false);
   void empty();
   void empty(int i);
   stringstream * msg_buf();
 protected:
   stringstream * m_v;
   int m_max;
   int m_pos;
   int m_valid;
};

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
