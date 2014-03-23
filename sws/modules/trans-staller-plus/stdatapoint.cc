// MetaTM Project
// File Name: stdatapoint.cc
//
// Description: datapoint for staller stats
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2008. All Rights Reserved.
// See LICENSE file for license terms.

#include "stdatapoint.h"

stdatapoint::stdatapoint() {
  reset();
}
void stdatapoint::reset() {
  tx_min = min = 1e308;
  tx_max = max = 0;
  tx_avg = avg = 0;
  tx_stdev = stdev = 0;
  penalty_evts = 0;
}
ostream& operator<<(ostream& os, const stdatapoint& dp) {   
   os << "min: (" << dp.min << "," << dp.tx_min << ") ";
   os << "max: (" << dp.max << "," << dp.tx_max << ") ";
   os << "avg: (" << dp.avg << "," << dp.tx_avg << ") ";
   os << "std: (" << dp.stdev << "," << dp.tx_stdev << ") ";
   os << "penevts: " << dp.penalty_evts;
   return os; 
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
