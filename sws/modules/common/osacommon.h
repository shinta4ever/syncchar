// MetaTM Project
// File Name: osacommon.h
//
// Description: 
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006, 2007. All Rights Reserved.
// See LICENSE file for license terms.

#ifndef OSACOMMON_H
#define OSACOMMON_H

#include "writehist.h"
#include "MachineInfo.h"
#include "osaassert.h"

using namespace std;

#define OSA_MAX_CPUS       32

#include "simulator.h"

#define OSATXM_DEFAULT_TRACE	0
// #define traceTx 0

#define OSATXM 1
#define SYNCCHAR 2
#define SYNCCOUNT 3
#define COMMON 4
// #define OSA_WHY_DOESNT_USERMODE_WORK
typedef struct conflictRegisterInterface {
   int (*registerConflict)(system_component_object_t *obj, int myTxID, int enemyTxID);
} conflict_register_interface_t;
typedef struct conflict_result_ifc_t {
   int (*get_cm_res)(system_component_object_t *obj, void* memop);
} conflict_result_ifc;
typedef struct txIDInterface {
   int (*getTxID)(system_component_object_t *obj, unsigned int cpuid);
} tx_id_interface_t;
typedef struct abortCommitInterface {
   osa_cycles_t (*abortTx)(system_component_object_t *obj, int xID);
   osa_cycles_t (*commitTx)(system_component_object_t *obj, int xID);
   osa_cycles_t (*earlyRelease)(system_component_object_t *obj, 
                                osa_physical_address_t paddr,
                                osa_logical_address_t laddr,
                                int xID);
} abort_commit_interface_t;
typedef struct conflictStore {
   int myTxID;
   int enemyTxID;
   int predicted_result;
} conflict_store_t;
typedef struct overflow_interface_t {
   void (*signal_overflow)(conf_object_t *obj, int xid, int elbowroom);
} overflow_interface; 
typedef struct _ctxtsw_hist_t {
   osa_cycles_t       last_ctxtsw_start;
   osa_cycles_t       tot_ctxtsw;
   osa_cycles_t       tot_ctxtsw_cyc;
} ctxtsw_hist;

typedef struct _osamod_t {
#ifdef _USE_SIMICS
   log_object_t   log;
#endif
   int 		      value;
   int            condor_flag;
   int			   tx_trace;
   int            use_txcache;
   bool           past_bios;
   ctxtsw_hist    ctxtsws[OSA_MAX_CPUS];
   ostream*       pStatStream;
#ifdef OSA_WHY_DOESNT_USERMODE_WORK
   stringstream *back_trace;
   osa_integer_t prev_exception[OSA_MAX_CPUS][2];
#endif
   int            cpus_count;
   int            cpu_switch_time;
   double         disk_delay;

	/* the machine to which this module instance belongs */
	system_object_t *system;
   MachineInfo *minfo;

   /* timing model chaining */
   system_component_object_t *timing_model;
   timing_model_interface_t *timing_ifc;
    
   /* 
    * timing model constellation:
    * osatxm is an omniscient device--
    * all memory transactions go to it, 
    * but it needs to be able to 
    * redistribute transactions to the proper
    * lower level cache in order to collect
    * timing data. The caches attribute is
    * is this list of caches (assumed to be per-CPU)
    */
   system_component_object_t ** ppCaches;
   timing_model_interface_t **ppCachesIfc;
   int nCaches;
   int overflowBit;
   int tOperateCount;
   vector <conflict_store_t> pConflictStore;
   abort_commit_interface_t *abort_commit_ifc;

   /* Used by the sync_* modules */
   osa_cycles_t *procCycles;

   /* other devices listening on the bus - snoop chain */
   system_component_object_t **snoopers;
   interface_t **snoopers_ifc;
   int snoopers_size;

	/* the type of the module */
	int type;

   /* os visibility data */
   struct _os_data_t *os;


	/*  storage for module data - new home for crap previously
    *  freeloading on the global address space
    */
	struct _osatxm_data_t *osatxm;
	struct _syncchar_data_t *syncchar;
	struct _synccount_data_t *synccount;
	struct _common_data_t *common;

   /* Keep a list of all present osamod structs to do global
    * statistics and printing
    */
   struct _osamod_t *next_mod;

} osamod_t;

int sameMachine(osamod_t *a, osamod_t *b);

typedef unsigned int (*read_sim_4bytes_func)(osamod_t *osamod,
                                             osa_cpu_object_t *cpu, 
                                             osa_segment_t segment,
                                             osa_logical_address_t laddr);
typedef unsigned int (*get_current_transaction_func)(osamod_t *osamod,
                                                     osa_cpu_object_t *cpu);

typedef struct osatxm_interface {
   read_sim_4bytes_func read_4bytes;
   get_current_transaction_func get_current_transaction;
} osatxm_interface_t;


/* Interface from common to osamods */

typedef void (*magic_callback_func) (osamod_t *osamod);
typedef void (*magic_callback_func2) (osa_uinteger_t codVal, osamod_t *osamod);

typedef struct common_osamod_interface {
   magic_callback_func clear_stat;
   magic_callback_func output_stat;
} common_osamod_interface_t;

/* Interface from common to syncchar */

typedef struct common_syncchar_interface {
   magic_callback_func osa_sched;
   magic_callback_func osa_register_spinlock;
   magic_callback_func osa_after_boot;
   magic_callback_func osa_fork;
   magic_callback_func osa_exit;
   magic_callback_func syncchar_clear_map;
   magic_callback_func syncchar_load_map;
} common_syncchar_interface_t;

