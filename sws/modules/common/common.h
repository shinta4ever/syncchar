#ifndef __COMMON_H
#define __COMMON_H

#include "profile.h"

typedef struct _common_data_t {
   system_component_object_t **ide;
   int ide_count;

   /* Grub option */
   int grub_option;

   /* Preload benchmark data if supported by benchmark */
   int preload;

   /* squash cache latencies */
   int fast_caches;

   /* Break on schedule */
   bool break_on_sched;

   // context on which we set our breaks
   conf_object_t *context;

   // profiling information
   struct _ip_profiler prof;

   osa_cycles_t       idle_cycles;
   deque<char*> *benchmarks;

   // random kill data
   bool kill_imminent;
   int kill_safe_level;
   osa_cycles_t kill_begin_time;
} common_data_t;

#ifdef _USE_SIMICS
void idle_callback(lang_void *callback_data,
                   conf_object_t *trigger_obj);
void set_idle_callback(osamod_t *obj);
attr_value_t get_next_module( void *arg, conf_object_t *obj,
                              attr_value_t *pAttrIdx);
#elif defined(_USE_QEMU)
#error In common.h - define idle_callback etc
#endif

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
