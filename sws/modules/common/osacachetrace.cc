// MetaTM Project
// File Name: osacachetrace.cc
//
// Description: validation tools for txcache/g-cache
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006, 2007. All Rights Reserved.
// See LICENSE file for license terms.

#include <simics/api.h>
#include <simics/alloc.h>
#include <simics/utils.h>
#include "osacachetrace.h"
#include "../txcache/types.h"

#ifdef __cplusplus
extern "C" {
#endif

static const char * txcstates[] = { "S", "E", "M", "TI", "TS", "TE", "TMI", "TM", "I" };
static const char * statestrs[] = { "I", "S", "E", "M" };
static const char * missclass_strings[] = { "V", "COH", "ASC", "NTX", "ASYM", "--" };
static const char * hmstatus_strings[] = { "H", "M", "NA" };
static const char * cache_event_strings[] = { 
  "PREF", "CTRL", "UNC", "R", "W", 
  "REPL", "FILL", "INVL", "UPGR", 
  "ABORT_TX", "\tABT_CL",
  "COMMIT_TX", "\tCMT_CL",
  "RINVL" };
static const char * gwtf = "??";
static char badstate[256];

const char * gcstate_string(int s) {
  switch(s) {
  case 0: 
  case 1: 
  case 2: 
  case 3: 
    return statestrs[s];
  default:
    sprintf(badstate, "XX: state = %d", s);
    fprintf(stdout, "XX: state = %d\n", s);
    char * p = NULL;
    sprintf(p, "FUCK GDB FUCK GDB!\n");
    SIM_break_simulation(p);
    return badstate;
  }
}

const char * txcstate_string(enum state_t s) {
  switch(s) {
  case STATE_TM:
  case STATE_TMI:
  case STATE_TE:
  case STATE_TS:
  case STATE_TI:
  case STATE_M:
  case STATE_E:
  case STATE_S:
    return txcstates[s];
  case STATE_I:
    return txcstates[8];
  }
  return txcstates[8];
}

const char * event_str(pcache_event e) {
  if(!e || e->otyp > ot_remote_invl || e->otyp < 0) return gwtf;
  return cache_event_strings[e->otyp];
}

const char * event_status(pcache_event e) {
  if(!e || e->status > 2 || e->status < 0) return gwtf;
  return hmstatus_strings[e->status];
}

const char * event_line_state(pcache_event e) {
  if(!e) return gwtf;
  if(e->ctyp == ct_gcache) 
    return gcstate_string((int)e->state);
  return txcstate_string(e->state);
}

const char * event_miss_classification(pcache_event e) {
  if(!e || e->mc < 0 || e->mc > mc_none) return gwtf;
  return missclass_strings[e->mc];
}

void check_state(pcache_event e) {
  if(e->ctyp == ct_gcache) {
    int s = (int) e->state;
    if(s < 0 || s > 3) {
      char * p = NULL;
      sprintf(p, "Fuck GDB");
      SIM_break_simulation("bad cache state");
    }
  } else { 
    int s = (int) e->state;
    if(s < -1 || s > 7) {
      char * p = NULL;
      sprintf(p, "Fuck GDB");
      SIM_break_simulation("bad cache state");
    }
  }
}

void dump_event(FILE * fp, pcache_event evt) {
  if(!fp || !evt) return;
  char szcomm[64];
  char szaddr[256];
  char szevaddr[256];
  char sztag[256];
  char szxid[64];
  szcomm[0] = 0;
  szaddr[0] = 0;
  szevaddr[0] = 0;
  szxid[0] = 0;
  if(evt->comm_next) {
    if(evt->write_next) {
      if(evt->copy_next) {
	sprintf(szcomm, "CWNCB");
      } else {
	sprintf(szcomm, "CWN");
      }
    } else {
      sprintf(szcomm, "C");
    }
  } else {
  }
  sprintf(szaddr, "0x%.8llX", evt->addr);
  sprintf(sztag, "0x%.8llX", evt->tag);
  if(evt->evaddr) 
    sprintf(szevaddr, "0x%.8llX", evt->evaddr);
  if(evt->xID)
    sprintf(szxid, "<%d>", evt->xID);

  // e.g. 
  // W   M(M=0xffff88ff:ASSC)  CWNCB   350
  // R   M(:V)                         24
  // -------------------------------------
  bool detailed = true;
  if(detailed) {
    fprintf(fp, 
	    "%s%s:%s[%s]\t%s(%s%s%s:%s)\t%s\t%llu",
	    event_str(evt),
	    (evt->xID?szxid:""),
	    szaddr,
	    sztag,
	    event_status(evt),
	    event_line_state(evt),
	    (evt->evaddr?"|*":""),
	    (evt->evaddr?szevaddr:""),
	    event_miss_classification(evt),
	    szcomm,
	    evt->latency);
  } else {
    fprintf(fp, 
	    "%s:%s\t%s\t%llu",
	    event_str(evt),
	    szaddr,
	    event_status(evt),
	    evt->latency);
  }
}

#ifdef __cplusplus
};
#endif