/* Interface from common to osatxm */

typedef struct common_osatxm_interface {
   magic_callback_func osa_fork;
   magic_callback_func osa_exit_code;
   magic_callback_func osa_xsetpid;
   magic_callback_func osa_new_proc_val;
   magic_callback_func osa_proc_prio;
   magic_callback_func osa_kernel_boot;
   magic_callback_func osa_activetx_data;
   magic_callback_func osa_kill_transaction;
   magic_callback_func osa_tx_state;
   magic_callback_func osa_set_vconf_addr_buf;
   magic_callback_func osa_thread_prof_data;
   magic_callback_func osa_sink_area_val;
   magic_callback_func osa_txmigration_mode;
   magic_callback_func osa_tx_set_log_base;
   magic_callback_func osa_activetx_param;
   magic_callback_func osa_get_cm_policy;
   magic_callback_func osa_set_cm_policy;
   magic_callback_func osa_get_backoff_policy;
   magic_callback_func osa_set_backoff_policy;
   magic_callback_func osa_get_bk_polchg_thresh;
   magic_callback_func osa_get_cm_polchg_thresh;
   magic_callback_func osa_get_usetxlog;
   magic_callback_func osa_set_usetxlog;
   magic_callback_func osa_page_fault;
   magic_callback_func osa_get_max_conflicter;
   magic_callback_func osa_clear_max_conflicter;
   magic_callback_func osa_log_schedule;
   magic_callback_func2 osa_cxspin_info;
   magic_callback_func osa_cx_lock_attempt;
   magic_callback_func osa_track_named_event;
   magic_callback_func osa_back_trace;
   magic_callback_func osa_tx_trace_mode;
   magic_callback_func2 osa_opcode;
   magic_callback_func osa_txcache_trace_mode;
   magic_callback_func osa_xend_user;
   magic_callback_func osa_xabort_user;
   magic_callback_func osa_xgettxid_user;
   magic_callback_func osa_set_user_syscall_bit;
   magic_callback_func osa_get_user_syscall_bit;
   magic_callback_func osa_user_overflow_begin;
   magic_callback_func osa_user_overflow_end;

} common_osatxm_interface_t;


/* register a list of all osamod objects */
osamod_t *OSA_mod_list();
void OSA_add_mod(osamod_t *);

osa_attr_set_t
set_tx_trace( SIMULATOR_SET_INTEGER_ATTRIBUTE_SIGNATURE );

integer_attribute_t 
get_tx_trace( SIMULATOR_GET_ATTRIBUTE_SIGNATURE );

osa_attr_set_t
set_log( SIMULATOR_SET_STRING_ATTRIBUTE_SIGNATURE );

string_attribute_t
get_configuration( SIMULATOR_GET_ATTRIBUTE_SIGNATURE );

osa_attr_set_t 
set_proc_cycles( SIMULATOR_SET_LIST_ATTRIBUTE_SIGNATURE );

list_attribute_t
get_proc_cycles( SIMULATOR_GET_ATTRIBUTE_SIGNATURE );


void OSA_stack_trace_to_stringstream(osa_cpu_object_t *cpu, stringstream *sstream);
/**
 * trace_enabled
 * return true if global tracing is enabled.
 */
bool trace_enabled( void );

osa_attr_set_t 
set_cpu_switch_time( SIMULATOR_SET_INTEGER_ATTRIBUTE_SIGNATURE );

integer_attribute_t 
get_cpu_switch_time( SIMULATOR_GET_ATTRIBUTE_SIGNATURE );

osa_attr_set_t set_disk_delay( SIMULATOR_SET_FLOAT_ATTRIBUTE_SIGNATURE );

float_attribute_t get_disk_delay( SIMULATOR_GET_ATTRIBUTE_SIGNATURE );

osa_attr_set_t set_alloc_hist(SIMULATOR_SET_INTEGER_ATTRIBUTE_SIGNATURE); 

integer_attribute_t get_alloc_hist( SIMULATOR_GET_ATTRIBUTE_SIGNATURE );

// In a world where contention management policy can involve 
// stalling the current transaction in the event of a conflict,
// contention decisions can include more than just win lose
// results. "Arbitration Decision = ruling" enum:
// For some reason I felt moved to avoid int + macros.
// ===================================================
typedef enum _cmruling_t {
   cm_no_conflict = 2176,
   cm_selftx_wins = cm_no_conflict+1,
   cm_enemytx_wins = cm_no_conflict+2,
   cm_selftx_stall = cm_no_conflict+3,
   cm_enemytx_stall = cm_no_conflict+4,
   cm_ctm_resolved = cm_no_conflict+5,
   cm_xwait_stall = cm_no_conflict+6,
   cm_xwait_xcas_retry = cm_no_conflict+7,
   cm_xcas_fail = cm_no_conflict+8,
   cm_forward_restart = cm_no_conflict+9
} cm_ruling_t;   // cm = contention managment.
typedef cm_ruling_t cm_ruling;
#define successful_memop(x) \
  (x == cm_no_conflict || \
   x == cm_selftx_wins || \
   x == cm_ctm_resolved || \
   x == cm_forward_restart) 
#define conflicting_memop(w,x) \
  (x == cm_selftx_wins || \
   x == cm_enemytx_wins || \
   x == cm_selftx_stall || \
   x == cm_enemytx_stall || \
   x == cm_xwait_stall || \
   x == cm_forward_restart)



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
