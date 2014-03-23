// MetaTM Project
// File Name: staller.h
//
// Description: this class implements a simple
// bandwidth measuring/limiting class. 
// This class implements a simple staller. If a transactions is
// stallable, it will return a fixed stall time. It can be used for a
// simple memory simulation at the end of a cache hierarchy.
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2008. All Rights Reserved.
// See LICENSE file for license terms.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <simics/api.h>
#include <simics/alloc.h>
#include <simics/utils.h>
#include "../common/simulator.h"
#include "../common/osacommon.h"
#include "staller.h"

int booted =0;

static void staller_stat_tick(conf_object_t *staller, void *arg)
{
   attr_value_t ret;
   float bw, bwtx;
   simple_staller_t *st = (simple_staller_t *)staller; 
   conf_object_t *cpu;

   vector<double>* pts = st->stats->get_ts_p();
   vector<double>* ptxts = st->stats->get_tx_ts_p();
   STALLER_ASSERT(pts->size() == ptxts->size());

   if(pts->size() >= st->config->get_frontbus_stat_size()) {

      // if we've collected enough data points
      // for the current epoch, create a time-series
      // data point for the bw in this epoch. 

      double min = BIG_BW;
      double mintx = BIG_BW;
      double max = 0; 
      double maxtx = 0;
      double sum = 0;
      double sumtx = 0;
      double sumsq = 0;
      double sumsqtx = 0;
      stdatapoint sample;
      vector<double>::iterator ptsi = pts->begin();
      vector<double>::iterator ptxsi = ptxts->begin(); 
      vector<stdatapoint>* psamples = st->stats->get_samples();

      while(ptsi != pts->end() && ptxsi != ptxts->end()) {
         double dp = (*ptsi);
         double txdp = (*ptxsi);
         sum += dp;
         sumtx += txdp;
         max = MAX(max, dp);
         maxtx = MAX(maxtx, txdp);
         min = MIN(min, dp);
         mintx = MIN(mintx, txdp);
         ptsi++;
         ptxsi++;
      }

      sample.avg = sum / pts->size();
      sample.tx_avg = sumtx / ptxts->size();
      sample.min = min;
      sample.tx_min = mintx;
      sample.max = max;
      sample.tx_max = maxtx; 

      ptsi = pts->begin();
      ptxsi = ptxts->begin();
      while(ptsi != pts->end() && ptxsi != ptxts->end()) {
         double dp = (*ptsi);
         double txdp = (*ptxsi);
         double delta = (dp - sample.avg);
         double txdelta = (txdp - sample.tx_avg);
         sumsq += delta*delta;
         sumsqtx += txdelta*txdelta;
         ptsi++;
         ptxsi++;
      }
      sample.stdev = sqrt(sumsq) / (double) pts->size();
      sample.tx_stdev = sqrt(sumsqtx) / (double) ptxts->size();
      sample.penalty_evts = st->penaltycounter;
      
      // stash the data point,
      // clear the data for this epoch
      if(st->verbose)  {
         if(booted)
            cout << "HEY kernel booted" << endl;
         cout << sample << endl;
      }   
      psamples->push_back(sample);
      st->stats->set_current(*&sample);
      st->penaltycounter = 0;
      pts->clear();
      ptxts->clear();
   }

   // insert a new sample of the bw consumption
   ret = get_frontbus_bandwidth_probe(NULL, staller, NULL);        
   bw = ret.u.floating;   
   ret = get_frontbus_bandwidth_tx_probe(NULL, staller, NULL);
   bwtx = ret.u.floating;
   pts->push_back(bw);
   ptxts->push_back(bwtx);

   //printf("staller ticks BW:%f , BWTX: %f\n", bw, bwtx );

   cpu = SIM_current_processor();
   SIM_time_post_cycle((conf_object_t *)st, 
                       st->config->get_frontbus_stat_interval(), 
                       Sim_Sync_Processor, 
                       staller_stat_tick, 
                       NULL);
}

