// MetaTM Project
// File Name: staller.h
//
// Description: this class implements a simple
// bandwidth measuring/limiting class. 
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2008. All Rights Reserved.
// See LICENSE file for license terms.

#ifndef _TX_STALLER_H_
#define _TX_STALLER_H_

#include <simics/api.h>
#include <simics/alloc.h>
#include <simics/utils.h>
#include "../common/simulator.h"
#include "../common/osacommon.h"
#include "ststats.h"
#include "stconfig.h"
#include "stdatapoint.h"

#define STALLER_REGISTER_ATTR(attr, desc)                 \
   SIM_register_typed_attribute(st_class, ""#attr,	       \
                                get_st_##attr, 0,	       \
                                set_st_##attr, 0,         \
                                Sim_Attr_Optional,        \
                                "i", NULL,                \
                                desc)                           

#define STALLER_REGISTER_S_ATTR(attr, desc)               \
   SIM_register_typed_attribute(st_class, ""#attr,	       \
                                get_st_##attr, 0,	       \
                                set_st_##attr, 0,	       \
                                Sim_Attr_Optional,	       \
                                "s", NULL,                \
                                desc)                           

#define STALLER_REGISTER_VO_ATTR(attr, desc)              \
   SIM_register_typed_attribute(st_class, #attr,          \
                                get_st_##attr, 0,	       \
                                set_st_##attr, 0,	       \
                                Sim_Attr_Optional,	       \
                                "[o*]", NULL,		       \
                                desc)                           

#define STALLER_REGISTER_O_ATTR(attr, desc)               \
   SIM_register_typed_attribute(st_class, #attr,          \
                                get_st_##attr, 0,	       \
                                set_st_##attr, 0,	       \
                                Sim_Attr_Optional,	       \
                                "o", NULL,                \
                                desc)                           

#define STALLER_REGISTER_STAT_ATTR(attr, desc)                \
   SIM_register_typed_attribute(st_class, "stat_"#attr,		  \
                                get_st_stat_##attr, 0,		  \
                                set_st_stat_##attr, 0,		  \
                                Sim_Attr_Optional,            \
                                "f", NULL,                    \
                                desc)                           

#define STALLER_REGISTER_ISTAT_ATTR(attr, desc)               \
   SIM_register_typed_attribute(st_class, "stat_"#attr,		  \
                                get_st_stat_##attr, 0,		  \
                                set_st_stat_##attr, 0,		  \
                                Sim_Attr_Optional,            \
                                "i", NULL,                    \
                                desc)                           

#define STALLER_STATS_ATTR(attr)                               \
   static set_error_t                                          \
   set_st_stat_##attr(void *dont_care, conf_object_t *obj,		\
                      attr_value_t *val, attr_value_t *idx) {  \
      if(val->kind != Sim_Val_Floating){                       \
         return Sim_Set_Need_Floating;                         \
      }                                                        \
      st_t *st = (st_t *) obj;                                 \
      st->stats->set_##attr(val->u.floating);                  \
      return Sim_Set_Ok;                                       \
   }                                                           \
   static attr_value_t                                         \
   get_st_stat_##attr(void *dont_care, conf_object_t *obj,		\
                      attr_value_t *idx){                      \
      st_t *st = (st_t *) obj;                                 \
      return SIM_make_attr_floating(st->stats->get_##attr());  \
   }                                      

#define STALLER_STATS_INT_ATTR(attr)                           \
   static set_error_t                                          \
   set_st_stat_##attr(void *dont_care, conf_object_t *obj,		\
                      attr_value_t *val, attr_value_t *idx) {  \
      if(val->kind != Sim_Val_Integer){                        \
         return Sim_Set_Need_Integer;                          \
      }                                                        \
      st_t *st = (st_t *) obj;                                 \
      st->stats->set_##attr(val->u.integer);                   \
      return Sim_Set_Ok;                                       \
   }                                                           \
   static attr_value_t                                         \
   get_st_stat_##attr(void *dont_care, conf_object_t *obj,		\
                      attr_value_t *idx){                      \
      st_t *st = (st_t *) obj;                                 \
      return SIM_make_attr_integer(st->stats->get_##attr());   \
   }                                      

#define STALLER_ATTR(attr)                                  \
   static set_error_t                                       \
   set_st_##attr(void *dont_care, conf_object_t *obj,			\
                 attr_value_t *val, attr_value_t *idx) {    \
      if(val->kind != Sim_Val_Integer){                     \
         return Sim_Set_Need_Integer;                       \
      }                                                     \
      st_t *st = (st_t *) obj;                              \
      st->attr = val->u.integer;                            \
      return Sim_Set_Ok;                                    \
   }                                                        \
   attr_value_t                                             \
   get_st_##attr(void *arg, conf_object_t *obj,             \
                 attr_value_t *pAttrIdx);                   \
   attr_value_t                                             \
   get_st_##attr(void *arg, conf_object_t *obj,             \
                 attr_value_t *pAttrIdx){                   \
      st_t *st = (st_t *) obj;                                 \
      attr_value_t avReturn = SIM_make_attr_integer(st->attr);	\
      return avReturn;                                         \
   }

#define STALLER_CONFIG_ATTR(attr)                                       \
   static set_error_t                                                   \
   set_st_##attr(void *dont_care, conf_object_t *obj,                   \
                 attr_value_t *val, attr_value_t *idx) {                \
      if(val->kind != Sim_Val_Integer){                                 \
         return Sim_Set_Need_Integer;                                   \
      }                                                                 \
      st_t *st = (st_t *) obj;                                          \
      st->config->set_##attr(val->u.integer);                           \
      return Sim_Set_Ok;                                                \
   }                                                                    \
   attr_value_t                                                         \
   get_st_##attr(void *arg, conf_object_t *obj,                         \
                 attr_value_t *pAttrIdx);                               \
   attr_value_t                                                         \
   get_st_##attr(void *arg, conf_object_t *obj,                         \
                 attr_value_t *pAttrIdx){                               \
      st_t *st = (st_t *) obj;                                          \
      attr_value_t avReturn = SIM_make_attr_integer(st->config->get_##attr()); \
      return avReturn;                                                  \
   }

extern "C" {
  void st_assert_fail(const char *assertion, const char *file, unsigned int line);
  static attr_value_t                                             
  get_frontbus_bandwidth_probe(void *, conf_object_t *,       
			       attr_value_t *);       
  
  static attr_value_t                                             
  get_frontbus_bandwidth_tx_probe(void *, conf_object_t *,       
				  attr_value_t *);       
  
}
#define STALLER_ASSERT(expr)	    \
  (static_cast<void> ((expr) ? 0 :				      \
		      (st_assert_fail (__STRING(expr),		      \
				       __FILE__, __LINE__ ), 0)))

typedef struct simple_staller {
   log_object_t log;
   int past_bios;
   int memops_counter;
   cycles_t staller_previous_cycle;
   int penaltycounter;
   int memops_probe_counter;
   int memops_tx_probe_counter;
   cycles_t frontbus_previous_probe_cycle;
   cycles_t frontbus_previous_tx_probe_cycle;
   double frontbus_bandwidth_probe; 
   double frontbus_bandwidth_tx_probe;
   int logcounter;
   int verbose;
   int random_stress;
   int stress_range;
   stconfig oconfig;
   ststats  ostats;
   stconfig * config;
   ststats *  stats;
} simple_staller_t;
typedef simple_staller_t st_t;

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
