// MetaTM Project
// File Name: stdatapoint.h
//
// Description: datapoint for staller stats
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2008. All Rights Reserved.
// See LICENSE file for license terms.

#ifndef _TX_STALLER_STATS_DATAPOINT_H_
#define _TX_STALLER_STATS_DATAPOINT_H_
#include "../common/simulator.h"
#include "../include/member.h"
using namespace std; 

class stdatapoint {
 public:
  stdatapoint();
  void reset();
  pmember(double, min);
  pmember(double, max);
  pmember(double, avg);
  pmember(double, stdev);
  pmember(double, tx_min);
  pmember(double, tx_max);
  pmember(double, tx_avg);
  pmember(double, tx_stdev);
  pmember(unsigned long long, penalty_evts);
 public:
  friend ostream& operator<<(ostream& os, const stdatapoint& dp);
};
ostream& operator<<(ostream& os, const stdatapoint& dp);
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
