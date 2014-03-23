// MetaTM Project
// File Name: stconfig.h
//
// Description: this class implements a simple
// bandwidth measuring/limiting class. 
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2008. All Rights Reserved.
// See LICENSE file for license terms.

#ifndef _TX_STALLER_CONFIG_H_
#define _TX_STALLER_CONFIG_H_
#include "../common/simulator.h"
#include "../include/member.h"
using namespace std; 

#define STALL_CHECK_INTERVAL 10000    //size of data transfered eich trigger the BW check for BW limitation
#define FB_BW_bc 16                   //BW limit in bits per cycle
#define FB_BUSY_PENALTY_FACTOR  10    //Stall penalty in case of exceeding the BW limit 
#define STALLER_STAT_INTERVAL 10000   //Period length to probe BW for time series extraction
#define MAX_TIME_SERIES_SIZE 100000   //Maximum size of bw time series
#define BW_STAT_SIZE 100            //Size of probed BWs which triggers the stat data calculation
#define BIG_BW  1000000000

static const int DEF_STALL_TIME = 200;
static const int DEF_SEED = 0;
static const int DEF_STRESS_TEST = 0;
static const int DEF_STRESS_RANGE = 0;

class stconfig {
 public:
  stconfig();
  void defaults();
  member(int, stall_time);
  member(unsigned int, seed);
  member(cycles_t, perturb_range);
  member(int, cache_perturb);
  member(int, stress_test);
  member(cycles_t, stress_range);
  member(int, bw_ts_index);
  member(int, frontbus_bandwidth_limit_bpc);  
  member(int, frontbus_busy_penalty_factor);
  member(int, frontbus_check_interval);
  member(unsigned int, frontbus_stat_size); 
  member(unsigned int, frontbus_stat_interval);
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
