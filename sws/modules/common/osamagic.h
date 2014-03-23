// MetaTM Project
// File Name: osamagic.h
//
// Description: 
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006, 2007. All Rights Reserved.
// See LICENSE file for license terms.

#ifndef __OSAMAGIC_H
#define __OSAMAGIC_H

#include "osacommon.h"
#include "simulator.h"

#define OSA_ILLEGAL            0
#define OSA_PRINT_STR_VAL      1
#define OSA_CLEAR_STAT         2
#define OSA_OUTPUT_STAT        3
#define OSA_KILLSIM            4
#define OSA_BREAKSIM           5
#define OSA_BENCHMARK          6
#define OSA_NETWORK            7
#define OSA_GRUB               8
#define OSA_PRINT_NUM_CHAR     9 /* Print X characters */
#define OSA_PRELOAD           10
#define OSA_TOGGLE_DISK_DELAY 11
#define OSA_CLEAR_STAT_ALL    12 // like their namesakes, but across all
#define OSA_OUTPUT_STAT_ALL   13 // machines in multi-machine 
#define OSA_PRINT_STR_VAL_ALL 14
#define OSA_DEBUG_PRINT       15 //just prints in the local output file
#define SYNCCHAR_CLEAR_MAP    16
#define SYNCCHAR_LOAD_MAP     17
#define OSA_LOG_THREAD_START  18
#define OSA_LOG_THREAD_STOP   19
#define OSA_PRINT_STACK_TRACE 20


#define OSA_TRACK_EVENT_VAL   81
#define OSA_BACK_TRACE_VAL    82
#define OSA_TX_TRACE_MODE_VAL 83

// jump ahead a bit for the os visibility stuff
#define OSA_SCHED             100
#define OSA_FORK              101
#define OSA_TIMER             102
#define OSA_KSTAT             103
#define OSA_EXIT_CODE         104
#define OSA_REGISTER_SPINLOCK 105
#define OSA_ENTER_SCHED       106
#define OSA_EXIT_SWITCH_TO    107
#define OSA_KSTAT_2_4         108
#define OSA_TASK_STATE_VAL    109
#define OSA_CUR_SYSCALL_VAL   110
#define OSA_PROTECT_ADDR      111
#define OSA_UNPROTECT_ADDR    112
#define OSA_PROTECT_SUSPEND   113
#define OSA_PROTECT_RESUME    114
#define OSA_LOG_SIGSEGV       115


//Added for usermode transactions
#define OSA_XSETPID        200
#define OSA_NEW_PROC_VAL   201
#define OSA_SINK_AREA_VAL  202

// process priority visibility
#define OSA_PROC_PRIO      300   // capture OS process prio

// page fault event types.
// The OS will let us know every time a page fault
// occurs, and will include a type specifier in EBX. 
// A major fault is a page fault that causes scheduler activity.
// A migrate fault is a major fault that causes process migration
#define OSA_PAGE_FAULT           301  
#define OSA_PFTYP_PAGE_FAULT     987
#define OSA_PFTYP_MAJOR_FAULT    988
#define OSA_PFTYP_MIGRATE_FAULT  989

// make transactional state (abstractly)
// visible to the OS so the scheduler can 
// manipulate dynamic priority accordingly
// moreover, make the bonus/penalty structure
// parameterizable in osatxm, so that 
// we can more easily do sensitivity studies
// on the parameter effects. 
#define OSA_ACTIVETX_DATA        302
#define OSA_ACTIVETX_PARAM       303
#define OSA_ACTTX_EXISTS         543
#define OSA_ACTTX_RESTARTS       544
#define OSA_ACTTX_SIZE           545
#define OSA_ACTTX_OVERFLOWED     546
#define OSA_ACTTX_COMPLEXCONF    547
#define OSA_ACTTX_LONGRUNNING    548
#define OSA_ACTTX_STALLED        549    
#define OSA_PINNED_CACHECOLD_TXPID 550
#define OSA_ACTTX_DESCHEDTHRESH  551
#define OSA_ACTTX_RESTARTAVG     552
#define OSA_ACTTX_CONFLICTSET    553    

