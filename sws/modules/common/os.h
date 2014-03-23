// MetaTM Project
// File Name: os.h
//
// Description: 
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006, 2007. All Rights Reserved.
// See LICENSE file for license terms.

#ifndef OS_H
#define OS_H

using namespace std;
#include "simulator.h"
#include "osacommon.h"

typedef unsigned int spid_t;

struct pid_info{
   int pid;
   spid_t spid;
   bool kernel;
   int cpu;
   int state;
   int syscall;
   char cmd[256];
};

typedef unsigned long long cputime64_t;

struct PeriodicData
{
   public:
PeriodicData()
	{
      user = 0;
      nice = 0;
      system = 0;
      softirq = 0;
      irq = 0;
      idle = 0;
      iowait = 0;
      steal = 0;
	}
   cputime64_t user;
   cputime64_t nice; 
   cputime64_t system;
   cputime64_t softirq;
   cputime64_t irq; 
   cputime64_t idle;
   cputime64_t iowait;
   cputime64_t steal;
};

typedef struct _os_data_t {
   int last_pid;
   int timer_count;
   unsigned int spid_max;

   /* Data for user/kernel/idle stats */
   vector<unsigned int> kstats;
   vector<PeriodicData *> snapshot;

   int stat_interval;

   map<int, struct pid_info*> procs;
   map<spid_t, struct pid_info*> sprocs;
   map<int, spid_t> current_process;

   /* kstat plugin */
   void (*kstat_plugin)(lang_void *callback_data, 
                        system_component_object_t *trigger_obj);

    /* kstat for 2.4 kernel */
    physical_address_t kstat_2_4_paddr;
    int kstat_2_4_nrcpus;

    int kernel_version;
} os_data_t;

void init_procs(osamod_t *osamod);
void os_sched(osamod_t *osamod);
void os_enter_sched(osamod_t *osamod);
void os_exit_switch_to(osamod_t *osamod);
void os_timer(osamod_t *osamod);
void os_fork(osamod_t *osamod);
void os_exit(osamod_t *osamod);
void kstat_callback(lang_void *callback_data, system_component_object_t *trigger_obj);
void set_kstat_callback(osamod_t *obj, int new_int);
void os_kstat(osamod_t *osa_obj);
void os_kstat_2_4(osamod_t *osa_obj);
void os_task_state(osamod_t *osa_obj);
void os_cur_syscall(osamod_t *osa_obj);
osa_attr_set_t set_stat_interval( SIMULATOR_SET_INTEGER_ATTRIBUTE_SIGNATURE );
integer_attribute_t get_stat_interval( SIMULATOR_GET_ATTRIBUTE_SIGNATURE );
osa_attr_set_t set_kstat_addrs(os_data_t *os, list_attribute_t *pAttrValue);
list_attribute_t get_kstat_addrs(os_data_t *os);

set_error_t set_os_visibility(void *arg, conf_object_t *obj,
                              attr_value_t *pAttrValue, attr_value_t *pAttrIdx);

attr_value_t get_os_visibility( void *arg, conf_object_t *obj,
                                attr_value_t *pAttrIdx);

void prepare_kstats_snapshot(osamod_t *osamod);
string take_kstats_snapshot(osamod_t *osamod);

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
