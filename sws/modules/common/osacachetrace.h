// MetaTM Project
// File Name: osacachetrace.h
//
// Description: validation tools for txcache/g-cache
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006, 2007. All Rights Reserved.
// See LICENSE file for license terms.

#ifndef OSACACHETRACE_H
#define OSACACHETRACE_H

#include "../txcache/types.h"

typedef enum cache_event_types_t {
  ot_prefetch = 0,
  ot_ctrl = 1,
  ot_uncacheable = 2,
  ot_read = 3,
  ot_write = 4,
  ot_repl = 5,
  ot_linefill = 6,
  ot_inv = 7,
  ot_upgrade = 8,
  ot_abort_tx = 9,
  ot_abort_line = 10,
  ot_commit_tx = 11,
  ot_commit_line = 12,
  ot_remote_invl = 13
} optype;

typedef enum hmstatus_t {
  hms_hit = 0,
  hms_miss = 1,
  hms_na = 2
} hmstatus;

typedef enum missclass_t {
  mc_vanilla = 0,
  mc_coherence = 1,
  mc_associativity = 2,
  mc_notx_to_tx = 3,
  mc_asymmetry = 4,
  mc_none = 5
} missclass;

typedef enum cache_type_t {
  ct_gcache = 0,
  ct_txcache = 1
} cache_type;

typedef struct cache_event_t {
  optype otyp;
  hmstatus status;
  missclass mc;
  cycles_t latency;
  enum state_t state;
  unsigned long long addr;
  unsigned long long tag;
  unsigned long long evaddr;
  enum cache_type_t ctyp;
  int comm_next;
  int write_next;
  int copy_next;
  int xID;
} cache_event;
typedef cache_event* pcache_event;

#ifdef __cplusplus 
extern "C" {
#endif
const char * gcstate_string(int s);
const char * txcstate_string(enum state_t s);
const char * event_str(pcache_event e);
const char * event_status(pcache_event e);
const char * event_line_state(pcache_event e);
const char * event_miss_classification(pcache_event e);
void dump_event(FILE * fp, pcache_event evt);
void check_state(pcache_event evt);
#ifdef __cplusplus
};
#endif

#endif



