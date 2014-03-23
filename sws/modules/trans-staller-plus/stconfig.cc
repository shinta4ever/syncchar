// MetaTM Project
// File Name: stconfig.cc
//
// Description: configuration parameters for staller
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2008. All Rights Reserved.
// See LICENSE file for license terms.

#include "stconfig.h"

stconfig::stconfig() {
  defaults();
}
void stconfig::defaults() {
  stall_time = DEF_STALL_TIME;
  seed = DEF_SEED;
  stress_test = DEF_STRESS_TEST;
  stress_range = DEF_STRESS_RANGE;
  perturb_range = 4;
  cache_perturb = 0;
  frontbus_bandwidth_limit_bpc = FB_BW_bc;  
  frontbus_busy_penalty_factor = FB_BUSY_PENALTY_FACTOR;
  frontbus_check_interval = STALL_CHECK_INTERVAL;
  frontbus_stat_size = BW_STAT_SIZE;
  frontbus_stat_interval = STALLER_STAT_INTERVAL;
  bw_ts_index = 0;
};

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
