// MetaTM Project
// File Name: writehist.h
//
// Description: 
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006, 2007. All Rights Reserved.
// See LICENSE file for license terms.

#ifndef WRITEHIST_H
#define WRITEHIST_H

using namespace std;
#include "simulator.h"

// Uncomment the following line to keep track of last-access to transactional memory locations
// NOTE: Intensive memory usage.  Accessible via addrinfo attribute
// #define TRACK_WRITE_HISTORY

typedef struct _write_info{
   int txid;
   osa_cycles_t cycleNum;
   char val;
} write_info_t;

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
