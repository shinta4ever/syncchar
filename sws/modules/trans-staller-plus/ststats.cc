// MetaTM Project
// File Name: ststats.cc
//
// Description: staller stats
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2008. All Rights Reserved.
// See LICENSE file for license terms.

#include "ststats.h"

ststats::ststats(){
   reset();
}

void ststats::reset() {
   current.reset();
   time_series.clear();
}

stdatapoint* ststats::getstats() {
   return &current;
}
vector<stdatapoint>* ststats::get_samples() {
   return &time_series;
}
vector<double>* ststats::get_ts_p() {
   return &ts;
}
vector<double>* ststats::get_tx_ts_p() {
   return &tx_ts;
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
