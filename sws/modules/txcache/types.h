// MetaTM Project
// File Name: types.h
//
// Description: header file for cache object types
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006, 2007. All Rights Reserved.
// See LICENSE file for license terms.
#ifndef _TXCACHE_TYPES_H_
#define _TXCACHE_TYPES_H_
typedef uint64	 		  addr_t;
typedef unsigned 		  data_t;
typedef unsigned 		  data_size_t;
typedef int 	 		  line_t;
typedef unsigned 		  bank_size_t;
typedef unsigned 		  cache_size_t;
typedef uint64 			  tag_t;
typedef unsigned char             bytes_t;
#define INVALID  -1

#if 0
enum memacc_t {
  ACCESS_INVALID = -1, 
  ACCESS_READ, 
  ACCESS_WRITE,
  ACCESS_TXREAD,
  ACCESS_TXWRITE
};
enum model_t {
  MODE_SHARED = 1, 
  MODE_EXCLUSIVE = 2
};
#endif
// TMESI state space, adopting
// Rochester model for PDI:
enum state_t {
  STATE_TM = 7,
  STATE_TMI = 6,
  STATE_TE = 5, 
  STATE_TS = 4,
  STATE_TI = 3,
  STATE_M = 2,
  STATE_E = 1,
  STATE_S = 0,
  STATE_I = -1
};
enum action_t {
  tmesi_action_R,
  tmesi_action_W,
  tmesi_action_txR,
  tmesi_action_txW,
  tmesi_action_snR,
  tmesi_action_snW,
  tmesi_action_sntxR,
  tmesi_action_sntxW,
  tmesi_action_commit,
  tmesi_action_retry
};
#define TXVALID_S(s) \
  ((s==STATE_TM)||(s==STATE_TMI)||(s==STATE_TE)||(s==STATE_TS))
#define NOTXVALID_S(s) \
  ((s==STATE_M)||(s==STATE_E)||(s==STATE_S))
#define INVALID_S(s) \
  ((s==STATE_I)||(s==STATE_TI))
#endif
