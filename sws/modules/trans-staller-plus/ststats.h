// MetaTM Project
// File Name: ststats.h
//
// Description: staller stats
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2008. All Rights Reserved.
// See LICENSE file for license terms.

#ifndef _TX_STALLER_STATS_H_
#define _TX_STALLER_STATS_H_
#include "../common/simulator.h"
#include "../include/member.h"
#include "stdatapoint.h"

using namespace std; 

class ststats {
 public:
  ststats();
  void reset();
  vector<stdatapoint>* get_samples();
  vector<double>* get_ts_p();
  vector<double>* get_tx_ts_p();
  stdatapoint* getstats();
  member(vector<double>, ts);
  member(vector<double>, tx_ts);
  member(vector<stdatapoint>, time_series);
  refvmember(stdatapoint, current);
  double get_min() { return current.get_min(); }
  double get_max() { return current.get_max(); }
  double get_avg() { return current.get_avg(); }
  double get_stdev() { return current.get_stdev(); }
  double get_tx_min() { return current.get_tx_min(); }
  double get_tx_max() { return current.get_tx_max(); }
  double get_tx_avg() { return current.get_tx_avg(); }
  double get_tx_stdev() { return current.get_tx_stdev(); }
  unsigned long long get_penalty_evts() { return current.get_penalty_evts(); }
  void set_min(double d) { return current.set_min(d); }
  void set_max(double d) { return current.set_max(d); }
  void set_avg(double d) { return current.set_avg(d); }
  void set_stdev(double d) { return current.set_stdev(d); }
  void set_tx_min(double d) { return current.set_tx_min(d); }
  void set_tx_max(double d) { return current.set_tx_max(d); }
  void set_tx_avg(double d) { return current.set_tx_avg(d); }
  void set_tx_stdev(double d) { return current.set_tx_stdev(d); }
  void set_penalty_evts(unsigned long long l) { return current.set_penalty_evts(l); }
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