#define OSA_ACTTX_HASCTMDEPS     610
#define OSA_ACTTX_ACTTXXPUSHED   611 
#define OSA_ACTTX_NOCTMDEPS      612
#define OSA_ACTTX_SUPPRESSINTR   613

// get a free prng number! avoids
// implementing prng in the simulated
// code.
#define OSA_GET_RANDOM           304

// allow the kernel to query and
// modify the contention manager 
// and backoff policy, as well as
// thresholds at which to make 
// policy change decisions. 
#define OSA_GET_CM_POLICY        305
#define OSA_SET_CM_POLICY        306
#define OSA_GET_CM_POLCHG_THRESH 307
#define OSA_GET_BACKOFF_POLICY   308
#define OSA_SET_BACKOFF_POLICY   309
#define OSA_GET_BK_POLCHG_THRESH 310


// thread profiling. Very similar to active tx
// but maintains state across tx's for a thread
// until a context switch (or access to profile data)
// also allow the kernel to collect decision parameter
// thresholds through osatxm, so that we can study 
// the effects without building a bezillion different kernels 
#define OSA_THREAD_PROF_DATA          600
#define OSA_THREAD_PROF_RESTARTS      601
#define OSA_THREAD_PROF_SIZE          602 
#define OSA_THREAD_PROF_OVERFLOWED    603 
#define OSA_THREAD_PROF_COMPLEXCONF   604 
#define OSA_THREAD_PROF_LONGRUNNING   605 
#define OSA_THREAD_PROF_UNIQUE_TX     606
#define OSA_THREAD_PROF_CURRENT_RESTARTS 607
#define OSA_THREAD_PROF_BKCYC_TX      608
#define OSA_SCHEDULER_MODE            609
#define OSA_GET_MAX_CONFLICTER        650
#define OSA_CLEAR_MAX_CONFLICTER      651
#define OSA_LOG_SCHEDULE              670
#define OSA_LOG_ANTI_CONFLICT         671
#define OSA_LOG_RESTART_DESCHED       672
#define OSA_GET_SCHED_PARAMETER       677
#define OSA_TX_STATE                  678
#define OSA_KILL_TX                   679
#define OSA_SET_VCONF_ADDR_BUF        680
#define OSA_SCHEDMODE_LINUXDEFAULT    0
#define OSA_SCHEDMODE_CLASSIC         1
#define OSA_SCHEDMODE_BADASS          2
#define OSA_SCHEDMODE_STUPID          3

#define OSA_TXMIGRATION_MODE      700

#define OSA_TX_SET_LOG_BASE       800

#define OSA_KERNEL_BOOT          1100

#define OSA_CXSPIN_INFO_TX          1201
#define OSA_CXSPIN_INFO_NOTX        1202
#define OSA_CXSPIN_INFO_IOTX        1203
#define OSA_CXSPIN_INFO_IONOTX      1204
#define OSA_CX_LOCK_ATTEMPT         1205

#define OSA_OP_XGETTXID             1206
#define OSA_OP_XBEGIN               1207
#define OSA_OP_XBEGIN_IO            1208
#define OSA_OP_XEND                 1209
#define OSA_OP_XRESTART             1210
#define OSA_OP_XRESTART_EX          1211
#define OSA_OP_XCAS                 1212
#define OSA_OP_XPUSH                1213
#define OSA_OP_XPOP                 1214
#define OSA_OP_XSTATUS              1215
#define OSA_OP_XEND_USER            1216
#define OSA_OP_XABORT_USER          1217
#define OSA_OP_XGETTXID_USER        1218
#define OSA_OP_SET_USER_SYSCALL_BIT 1219
#define OSA_OP_GET_USER_SYSCALL_BIT 1220

#define OSA_TXCACHE_TRACE           1300

#define OSA_USER_OVERFLOW_BEGIN     1400
#define OSA_USER_OVERFLOW_END       1401

#define OSA_RANDOM_SHUTDOWN_BEGIN   1501
#define OSA_RANDOM_SHUTDOWN_UNSAFE  1502
#define OSA_RANDOM_SHUTDOWN_SAFE    1503

#endif // __OSAMAGIC_H

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