extern "C" {   
conf_object_t *
st_new_instance(parse_object_t *pa)
{
   simple_staller_t *st = MM_ZALLOC(1, simple_staller_t);
   SIM_log_constructor(&st->log, pa);
   st->stats = &st->ostats;
   st->config = &st->oconfig;
   st->config->set_perturb_range(4);
   st->past_bios = 0;
   st->config->set_frontbus_bandwidth_limit_bpc(FB_BW_bc);  
   st->config->set_frontbus_busy_penalty_factor(FB_BUSY_PENALTY_FACTOR);
   st->config->set_frontbus_check_interval(STALL_CHECK_INTERVAL);
   st->memops_counter = 0;
   st->staller_previous_cycle = 0;
   st->penaltycounter = 0;
   st->memops_probe_counter = 0;
   st->memops_tx_probe_counter = 0;
   st->frontbus_previous_probe_cycle = 0;
   st->frontbus_previous_tx_probe_cycle = 0;
   st->logcounter = 0;
   st->verbose = 0;
   st->config->set_frontbus_stat_size(BW_STAT_SIZE);
   st->config->set_frontbus_stat_interval(STALLER_STAT_INTERVAL);
   
   return (conf_object_t *) st;
   
}

int st_initialized = 0;
int current_penalty = 0;
int max_penalty = 0;

cycles_t
st_operate(conf_object_t *mem_hier, conf_object_t *space, 
           map_list_t *map, generic_transaction_t *mem_op) {

   simple_staller_t *st = (simple_staller_t *) mem_hier;
   conf_object_t *cpu;   
   cycles_t staller_current_cycle;
   int factor = 1;
   bool fast_caches;
   conf_object_t *common;
  
   common = osa_get_object_by_name("common");
   if (common) 
   	fast_caches = SIM_get_attribute(common, "fast_caches").u.boolean; 
   else 
	fast_caches = 1;

   if ( !fast_caches )
      booted = 1; 

   cpu = SIM_current_processor();
   staller_current_cycle = SIM_cycle_count(cpu);

   if (!st_initialized) {
      SIM_time_post_cycle((conf_object_t *)st, 
                       st->config->get_frontbus_stat_interval(), 
                       Sim_Sync_Processor, 
                       staller_stat_tick, 
                       NULL); 
      st_initialized = 1;
   }
   if (mem_op->may_stall) {
      st->memops_probe_counter += mem_op->size;
      if ((long)(mem_op->user_ptr) & 0x7fffffff) {
         st->memops_tx_probe_counter += mem_op->size;
      }
   }

   if (!current_penalty)
      current_penalty = st->config->get_frontbus_busy_penalty_factor();
   
   if (st->config->get_frontbus_bandwidth_limit_bpc() && !fast_caches) {
      st->memops_counter += mem_op->size;
      if ((staller_current_cycle - st->staller_previous_cycle) > 
         st->config->get_frontbus_check_interval()) {
    
            if ((staller_current_cycle - st->staller_previous_cycle)* 
            st->config->get_frontbus_bandwidth_limit_bpc() <= st->memops_counter*8) {
               factor = current_penalty;
               st->penaltycounter++;
               current_penalty++;
               if (current_penalty > max_penalty) {
                  max_penalty = current_penalty;
                  printf("MAX penalty is %d\n", max_penalty);
               }         
            } else {
               st->staller_previous_cycle = staller_current_cycle; 
               st->memops_counter = 0;
               current_penalty = st->config->get_frontbus_busy_penalty_factor();
            }
      }
   }
   mem_op->reissue = 0;
   mem_op->block_STC = 1;
   if (mem_op->may_stall){
      int stall =  st->config->get_stall_time();
      if(st->past_bios &&
         st->config->get_stress_test() &&
         st->config->get_stress_range() > 0) {
         // if we are in stress mode, additionally randomly perturb
         // the stall amount by a potentially wide margin either
         // direction. What we really want here is high variability,
         // but we also want an occasional very very long wait.
         int r = st->config->get_stress_range();
         int s = random() % (r+1);
         if(r > st->config->get_stall_time()) {
            stall = s; // allows us to return 0 values sometimes
         } else {
            s *= ((random()%2)&0x1)?-1:1;
            stall += s;
         }
         // pr( "stressed timing: %d\n", stall);
      }
      // Randomly perturb the cache timing if we have a non-zero range
      // (regardless of whether we are in stress test mode or not
      if(st->past_bios &&
         st->config->get_perturb_range() > 0){
         // Add 1 to the range before moding to make it inclusive
         // i.e. 4 cycles should be [0,4], not [0,4) 
         stall += random() % (st->config->get_perturb_range() + 1);
         // pr( "Poenas Dare: %d\n", stall);
      }
      return (factor * stall);
      // return (10000);
   } else {
      return 0;
   }
}

STALLER_CONFIG_ATTR(frontbus_bandwidth_limit_bpc)
STALLER_CONFIG_ATTR(frontbus_busy_penalty_factor)
STALLER_CONFIG_ATTR(frontbus_check_interval)
STALLER_CONFIG_ATTR(frontbus_stat_interval)
STALLER_CONFIG_ATTR(frontbus_stat_size)
STALLER_CONFIG_ATTR(stall_time)
STALLER_CONFIG_ATTR(cache_perturb)
STALLER_CONFIG_ATTR(seed)
STALLER_CONFIG_ATTR(stress_test)
STALLER_CONFIG_ATTR(stress_range)
STALLER_ATTR(past_bios)
STALLER_ATTR(verbose)
STALLER_STATS_ATTR(min)
STALLER_STATS_ATTR(max)
STALLER_STATS_ATTR(avg)
STALLER_STATS_ATTR(stdev)
STALLER_STATS_ATTR(tx_min)
STALLER_STATS_ATTR(tx_max)
STALLER_STATS_ATTR(tx_avg)
STALLER_STATS_ATTR(tx_stdev)
STALLER_STATS_INT_ATTR(penalty_evts)

static set_error_t
set_st_stats(void *dont_care, 
             conf_object_t *obj,
             attr_value_t *val, 
             attr_value_t *idx) {
   // is a setter meaningful for this attr?
   simple_staller_t *st = (simple_staller_t *) obj;
   st->frontbus_bandwidth_probe = val->u.integer;  
   return Sim_Set_Ok;
}

static attr_value_t                                             
get_st_stats(void *dont_care, 
             conf_object_t *obj,       
             attr_value_t *idx) {                                                               
   
   simple_staller_t *st = (simple_staller_t *) obj;

   // first compute an overall average, given
   // the time series data for bw samples,
   // then dump both the current epoch and the
   // cumulative data. Note that we collect 
   // stdev per epoch--we'll report an average 
   // of stdevs here. 
   stdatapoint overall;
   vector<stdatapoint>::iterator si;
   vector<stdatapoint>* psamples = st->stats->get_samples();
   for(si=psamples->begin(); si!=psamples->end(); si++) {
      stdatapoint * psd = &(*si);
      overall.min = MIN(overall.min, psd->min);
      overall.max = MAX(overall.max, psd->max);
      overall.avg += psd->avg;
      overall.stdev += psd->stdev;
      overall.tx_min = MIN(overall.tx_min, psd->tx_min);
      overall.tx_max = MAX(overall.tx_max, psd->tx_max);
      overall.tx_avg += psd->tx_avg;
      overall.tx_stdev += psd->tx_stdev;
      overall.penalty_evts += psd->penalty_evts;
   }
   if(psamples->size()) {
      overall.avg /= psamples->size();
      overall.stdev /= psamples->size();
      overall.penalty_evts /= psamples->size();
   }

   // now output stats as csv. First
   // line lists attribute names
   stringstream ss;
   ss << "staller stats:";
   ss << "       , min, max, avg, stdev, tx_min, tx_max, tx_avg, tx_stdev, penevts" << endl;
   ss << "current, "
      << st->stats->get_min() << ", "
      << st->stats->get_max() << ", "
      << st->stats->get_avg() << ", "
      << st->stats->get_stdev() << ", "
      << st->stats->get_tx_min() << ", "
      << st->stats->get_tx_max() << ", "
      << st->stats->get_tx_avg() << ", "
      << st->stats->get_tx_stdev() << ", "
      << st->stats->get_penalty_evts()
      << endl;
   ss << "overall, "
      << overall.get_min() << ", "
      << overall.get_max() << ", "
      << overall.get_avg() << ", "
      << overall.get_stdev() << ", "
      << overall.get_tx_min() << ", "
      << overall.get_tx_max() << ", "
      << overall.get_tx_avg() << ", "
      << overall.get_tx_stdev() << ", "
      << overall.get_penalty_evts() 
      << endl;

   attr_value_t ret = SIM_make_attr_string(ss.str().c_str()); 
   return ret;
}


static attr_value_t
get_epoch_time_series(void * v,
                      conf_object_t * o,
                      attr_value_t * idx) {
   
   int i;
   simple_staller_t * st = (simple_staller_t*) o;
   vector<stdatapoint>::iterator ptsi;
   vector<stdatapoint>* pts = st->stats->get_samples();
   attr_value_t avlist = SIM_alloc_attr_list(pts->size());
   for(i=0, ptsi=pts->begin(); ptsi!=pts->end(); ptsi++, i++) {

      int j=0;
      stdatapoint* pdp = &(*ptsi);
      attr_value_t epochlist = SIM_alloc_attr_list(9);
      epochlist.u.list.vector[j++] = SIM_make_attr_floating(pdp->get_min());
      epochlist.u.list.vector[j++] = SIM_make_attr_floating(pdp->get_max());
      epochlist.u.list.vector[j++] = SIM_make_attr_floating(pdp->get_avg());
      epochlist.u.list.vector[j++] = SIM_make_attr_floating(pdp->get_stdev());
      epochlist.u.list.vector[j++] = SIM_make_attr_floating(pdp->get_tx_min());
      epochlist.u.list.vector[j++] = SIM_make_attr_floating(pdp->get_tx_max());
      epochlist.u.list.vector[j++] = SIM_make_attr_floating(pdp->get_tx_avg());
      epochlist.u.list.vector[j++] = SIM_make_attr_floating(pdp->get_tx_stdev());
      epochlist.u.list.vector[j++] = SIM_make_attr_floating((double)pdp->get_penalty_evts());

      avlist.u.list.vector[i] = epochlist;
   }
   return avlist;
}  

static set_error_t
set_epoch_time_series(void *dont_care, 
                      conf_object_t *obj,
                      attr_value_t *val, 
                      attr_value_t *idx) {
   simple_staller_t *st = (simple_staller_t *) obj;
   st->stats->get_samples()->clear();
   return Sim_Set_Ok;
}

static attr_value_t
get_bw_time_series(void * v,
                   conf_object_t * o,
                   attr_value_t * idx) {
   
   int i;
   simple_staller_t * st = (simple_staller_t*) o;
   vector<double>::iterator ptsi;
   vector<double>* pts = st->stats->get_ts_p();
   attr_value_t avlist = SIM_alloc_attr_list(pts->size());
   for(i=0, ptsi=pts->begin(); ptsi!=pts->end(); ptsi++, i++) 
      avlist.u.list.vector[i] = SIM_make_attr_floating(*ptsi);
   return avlist;
}  

static set_error_t
set_bw_time_series(void *dont_care, 
                   conf_object_t *obj,
                   attr_value_t *val, 
                   attr_value_t *idx) {
   simple_staller_t *st = (simple_staller_t *) obj;
   st->stats->get_ts_p()->clear();
   return Sim_Set_Ok;
}

static attr_value_t
get_tx_bw_time_series(void * v,
                      conf_object_t * o,
                      attr_value_t * idx) {
   
   int i;
   simple_staller_t * st = (simple_staller_t*) o;
   vector<double>::iterator ptsi;
   vector<double>* pts = st->stats->get_tx_ts_p();
   attr_value_t avlist = SIM_alloc_attr_list(pts->size());
   for(i=0, ptsi=pts->begin(); ptsi!=pts->end(); ptsi++, i++) 
      avlist.u.list.vector[i] = SIM_make_attr_floating(*ptsi);
   return avlist;
}  

static set_error_t
set_tx_bw_time_series(void *dont_care, 
                      conf_object_t *obj,
                      attr_value_t *val, 
                      attr_value_t *idx) {
   simple_staller_t *st = (simple_staller_t *) obj;
   st->stats->get_tx_ts_p()->clear();
   return Sim_Set_Ok;
}

static set_error_t
set_frontbus_bandwidth_probe(void *dont_care, 
                             conf_object_t *obj,
                             attr_value_t *val, 
                             attr_value_t *idx) {
   // is a setter meaningful for this attr?
   simple_staller_t *st = (simple_staller_t *) obj;
   st->frontbus_bandwidth_probe = val->u.integer;  
   return Sim_Set_Ok;
}

static attr_value_t                                             
get_frontbus_bandwidth_probe(void *dont_care, 
                             conf_object_t *obj,       
                             attr_value_t *idx) {                                                               

   simple_staller_t *st = (simple_staller_t *) obj;          
   attr_value_t ret;                                       
   cycles_t frontbus_current_probe_cycle;
   conf_object_t *cpu; 

   cpu = SIM_current_processor();
   frontbus_current_probe_cycle = SIM_cycle_count(cpu);
   if(frontbus_current_probe_cycle == st->frontbus_previous_probe_cycle) {
      st->frontbus_bandwidth_probe = -1;
   } else {
      cycles_t delta = (frontbus_current_probe_cycle - st->frontbus_previous_probe_cycle);
      double fbp = (double) st->memops_probe_counter / delta;
      st->frontbus_bandwidth_probe = fbp;
      st->frontbus_previous_probe_cycle = frontbus_current_probe_cycle;
      st->memops_probe_counter = 0;   
   }

   ret.kind = Sim_Val_Floating;
   ret.u.floating = st->frontbus_bandwidth_probe;
   return ret;
}

static set_error_t
set_frontbus_bandwidth_tx_probe(void *dont_care, 
                                conf_object_t *obj,
                                attr_value_t *val, 
                                attr_value_t *idx) {
   simple_staller_t *st = (simple_staller_t *) obj;
   st->frontbus_bandwidth_tx_probe = val->u.integer;
   return Sim_Set_Ok;
}

static attr_value_t                                             
get_frontbus_bandwidth_tx_probe(void *dont_care, 
                                conf_object_t *obj,       
                                attr_value_t *idx) {                                                               

   simple_staller_t *st = (simple_staller_t *) obj;          
   attr_value_t ret;                                       
   cycles_t frontbus_current_probe_cycle;
   conf_object_t *cpu; 
    
   cpu = SIM_current_processor();
   frontbus_current_probe_cycle = SIM_cycle_count(cpu);

   if(frontbus_current_probe_cycle == st->frontbus_previous_tx_probe_cycle) 
      st->frontbus_bandwidth_tx_probe = -1;
   else {
      cycles_t delta = (frontbus_current_probe_cycle - st->frontbus_previous_tx_probe_cycle);
      double fbp = (double) st->memops_tx_probe_counter/delta;
      st->frontbus_bandwidth_tx_probe = fbp;
      st->frontbus_previous_tx_probe_cycle = frontbus_current_probe_cycle;
      st->memops_tx_probe_counter = 0;
   }   

   ret.kind = Sim_Val_Floating;
   ret.u.floating = st->frontbus_bandwidth_tx_probe;
   return ret;
}

static void
st_register(conf_class_t *st_class) {

   STALLER_REGISTER_ATTR(frontbus_bandwidth_limit_bpc, "FrontBus BW in bits per CPU cycle");
   STALLER_REGISTER_ATTR(frontbus_busy_penalty_factor, "Penalty factor in case of exceeding the FB BW");
   STALLER_REGISTER_ATTR(frontbus_check_interval, "Front Bus BW check interval");
   STALLER_REGISTER_ATTR(frontbus_stat_interval, "Front Bus probe interval extracting BW time series");
   STALLER_REGISTER_ATTR(frontbus_stat_size, "size of BW time series for statistical calculation");
   STALLER_REGISTER_ATTR(stall_time, "Stall time returned when accessed");
   STALLER_REGISTER_ATTR(cache_perturb, "Range of cycles for random increments to miss time.");
   STALLER_REGISTER_ATTR(seed, "Seed for pseudo-random number generator.");
   STALLER_REGISTER_ATTR(stress_test, "stress test mode--perturbs timings by wild intervals");
   STALLER_REGISTER_ATTR(stress_range, "specifies the range over which stress test timings are taken");
   STALLER_REGISTER_ATTR(past_bios,"Signals that we are through the bios and can begin perturbing the timings.");
   STALLER_REGISTER_ATTR(verbose, "controls how much junk we spit to cout");
   STALLER_REGISTER_STAT_ATTR(min, "current epoch min bandwidth consumption");
   STALLER_REGISTER_STAT_ATTR(max, "current max bandwidth consumption");
   STALLER_REGISTER_STAT_ATTR(avg, "current average bw consumption");
   STALLER_REGISTER_STAT_ATTR(stdev, "current stdev bw consumption");
   STALLER_REGISTER_STAT_ATTR(tx_min, "current min bandwidth consumption (transactional memops)");
   STALLER_REGISTER_STAT_ATTR(tx_max, "current max bandwidth consumption (transactional memops)");
   STALLER_REGISTER_STAT_ATTR(tx_avg, "current average bw consumption (transactional memops)");
   STALLER_REGISTER_STAT_ATTR(tx_stdev, "current stdev bw consumption(transactional memops)");
   STALLER_REGISTER_ISTAT_ATTR(penalty_evts, "number of times staller increased penalty to throttle bw");

   SIM_register_typed_attribute(st_class, "bw_time_series",
                                get_bw_time_series, 0,
                                set_bw_time_series, 0,
                                Sim_Attr_Optional,
                                "[f*]", NULL,
                                "bw sample values for most recent epoch");

   SIM_register_typed_attribute(st_class, "tx_bw_time_series",
                                get_tx_bw_time_series, 0,
                                set_tx_bw_time_series, 0,
                                Sim_Attr_Optional,
                                "[f*]", NULL,
                                "tx bw sample values for most recent epoch");

   SIM_register_typed_attribute(st_class, "epoch_time_series",
                                get_epoch_time_series, 0,
                                set_epoch_time_series, 0,
                                Sim_Attr_Optional,
                                "[[f*]*]", NULL,
                                "time series of epoch sample values");

   SIM_register_typed_attribute(st_class, "statistics",
                                get_st_stats, 0,
                                set_st_stats, 0,
                                Sim_Attr_Optional,
                                "s", NULL,
                                "getter returns stats, setter resets");

   SIM_register_typed_attribute(st_class, "frontbus_bandwidth_probe",
                                get_frontbus_bandwidth_probe, 0,
                                set_frontbus_bandwidth_probe, 0,
                                Sim_Attr_Optional,
                                "f", NULL,
                                "Front Bus BW value");
 
   SIM_register_typed_attribute(st_class, "frontbus_bandwidth_tx_probe",
                                get_frontbus_bandwidth_tx_probe, 0,
                                set_frontbus_bandwidth_tx_probe, 0,
                                Sim_Attr_Optional,
                                "f", NULL,
                                "Front Bus BW value for Tx memops");
}

void st_assert_fail(const char *assertion, 
                    const char *file, 
                    unsigned int line) {
   stringstream ss;
   ss << "STALLER ASSERT FAILURE : " 
      << assertion << " " 
      << file << ":" 
      << line 
      << endl;
   SIM_break_simulation(ss.str().c_str());
}

/*
  Information for class registering
*/
static conf_class_t *st_class;
static class_data_t st_data;
static timing_model_interface_t st_ifc;

DLL_EXPORT void
init_local(void)
{
   st_data.new_instance = st_new_instance;
   st_data.parent = SIM_get_class("log-object");
   st_data.description =
      "This class implements a simple transaction staller. If a"
      " transactions is stallable, it will return a fixed stall"
      " time. It can be used for a simple memory simulation at the"
      " end of a cache hierarchy.";
                
   if (!(st_class = SIM_register_class("trans-staller-plus",
                                       &st_data))) {
      pr_err("Could not create trans-staller-plus class\n");
      return;
   }

   /* register the attributes */
   st_register(st_class);

   /* register the timing model interface */
   st_ifc.operate = st_operate;
   SIM_register_interface(st_class, "timing-model",
                          (void *) &st_ifc);
}
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
