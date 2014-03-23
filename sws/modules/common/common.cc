// MetaTM Project
// File Name: common.cc
//
// Description: 
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006, 2007. All Rights Reserved.
// See LICENSE file for license terms.


/*** Be sure to include STL first, otherwise simics gets cranky */
#include <tr1/unordered_map>
#include <map>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <deque>
#include <string>

#include "simulator.h"
#include "memaccess.h"
#include "osamagic.h"
#include "osacommon.h"
#include "common.h"
#include "profile.h"

#define OSA_PRINT_TO_CONSOLE	true  /*true*/
bool gbOsaPrintCout	= OSA_PRINT_TO_CONSOLE;

char log_file[256];
FILE *fp;


#include "stdio.h"
#include <sys/time.h>
#include <time.h>
#include "osacache.h"
#include "os.h"

using namespace std;
using namespace std::tr1;

/* Active memory watchpoints for catching STM isolation violations */
typedef struct watchpoint{
   osa_integer_t addr;
   osa_integer_t len;
   unsigned int pid;
   breakpoint_id_t bp;
} watchpoint_t;

map<osa_integer_t, watchpoint_t> bps;
int suspend_protect[OSA_MAX_CPUS];

inline int cache_count(osamod_t *osamod) {
   return osamod->minfo->getNumCpus();
}

/**** Begin: Code to manage idle cycles between end of benchmarks and killing simulation ***/

static osa_attr_set_t set_idle_cycles(SIMULATOR_SET_INTEGER_ATTRIBUTE_SIGNATURE){
   osamod_t *osamod = (osamod_t*)obj;
   common_data_t *common = osamod->common;
   common->idle_cycles = INTEGER_ARGUMENT;
   return ATTR_OK;
}

static integer_attribute_t get_idle_cycles(SIMULATOR_GET_ATTRIBUTE_SIGNATURE){
   osamod_t *osamod = (osamod_t*)obj;
   common_data_t *common = osamod->common;
   return INT_ATTRIFY(common->idle_cycles);
}


/**** End: Code to manage idle cycles between end of benchmarks and killing simulation ***/

/**** Begin: Code to manage benchmarks ***/
static osa_attr_set_t set_benchmarks(SIMULATOR_SET_LIST_ATTRIBUTE_SIGNATURE){
   osamod_t *osamod = (osamod_t*)obj;
   common_data_t *common = osamod->common;

   delete common->benchmarks;
   common->benchmarks = new deque<char*>;

#ifdef _USE_SIMICS
   if(pAttrValue->kind == Sim_Val_List)
#endif
   {
      for(int i = 0; i < LIST_ARGUMENT_SIZE; i++){
         common->benchmarks->push_back(MM_STRDUP(STRING_ATTR(LIST_ARGUMENT(i))));
      }
   }
#ifdef _USE_SIMICS
   else {
      return Sim_Set_Need_List;
   }
#endif
   return ATTR_OK;
}


static list_attribute_t get_benchmarks(SIMULATOR_GET_ATTRIBUTE_SIGNATURE){

   osamod_t *osamod = (osamod_t*)obj;
   common_data_t *common = osamod->common;

   list_attribute_t avReturn = osa_sim_allocate_list(common->benchmarks->size());
   for(unsigned int i = 0; i < common->benchmarks->size(); i++){
      LIST_ATTR(avReturn, i) = STRING_ATTRIFY((*common->benchmarks)[i]);
   }
   return avReturn;
} 

static int osa_network_query(osamod_t *osamod){
   common_data_t *common = osamod->common;
   for(unsigned int i = 0; i < common->benchmarks->size(); i++){
      if(( 0 == strcmp((*common->benchmarks)[i], "specweb_cli"))
         || (0 == strcmp((*common->benchmarks)[i], "specweb_cli_primary"))
         || (0 == strcmp((*common->benchmarks)[i], "server"))
         || (0 == strcmp((*common->benchmarks)[i], "apt"))
         || (0 == strcmp((*common->benchmarks)[i], "scp"))
         || (0 == strcmp((*common->benchmarks)[i], "scp_nplus1"))
         || (0 == strcmp((*common->benchmarks)[i], "nc"))
         || (0 == strcmp((*common->benchmarks)[i], "nc_2n"))
         || (0 == strcmp((*common->benchmarks)[i], "nc_udp"))
         || (0 == strcmp((*common->benchmarks)[i], "nc_udp_2n"))
         || (0 == strcmp((*common->benchmarks)[i], "nc_server"))
         || (0 == strcmp((*common->benchmarks)[i], "nc_udp_server"))
         )
         return 1;
   }
   return 0;
}


/**** End: Code to manage benchmarks ***/

/**** Begin: Code to manage benchmark hacks ***/
static osa_attr_set_t set_ide(SIMULATOR_SET_LIST_ATTRIBUTE_SIGNATURE) {
   osamod_t *osamod = (osamod_t*)obj;
   common_data_t *common = osamod->common;

   common->ide_count = LIST_ARGUMENT_SIZE;
   if(common->ide != NULL){
      MM_FREE(common->ide);
   }
   common->ide = MM_MALLOC( common->ide_count, system_component_object_t*);
   for(int i = 0; i < common->ide_count; i++){
      common->ide[i] = GENERIC_ATTR(LIST_ARGUMENT(i));
   }
   return ATTR_OK;
}

static list_attribute_t get_ide(SIMULATOR_GET_ATTRIBUTE_SIGNATURE) {
   osamod_t *osamod = (osamod_t*)obj;
   common_data_t *common = osamod->common;

   list_attribute_t avreturn = osa_sim_allocate_list(common->ide_count);
   for(int i = 0; i < common->ide_count; i++){
     LIST_ATTR(avreturn, i) = GENERIC_ATTRIFY(common->ide[i]);
   }
   return avreturn;
}

static osa_attr_set_t set_preload(SIMULATOR_SET_INTEGER_ATTRIBUTE_SIGNATURE){

   osamod_t *osamod = (osamod_t*)obj;
   common_data_t *common = osamod->common;
   common->preload = INTEGER_ARGUMENT;
   return ATTR_OK;
}

static integer_attribute_t get_preload( SIMULATOR_GET_ATTRIBUTE_SIGNATURE ){

   osamod_t *osamod = (osamod_t*)obj;
   common_data_t *common = osamod->common;
   return INT_ATTRIFY(common->preload);
}

/**** End: Code to manage benchmark hacks ***/

/**** Begin: Code to manage grub hackery ***/

static osa_attr_set_t set_grub_option( SIMULATOR_SET_INTEGER_ATTRIBUTE_SIGNATURE ){
   osamod_t *osamod = (osamod_t*)obj;
   common_data_t *common = osamod->common;
   common->grub_option = INTEGER_ARGUMENT;
   return ATTR_OK;
}

static integer_attribute_t get_grub_option( SIMULATOR_GET_ATTRIBUTE_SIGNATURE ){
   osamod_t *osamod = (osamod_t*)obj;
   common_data_t *common = osamod->common;
   return INT_ATTRIFY(common->grub_option);
}

/**** End: Code to manage grub hackery ***/

/**** Begin: Code for osamod compatibility ***/

static void clear_stats(osamod_t *osamod){
   clear_profile(osamod);
}

static void output_stats(osamod_t *osamod){
   dump_profile(osamod, NULL);
}


/**** End: Code for osamod compatibility ***/

// This function cheats the interface a bit, as it accumulates
// modules, rather than replacine one
static osa_attr_set_t set_next_module( SIMULATOR_SET_GENERIC_ATTRIBUTE_SIGNATURE ){

   osamod_t *osamod = (osamod_t *) GENERIC_ARGUMENT;
#if defined(_USE_SIMICS)
   OSA_assert(pAttrValue->kind == Sim_Val_Object, (osamod_t *)obj);
#endif
   OSA_add_mod(osamod);

   // if the module is osatxm, set up the profiler to use it
   // for memory reads
   if(osamod->type == OSATXM) {
      osamod_t *common_mod = (osamod_t*)obj;
      osatxm_interface_t *iface =
         (osatxm_interface_t*)osa_sim_get_interface((conf_object_t*)osamod,
               "osatxm_interface");
      common_mod->common->prof.memread = iface->read_4bytes;
      common_mod->common->prof.mem_reader = osamod;
   }

   return ATTR_OK;
}

/**** End: Code for other modules to register with us ***/

/**** Begin: Code for cache latency squashing ***/
static integer_attribute_t get_fast_caches( SIMULATOR_GET_ATTRIBUTE_SIGNATURE ) {
   osamod_t *osamod = (osamod_t*)obj;
   return INT_ATTRIFY(osamod->common->fast_caches);
}

static osa_attr_set_t set_fast_caches( SIMULATOR_SET_INTEGER_ATTRIBUTE_SIGNATURE ) {
   osamod_t *osamod = (osamod_t*)obj;
   osamod->common->fast_caches = INTEGER_ARGUMENT;
   return ATTR_OK;
}
/**** End: Code for cache latency squashing ***/

/**** Begin: Code for adding a breakpoint on schedule ***/
static boolean_attribute_t get_break_on_sched( SIMULATOR_GET_ATTRIBUTE_SIGNATURE ) {
   osamod_t *osamod = (osamod_t*)obj;
   return BOOL_ATTRIFY(osamod->common->break_on_sched);
}

static osa_attr_set_t set_break_on_sched( SIMULATOR_SET_BOOLEAN_ATTRIBUTE_SIGNATURE ) {
   osamod_t *osamod = (osamod_t*)obj;
   osamod->common->break_on_sched = BOOLEAN_ARGUMENT;
   return ATTR_OK;
}
/**** End: Code for adding a breakpoint on schedule ***/

/**** Begin: Code for random simulator kills ***/
static void osa_random_kill(conf_object_t *cpu, void *_osamod) {
   osamod_t *osamod = (osamod_t*)_osamod;
   if(osamod->common->kill_safe_level) {
      osamod->common->kill_imminent = true;
   }
   else {
      osa_break_simulation("RANDOM_KILL_INVOKED");
   }
}
/**** End: Code for random simulator kills ***/

/**** Begin: Macros for magic_callback dispatching ***/

void _dispatch_all(osamod_t *osamod, unsigned long f_offset, bool all_flag) {
   for(osamod_t *cur_mod = OSA_mod_list();
       cur_mod != NULL; cur_mod = cur_mod->next_mod){
      if(all_flag                                     
         || sameMachine(osamod, cur_mod)){ 

         osa_sim_clear_error();

         common_osamod_interface_t *iface =           
            (common_osamod_interface_t *) osa_sim_get_interface((system_component_object_t *)cur_mod,
                                                            "common_osamod_interface");
         if( SIM_get_pending_exception() ) {
            *cur_mod->pStatStream << "XXX: Can't find common interface \"" << SIM_last_error() << "\"" << endl;
            SIM_clear_exception();
         } else {
            magic_callback_func *f =
               (magic_callback_func*)((char*)iface + f_offset);
            (*f)(cur_mod);
         }
      }
   }
}

#define DISPATCH_ALL(osamod, func, all_flag)  \
         _dispatch_all(osamod, \
               (unsigned long)&((common_osamod_interface_t*)0x0)->func, \
               all_flag);
#if 0
         for(osamod_t *cur_mod = OSA_mod_list();            \
             cur_mod != NULL; cur_mod = cur_mod->next_mod){ \
                                                            \
            if(all_flag                                     \
               || sameMachine(osamod, cur_mod)){            \
                                                            \
               common_osamod_interface_t *iface =           \
                  (common_osamod_interface_t *) osa_sim_get_interface((system_component_object_t *)cur_mod, \
                                                                  "common_osamod_interface"); \
               if( osa_sim_clear_error() ) {                \
                  *cur_mod->pStatStream << "XXX: Can't find common interface" << endl; \
               } else {                                     \
                  iface->func(cur_mod);                     \
               }                                            \
            }                                               \
         }
#endif


// don't muck with other machines
#define DISPATCH_OS_VISIBILITY(osamod, func)  \
         for(osamod_t *cur_mod = OSA_mod_list();                      \
             cur_mod != NULL; cur_mod = cur_mod->next_mod){           \
                                                                      \
            if(!sameMachine(osamod, cur_mod))                         \
               continue;                                              \
                                                                      \
            func(cur_mod);                                            \
         }          

// Only syncchar cares about this magic instruction
#define DISPATCH_SYNCCHAR(osamod, func) \
         for(osamod_t *cur_mod = OSA_mod_list();                      \
             cur_mod != NULL; cur_mod = cur_mod->next_mod){           \
                                                                      \
            if((!sameMachine(osamod, cur_mod))                        \
               || cur_mod->type != SYNCCHAR){                         \
               continue;                                              \
            }                                                         \
                                                                      \
            common_syncchar_interface_t *iface =                      \
               (common_syncchar_interface_t *) osa_sim_get_interface((system_component_object_t *)cur_mod, \
                                                                 "common_syncchar_interface");\
            if( osa_sim_clear_error() ) {                             \
               *cur_mod->pStatStream << "XXX: Can't find common syncchar interface" << endl; \
            } else {                                                  \
               iface->func(cur_mod);                                  \
            }                                                         \
         }


// Only osatxm cares about this magic instruction
#define DISPATCH_OSATXM(osamod, func) \
         for(osamod_t *cur_mod = OSA_mod_list();                      \
             cur_mod != NULL; cur_mod = cur_mod->next_mod){           \
                                                                      \
            if((!sameMachine(osamod, cur_mod))                        \
               || cur_mod->type != OSATXM){                           \
               continue;                                              \
            }                                                         \
                                                                      \
            common_osatxm_interface_t *iface =                        \
               (common_osatxm_interface_t *) osa_sim_get_interface((conf_object_t *)cur_mod, \
                                                               "common_osatxm_interface");\
            if( osa_sim_clear_error() ) {                             \
               *cur_mod->pStatStream << "XXX: Can't find common osatxm interface" << endl; \
            } else {                                                  \
               iface->func(cur_mod);                                  \
            }                                                         \
         }

// Only osatxm cares about this magic instruction
#define DISPATCH_OSATXM2(osamod, func, codeVal) \
         for(osamod_t *cur_mod = OSA_mod_list();                      \
             cur_mod != NULL; cur_mod = cur_mod->next_mod){           \
                                                                      \
            if((!sameMachine(osamod, cur_mod))                        \
               || cur_mod->type != OSATXM){                           \
               continue;                                              \
            }                                                         \
                                                                      \
            common_osatxm_interface_t *iface =                        \
               (common_osatxm_interface_t *) osa_sim_get_interface((conf_object_t *)cur_mod, \
                                                               "common_osatxm_interface");\
            if( osa_sim_clear_error() ) {                             \
               *cur_mod->pStatStream << "XXX: Can't find common osatxm interface" << endl; \
            } else {                                                  \
               iface->func(codeVal, cur_mod);                         \
            }                                                         \
         }

/**** End: Macros for magic_callback dispatching ***/

static void magic_callback(lang_void *callback_data, 
                           conf_object_t *trigger_obj) {

   osa_uinteger_t codeVal;
   osamod_t *osamod = (osamod_t*)callback_data;
   common_data_t *common = osamod->common;

   fp = fopen(log_file,"a");   

   osa_cpu_object_t *cpu = OSA_get_sim_cpu();
   int cpunum = osamod->minfo->getCpuNum(cpu);
   codeVal = _osa_read_register(cpu, regESI);

   // do we want the results of this instruction to happen on
   // all machines?
   int all_flag = 0;

   switch(codeVal) {
   case OSA_PRINT_STR_VAL_ALL:
      all_flag = 1;
   case OSA_PRINT_STR_VAL: 
      {
         string printStr =  osa_get_name_val(OSA_get_sim_cpu());
         if( gbOsaPrintCout ){
            cout << printStr << endl;
	    fprintf(fp,"%s\n",printStr.c_str());
		}
         // Iterate over all of the modules for our machine (or all if
         // the all_flag is set), and put this string in their logs
         for(osamod_t *cur_mod = OSA_mod_list();
             cur_mod != NULL; cur_mod = cur_mod->next_mod){
            // Don't print on modules without an output stream
            if (cur_mod->pStatStream == NULL
                || cur_mod->type == COMMON) {
               continue;
            }

            if(all_flag
               || sameMachine(osamod, cur_mod)){
               *cur_mod->pStatStream << printStr << endl;
	    }
         }

         break;
      }
   case OSA_CLEAR_STAT_ALL:
      all_flag = 1;
   case OSA_CLEAR_STAT:
      DISPATCH_ALL(osamod, clear_stat, all_flag);
      break;
   case OSA_OUTPUT_STAT_ALL:
      all_flag = 1;
   case OSA_OUTPUT_STAT: 
      DISPATCH_ALL(osamod, output_stat, all_flag);
      break;

   case OSA_KILLSIM:
      set_idle_callback(osamod);
      break;
   case OSA_BREAKSIM:
      osa_break_simulation("OSA Magic Breakpoint Requested.");
      break;
   case OSA_BENCHMARK:
      {
         // Actual code to give simlulator the benchmark name
         osa_uinteger_t addr =  osa_read_register(cpu, regEBX);
         osa_uinteger_t limit = osa_read_register(cpu, regECX);
         if(!common->benchmarks->empty()){
            char * bench = common->benchmarks->front();
            write_string(cpu, addr, bench, limit, true);
            common->benchmarks->pop_front();
            MM_FREE(bench);
         }
         
         // Miscellaneous other hacks that occur when we finish boot
         system_component_object_t * sim = osa_get_object_by_name("sim");
         integer_attribute_t av = INT_ATTRIFY(osamod->cpu_switch_time);
         osa_sim_set_integer_attribute(sim, "cpu_switch_time", &av);

         if(osamod->disk_delay > 0){
            integer_attribute_t tmp = INT_ATTRIFY(1);
            float_attribute_t tmp1 = FLOAT_ATTRIFY(osamod->disk_delay);
            
            for(int i = 0; i < common->ide_count; i++){
               osa_sim_set_integer_attribute(common->ide[i], "model_dma_delay", &tmp);
               osa_sim_set_float_attribute(common->ide[i], "interrupt_delay", &tmp1);
            }
         }

         // Make sure syncchar knows we are past boot so that it can
         // start logging worksets if needed.  This is idempotent, so
         // it is ok to run around every benchmark
         DISPATCH_SYNCCHAR(osamod, osa_after_boot);

         if(osamod->common->fast_caches != 2)
            osamod->common->fast_caches = 0;

         break;
      }
   case OSA_NETWORK:
      {
         int addr = _osa_read_register(cpu, regEBX);
         osa_write_sim_4bytes(cpu, DATA_SEGMENT, addr, osa_network_query(osamod));
         break;
      }
   case OSA_GRUB: 
      {
         if(common->grub_option != -1){
            int addr = _osa_read_register(cpu, regEBX);
            osa_write_sim_4bytes(cpu, DATA_SEGMENT, addr, common->grub_option);
         }
         break;
      }
   case OSA_PRINT_NUM_CHAR: 
      {
         string printStr =  osa_get_num_char(OSA_get_sim_cpu());

         // Don't put newlines here - osa_get_num_char() already does.
         // Extra newlines play hell with sync_char_post
         
         if( gbOsaPrintCout ) {
            cout << printStr;
            cout.flush();
         }
         
         for(osamod_t *cur_mod = OSA_mod_list();
             cur_mod != NULL; cur_mod = cur_mod->next_mod){

            if(sameMachine(osamod, cur_mod)
               && cur_mod->pStatStream != NULL
               && cur_mod->type != COMMON){
               *cur_mod->pStatStream << printStr;
               (*cur_mod->pStatStream).flush();
            }
         }
         break;
      }

   case OSA_PRINT_STACK_TRACE: 
      OSA_stack_trace(OSA_get_sim_cpu(), osamod);
      break;

   case OSA_LOG_THREAD_START: 
      {
         // Iterate over all of the modules for our machine (or all if
         // the all_flag is set), and put this string in their logs
         for(osamod_t *cur_mod = OSA_mod_list();
             cur_mod != NULL; cur_mod = cur_mod->next_mod){
            
            // Don't print on modules without an output stream
            if (cur_mod->pStatStream == NULL
                || cur_mod->type == COMMON) {
               continue;
            }

            if(sameMachine(osamod, cur_mod)){
               *cur_mod->pStatStream << "Thread " << cur_mod->os->current_process[cpunum] 
                                     << " starting up at cycle " << osa_get_sim_cycle_count(cpu) << endl;
            }
         }

         break;
      }


   case OSA_LOG_THREAD_STOP: 
      {
         // Iterate over all of the modules for our machine (or all if
         // the all_flag is set), and put this string in their logs
         for(osamod_t *cur_mod = OSA_mod_list();
             cur_mod != NULL; cur_mod = cur_mod->next_mod){
            
            // Don't print on modules without an output stream
            if (cur_mod->pStatStream == NULL
                || cur_mod->type == COMMON) {
               continue;
            }

            if(sameMachine(osamod, cur_mod)){
               *cur_mod->pStatStream << "Thread " << cur_mod->os->current_process[cpunum] 
                                     << " stopping at cycle " << osa_get_sim_cycle_count(cpu) << endl;
            }
         }

         break;
      }

   case OSA_PRELOAD:{
      osa_uinteger_t addr =  osa_read_register(cpu, regEBX);
      osa_write_sim_4bytes(cpu, DATA_SEGMENT, addr, common->preload);
      break;
   }
   case OSA_TOGGLE_DISK_DELAY: {
      osa_uinteger_t toggle =  osa_read_register(cpu, regEBX);
      if(toggle == 0){
         integer_attribute_t tmp = INT_ATTRIFY(0);
         float_attribute_t tmp1 = FLOAT_ATTRIFY(0.0);
         
         for(int i = 0; i < common->ide_count; i++){
            osa_sim_set_integer_attribute(common->ide[i], "model_dma_delay", &tmp);
            osa_sim_set_float_attribute(common->ide[i], "interrupt_delay", &tmp1);
         }
         
      } else if(osamod->disk_delay > 0){
         integer_attribute_t tmp = INT_ATTRIFY(1);
         float_attribute_t tmp1 = FLOAT_ATTRIFY(osamod->disk_delay);
         
         for(int i = 0; i < common->ide_count; i++){
            osa_sim_set_integer_attribute(common->ide[i], "model_dma_delay", &tmp);
            osa_sim_set_float_attribute(common->ide[i], "interrupt_delay", &tmp1);
         }
      }
      
      break;
   }
      // os visibility is needed by each module.
      // Maybe we can still put it in common later...
   case OSA_SCHED: 
      {
         // Iterate over all of the modules for our machine (or all if
         // the all_flag is set), and put this string in their logs
         for(osamod_t *cur_mod = OSA_mod_list();
             cur_mod != NULL; cur_mod = cur_mod->next_mod){
            
            // don't muck with other machines
            if(!sameMachine(osamod, cur_mod))
               continue;

            if(cur_mod->type == COMMON && cur_mod->common->break_on_sched)
               osa_break_simulation("OSA Magic Breakpoint Requested on schedule.");

            // Syncchar needs to manage runqueue handoff when sched is executed
            if(cur_mod->type == SYNCCHAR){
               common_syncchar_interface_t *iface = 
                  (common_syncchar_interface_t *) osa_sim_get_interface((conf_object_t *)cur_mod,
                                                                    "common_syncchar_interface");
               if( osa_sim_clear_error() ) {
                  *cur_mod->pStatStream << "XXX: Can't find common syncchar interface" << endl;
               } else {
                  iface->osa_sched(cur_mod);
               }
            }

            // Everyone does os_sched
            os_sched(cur_mod);
         }
         break;
      }
   case OSA_FORK:
      DISPATCH_OSATXM(osamod, osa_fork);
      DISPATCH_SYNCCHAR(osamod, osa_fork);
      DISPATCH_OS_VISIBILITY(osamod, os_fork);
      break;
   case OSA_TIMER:
      DISPATCH_OS_VISIBILITY(osamod, os_timer);
      break;
   case OSA_KSTAT:
      DISPATCH_OS_VISIBILITY(osamod, os_kstat);
      break;
   case OSA_KSTAT_2_4:
      DISPATCH_OS_VISIBILITY(osamod, os_kstat_2_4);
      break;
   case OSA_TASK_STATE_VAL:
      DISPATCH_OS_VISIBILITY(osamod, os_task_state);
      break;
   case OSA_CUR_SYSCALL_VAL:
      DISPATCH_OS_VISIBILITY(osamod, os_cur_syscall);
      break;
   case OSA_ENTER_SCHED:
      DISPATCH_OS_VISIBILITY(osamod, os_enter_sched);
      break;
   case OSA_EXIT_SWITCH_TO:
      DISPATCH_OS_VISIBILITY(osamod, os_exit_switch_to);
      break;

      // Dispatch this to trans-staller as well, to turn on cache
      // perturbation once we are out of the bios
   case OSA_KERNEL_BOOT:
      // it is ok to just pick one to find the staller, since all
      // modules should be on the timing chain
      enable_cache_perturb((system_component_object_t *) osamod,
            osamod->minfo->getCpuNum(cpu));
      DISPATCH_OSATXM(osamod, osa_kernel_boot);
      break;

      // Syncchar only
   case OSA_REGISTER_SPINLOCK: 
      DISPATCH_SYNCCHAR(osamod, osa_register_spinlock);
      break;

   case OSA_EXIT_CODE:
      DISPATCH_OS_VISIBILITY(osamod, os_exit);
      DISPATCH_OSATXM(osamod, osa_exit_code);
      DISPATCH_SYNCCHAR(osamod, osa_exit);
      break;
      // osatxm-specific stuff
   case OSA_XSETPID:
      DISPATCH_OSATXM(osamod, osa_xsetpid);
      break;
   case OSA_NEW_PROC_VAL:
      DISPATCH_OSATXM(osamod, osa_new_proc_val);
      break;
   case OSA_PROC_PRIO:
      DISPATCH_OSATXM(osamod, osa_proc_prio);
      break;
   case OSA_ACTIVETX_DATA:
      DISPATCH_OSATXM(osamod, osa_activetx_data);
      break;
   case OSA_KILL_TX:
      DISPATCH_OSATXM(osamod, osa_kill_transaction);
      break;
   case OSA_SET_VCONF_ADDR_BUF:
      DISPATCH_OSATXM(osamod, osa_set_vconf_addr_buf);
      break;
   case OSA_TX_STATE:
      DISPATCH_OSATXM(osamod, osa_tx_state);
      break;
   case OSA_GET_CM_POLICY:
      DISPATCH_OSATXM(osamod, osa_get_cm_policy);
      break;
   case OSA_SET_CM_POLICY:
      DISPATCH_OSATXM(osamod, osa_set_cm_policy);
      break;
   case OSA_GET_BACKOFF_POLICY:
      DISPATCH_OSATXM(osamod, osa_get_backoff_policy);
      break;
   case OSA_SET_BACKOFF_POLICY:
      DISPATCH_OSATXM(osamod, osa_set_backoff_policy);
      break;
   case OSA_GET_BK_POLCHG_THRESH:
      DISPATCH_OSATXM(osamod, osa_get_bk_polchg_thresh);
      break;
   case OSA_GET_CM_POLCHG_THRESH:
      DISPATCH_OSATXM(osamod, osa_get_cm_polchg_thresh);
      break;
   case OSA_GET_RANDOM: {
      osa_uinteger_t res = (osa_uinteger_t) rand();
      osa_write_register(cpu, regEBX, res);            
      break;
   }
   case OSA_TRACK_EVENT_VAL:
      DISPATCH_OSATXM(osamod, osa_track_named_event);
      break;
   case OSA_BACK_TRACE_VAL:
      DISPATCH_OSATXM(osamod, osa_back_trace);
      break;
   case OSA_TX_TRACE_MODE_VAL:
      DISPATCH_OSATXM(osamod, osa_tx_trace_mode);
      break;
   case OSA_THREAD_PROF_DATA:
      DISPATCH_OSATXM(osamod, osa_thread_prof_data);
      break;
   case OSA_SINK_AREA_VAL:
      DISPATCH_OSATXM(osamod, osa_sink_area_val);
      break;
   case OSA_TXMIGRATION_MODE:
      DISPATCH_OSATXM(osamod, osa_txmigration_mode);
      break;
   case OSA_TX_SET_LOG_BASE:
      DISPATCH_OSATXM(osamod, osa_tx_set_log_base);
      break;
   case OSA_ACTIVETX_PARAM:
      DISPATCH_OSATXM(osamod, osa_activetx_param);
      break;
   case OSA_PAGE_FAULT:
      DISPATCH_OSATXM(osamod, osa_page_fault);
      break;
   case OSA_GET_MAX_CONFLICTER:
      DISPATCH_OSATXM(osamod, osa_get_max_conflicter);
      break;
   case OSA_CLEAR_MAX_CONFLICTER:
      DISPATCH_OSATXM(osamod, osa_clear_max_conflicter);
      break;
   case OSA_LOG_SCHEDULE:
      DISPATCH_OSATXM(osamod, osa_log_schedule);
      break;
   case OSA_CXSPIN_INFO_TX:
   case OSA_CXSPIN_INFO_NOTX:
   case OSA_CXSPIN_INFO_IOTX:
   case OSA_CXSPIN_INFO_IONOTX:
      DISPATCH_OSATXM2(osamod, osa_cxspin_info, codeVal);
      break;
   case OSA_CX_LOCK_ATTEMPT:
      DISPATCH_OSATXM(osamod, osa_cx_lock_attempt);
      break;
   case OSA_OP_XGETTXID:
   case OSA_OP_XBEGIN:
   case OSA_OP_XBEGIN_IO:
   case OSA_OP_XEND:
   case OSA_OP_XPUSH:
   case OSA_OP_XPOP:
   case OSA_OP_XRESTART:
   case OSA_OP_XRESTART_EX:
   case OSA_OP_XCAS:
   case OSA_OP_XSTATUS:
      DISPATCH_OSATXM2(osamod, osa_opcode, codeVal);
      break;

   case OSA_OP_XEND_USER:
      DISPATCH_OSATXM(osamod, osa_xend_user);
      break;
   case OSA_OP_XABORT_USER:
      DISPATCH_OSATXM(osamod, osa_xabort_user);
      break;
   case OSA_OP_XGETTXID_USER:
      DISPATCH_OSATXM(osamod, osa_xgettxid_user);
      break;
   case OSA_OP_SET_USER_SYSCALL_BIT:
      DISPATCH_OSATXM(osamod, osa_set_user_syscall_bit);
      break;
   case OSA_OP_GET_USER_SYSCALL_BIT:
      DISPATCH_OSATXM(osamod, osa_get_user_syscall_bit);
      break;

   case OSA_USER_OVERFLOW_BEGIN:
      DISPATCH_OSATXM(osamod, osa_user_overflow_begin);
      break;

   case OSA_USER_OVERFLOW_END:
      DISPATCH_OSATXM(osamod, osa_user_overflow_end);
      break;

   case OSA_DEBUG_PRINT: {
         string printStr =  osa_get_name_val(OSA_get_sim_cpu());
         cout << printStr << endl;
	break;
   }


      /* Allow the simulator to do some crude fine-grained memory
       * protection for debugging
       */
   case OSA_PROTECT_ADDR: {
      osa_uinteger_t addr = osa_read_register(cpu, regEBX);
      osa_uinteger_t length = osa_read_register(cpu, regECX);
      int cpunum = osamod->minfo->getCpuNum(cpu);
      /* Assume all reads proceed writes, for simplicity.  We already
       *catch inconsistent writes
       */
      breakpoint_id_t bp = SIM_breakpoint(osamod->common->context,
                                          Sim_Break_Virtual,
                                          Sim_Access_Read,
                                          addr, length,
                                          Sim_Breakpoint_Simulation);

      struct watchpoint wp;
      wp.addr = addr;
      wp.len = length;
      wp.pid = osamod->os->current_process[cpunum];
      wp.bp = bp;
      bps.insert(make_pair(addr, wp));
      break;
   }
   case OSA_UNPROTECT_ADDR: {
      osa_uinteger_t addr = osa_read_register(cpu, regEBX);
      map<osa_integer_t, watchpoint_t>::iterator iter =
         bps.find(addr);
      if(iter != bps.end()){
         SIM_delete_breakpoint(iter->second.bp);
         bps.erase(iter);
      } else {
         cout << "Can't find " << std::hex << addr << std::dec << endl;
         //osa_break_simulation("Boo");
      }
      break;
   }
   case OSA_PROTECT_SUSPEND: {
      int cpunum = osamod->minfo->getCpuNum(cpu);
      suspend_protect[cpunum] = 1;
      break;
   }
   case OSA_PROTECT_RESUME: {
      int cpunum = osamod->minfo->getCpuNum(cpu);
      suspend_protect[cpunum] = 0;
      break;
   }

   case OSA_LOG_SIGSEGV: {
      osa_uinteger_t addr = osa_read_register(cpu, regEBX);
      osa_uinteger_t eip = osa_read_register(cpu, regECX);
      
      cout << "XXX: Application SIGSEGV on address " 
           << std::hex << addr  << std::dec
           << ", eip is at " 
           << std::hex << eip << std::dec 
           << ", pid is " << osamod->os->current_process[cpunum]
           << endl;

      SIM_break_simulation("SIGSEGV");

      break;
   }

   case SYNCCHAR_CLEAR_MAP: 
      DISPATCH_SYNCCHAR(osamod, syncchar_clear_map);
      break;

   case SYNCCHAR_LOAD_MAP: 
      DISPATCH_SYNCCHAR(osamod, syncchar_load_map);
      break;

   case OSA_TXCACHE_TRACE: {
      DISPATCH_OSATXM(osamod, osa_txcache_trace_mode);
      break;
   }

   case OSA_RANDOM_SHUTDOWN_BEGIN:
   {
      /* Randomly shutdown between now and limit cycles from
       * now */
      osa_uinteger_t limit_low = osa_read_register(cpu, regEAX);
      osa_uinteger_t limit_hi = osa_read_register(cpu, regEDX);

      int64_t limit = (int64_t)(limit_low | (limit_hi << 32));

      if(limit == -1) {
         osamod->common->kill_begin_time = SIM_cycle_count(cpu);
      }
      else if(limit == -2) {
         osa_cycles_t elapsed = SIM_cycle_count(cpu) -
            osamod->common->kill_begin_time;
         *osamod->pStatStream << "RANDOM KILL MEASURED " << elapsed <<
            " CYCLES";
      }
      else {
         struct timeval tv;
         gettimeofday(&tv, NULL);
         srand48(tv.tv_usec);
         osa_uinteger_t kill_time = lrand48() % limit;
         *osamod->pStatStream << "RANDOM KILL IN " << kill_time << " CYCLES"
            << endl;
         SIM_time_post_cycle(cpu, kill_time, Sim_Sync_Processor,osa_random_kill,
               osamod);
      }
      break;
   }
   case OSA_RANDOM_SHUTDOWN_UNSAFE:
   {
      osamod->common->kill_safe_level++;
      break;
   }
   case OSA_RANDOM_SHUTDOWN_SAFE:
   {
      if(!(--osamod->common->kill_safe_level) && osamod->common->kill_imminent)
         osa_random_kill(NULL, osamod);
      break;
   }

      // Odd magic breakpoints in the BIOS boot (primarily code 18194)
   case 0x0:
   case 0xfd6c:
   case 0x7f0c:
   case 0x2118:
   case 0x3e10:
   case 0x4c:
   case 0x67fd4:
   case 0x67f5e:
   case 0x7c88:
   case 0x7ac8:
   case 0x7d85:
   case 0x7c05:
   case 0x7d0a:
   case 0x47ac8:
   case 0x67ac8:
   case 0x47f10:
   case 0x57f10:
   case 0x67fc8:
   case 0x67f56:
   case 0x2cfbc:
   case 0x27c82:
   case 0x7bf4:
   case 0x7e88:
   case 0x7a54:
   case 0x47a54:
   case 0x67a54:
   case 0x47e74:
   case 0x7a60:
   case 0x47a60:
   case 0x67a60:
   case 0x57e80:
   case 0x7e60:
   case 0x27c12:
   case 0x7b84:
   case 0x7e18:
   case 0x7e30:
   case 0x25b7e:
   case 0x5af0:
   case 0x5d84:
   case 0x5950:
   case 0x45950:
   case 0x65950:
   case 0x65d70:
   case 0x65da4:
   case 0x279a2:
   case 0x7914:
   case 0x7ba8:
   case 0x7774:
   case 0x47774:
   case 0x67774:
   case 0x47b94:
   case 0x47c1c:
   case 0x26ca6f:
   case 0x26005d:
   case 0x55da4:
      // Added in bootstrap of 2.6.22.6
   case 0x534d4712:
      //Singularity
   case 0x14712:  
   case 0x24712:
   case 0x34712:
   case 0x54712:
   
      break;

   default:
      pr("[common] odd magic breakpoint 0x%x\n", (int) codeVal);
   
   }

   fclose (fp);
}

/*
 * Breakpoint_callback - for filtering spurious breakpoints
 */

static void breakpoint_callback(lang_void* callback_data,
                                conf_object_t *trigger_obj,
                                osa_integer_t break_number,
                                osa_sim_inner_memop_t *memop) {
   osamod_t *osamod = (osamod_t *) callback_data;

   osa_sim_outer_memop_t* xmt = (osa_sim_outer_memop_t*) memop;
   unsigned int addr = (unsigned int)xmt->linear_address;

   osa_cpu_object_t *cpu = OSA_get_sim_cpu();
   int cpunum = osamod->minfo->getCpuNum(cpu);

   /*
   cout << "Checking out a bp on addr " << std::hex << addr << std::dec 
        << ", prot = " << suspend_protect[cpunum] << ", cpunum = " << cpunum << endl;
   */

   /* Don't break on memops in a page fault handler */
   if(suspend_protect[cpunum])
      return;

   if(bps.empty())
      osa_break_simulation("Where did this come from?");

   /* Find the watchpoint object*/
   for(map<osa_integer_t, watchpoint_t>::const_iterator it = bps.begin();
       it != bps.end(); it++){

      if(it->second.addr <= addr && addr <= it->second.addr + it->second.len){
         /* Make sure the current pc is right */
         if(osamod->os->current_process[cpunum] == it->second.pid){
            osa_break_simulation("Isolation violation");
            return;
         } else {
            return;
         }
      }
   }
}

/*
 * timing_operate()
 * return the number of cycles 
 * spent on the current memory transaction
 * forward to lower level caches where
 * appropriate.
 */


static osa_cycles_t timing_operate( TIMING_SIGNATURE ) {

   osa_cycles_t penalty = collect_cache_timing( TIMING_ARGUMENTS );

   osamod_t *osamod = (osamod_t*)pConfObject;
   if(osamod->common->fast_caches && penalty > 0) {
      return 1;
   }
   else {
      return penalty;
   }

}

static generic_attribute_t get_system_attribute( SIMULATOR_GET_ATTRIBUTE_SIGNATURE ) {
   return GENERIC_ATTRIFY(((osamod_t*)obj)->system);
}

static osa_attr_set_t set_system_attribute( SIMULATOR_SET_GENERIC_ATTRIBUTE_SIGNATURE ) {

   osamod_t *osamod = (osamod_t*)obj;

   conf_object_t *system = GENERIC_ARGUMENT;

   osamod->system = system;
   osamod->minfo = new MachineInfo(system);

   // init os visibility 
   init_procs(osamod);

   // We don't care about procCycles in common
   osamod->procCycles = NULL;

   // init idle_cycles 
   integer_attribute_t idle_cycles = INT_ATTRIFY(0);
   osa_sim_set_integer_attribute((conf_object_t *)osamod,
                     "idle_cycles",
                     &idle_cycles);

   integer_attribute_t cpu_switch_time = INT_ATTRIFY(5);
   osa_sim_set_integer_attribute((conf_object_t *)osamod,
                     "cpu_switch_time",
                     &cpu_switch_time);

   float_attribute_t disk_delay = FLOAT_ATTRIFY(0.0055);
   osa_sim_set_float_attribute((conf_object_t *)osamod,
                     "disk_delay",
                     &disk_delay);

   // Figure out the architecture (32 or 64 bit)
   osa_cpu_object_t *cpu = osamod->minfo->getCpu(0);
   attr_value_t architecture = SIM_get_attribute(cpu, "architecture");
   OSA_assert(architecture.kind == Sim_Val_String, osamod);
   if(0 == strcmp("x86-64", architecture.u.string)){
      cout << "Detected a 64-bit architecture.  Setting larger registers\n";
      ArchitectureWordSizeMask = 0xFFFFFFFFFFFFFFFFLLU;
      // These registers are actually different
      regES = 71;
      regCS = 75;
      regSS = 79;
      regDS = 83;
      regFS = 87;
      regGS = 91;
      // Just hack to make the main general purpose registers go to RAX, etc.  The actual register indices are the same.
      regEAX = 26;
      regEBX = 29;
      regECX = 27;
      regEDX = 28;
      regESP = 30;
      regEBP = 32;
      regESI = 34;
      regEDI = 36;
      regEIP = 70;
   }

   // set callback handlers for only the cpus that we own
   //OSA_TODO: What????? How do I handle this in QEMU: Not quite sure... I guess don't need to worry for single machine sims... In QEMU, do this for
   return ATTR_OK;
}

static attr_value_t get_context_attribute(void*, conf_object_t *osamod,
                                          attr_value_t *idx) {
   return SIM_make_attr_object( ((osamod_t*)osamod)->common->context );
}

static set_error_t set_context_attribute(void*, conf_object_t *osamod_obj,
                                         attr_value_t *val, attr_value_t *idx) {
   osamod_t *osamod = (osamod_t*)osamod_obj;

   osamod->common->context = val->u.object;

   // set the symtable of the context
   conf_object_t *st0 = osa_get_object_by_name("st0");
   attr_value_t attr = SIM_make_attr_object(st0);
   SIM_set_attribute(osamod->common->context, "symtable", &attr);

   // make sure all of the cpus that we own have
   // this context set
   attr_value_t context_attr = SIM_make_attr_object(osamod->common->context);
   int i;
   hap_handle_t hapHandle;
   osa_cpu_object_t *cpu;


   for(i = 0; i < osamod->minfo->getNumCpus(); i++){
      cpu = osamod->minfo->getCpu(i);
      SIM_set_attribute(cpu, "current_context",
                        &context_attr);

      suspend_protect[i] = 0;

      hapHandle = SIM_hap_add_callback_obj(
                                           "Core_Magic_Instruction",
                                           cpu, (hap_flags_t)0,
                                           (obj_hap_func_t)magic_callback,
                                           (void*)osamod );
   
   }

   hapHandle = SIM_hap_add_callback_obj(
                                        "Core_Breakpoint_Memop",
                                        osamod->common->context,
                                        (hap_flags_t)0,
                                        (obj_hap_func_t)breakpoint_callback,
                                        (void*)osamod);

   return Sim_Set_Ok;
}

static attr_value_t get_profile_log(void*, conf_object_t *osamod,
      attr_value_t *idx) {
   return SIM_make_attr_string(((osamod_t*)osamod)->common->prof.log_prefix);
}

static set_error_t set_profile_log(void*, conf_object_t *osamod_obj, 
      attr_value_t *val, attr_value_t *idx) {
   osamod_t *osamod = (osamod_t*)osamod_obj;
   osamod->common->prof.log_prefix = MM_STRDUP(val->u.string);
   return Sim_Set_Ok;
}

static attr_value_t get_profile_interval(void*, conf_object_t *osamod,
      attr_value_t *idx) {
   return SIM_make_attr_integer(((osamod_t*)osamod)->common->prof.interval);
}

static set_error_t set_profile_interval(void*, conf_object_t *osamod_obj,
      attr_value_t *val, attr_value_t *idx) {
   osamod_t *osamod = (osamod_t*)osamod_obj;
   osamod->common->prof.interval = val->u.integer;
   if(val->u.integer > 0 && !osamod->common->prof.scheduled) {
      profiler_schedule_tick(osamod, NULL);
   }
   return Sim_Set_Ok;
}

static set_error_t set_common_log(void*, conf_object_t *osamod_obj, 
      attr_value_t *val, attr_value_t *idx) {
   time_t tim = time(NULL);  
   strcpy(log_file, val->u.string);
   fp = fopen (log_file,"a");
   fprintf(fp, "###########################\nStarted simulation at %s\n\n", ctime(&tim));
   fclose(fp);
   return Sim_Set_Ok;
}

extern "C" {
   static system_component_object_t* new_common_instance(parse_object_t *parse_obj) {

      pr("[common] new instance\n");

      osamod_t *osamod = MM_ZALLOC(1, osamod_t);
#ifdef _USE_SIMICS
      SIM_log_constructor(&osamod->log, parse_obj);
		osamod = (osamod_t *) &osamod->log.obj;
#endif
      //The above works because obj is the first member of log is the first member of osamod
      OSA_add_mod(osamod);

      osamod->type = COMMON;
      osamod->timing_model = NULL;
      osamod->timing_ifc = NULL;
      osamod->system = NULL;

      osamod->common = new common_data_t;
      osamod->common->benchmarks     = new deque<char *>;
      osamod->common->ide            = NULL;
      osamod->common->grub_option    = -1;
      osamod->common->preload        = 0;
      osamod->common->fast_caches    = 0;
      osamod->common->break_on_sched = false;
      init_profiler(osamod);

      // Common errors go to stderr
      osamod->pStatStream = &std::cerr;
      
      if(osamod->pStatStream->good() == false) {
         std::cerr << "Error connecting to stderr\n";
      }

      osamod->common->kill_imminent = false;
      osamod->common->kill_safe_level = 0;
      
      return &osamod->log.obj; //wouldn't "return osamod" do the same?
   }

#ifdef _USE_SIMICS
   void init_local(void) {
      class_data_t classData;
      conf_class_t* pConfClass;
      timing_model_interface_t *pTimingInterface;
      common_osamod_interface_t *common_osamod_iface;
      
      // don't seed the random numbers with something
      // we can't repeat. cache seed already seed rand anyway.
      // ------------------
      // srand(time(NULL));

      // Ignore error message so creation of module on build succeeds.
      memset(&classData, 0, sizeof(classData));
      classData.new_instance = new_common_instance;
      classData.description = "OSA Simulation Tools";
    
      pConfClass = SIM_register_class("common", &classData);

      SIM_register_typed_attribute(
                                   pConfClass, "system",
                                   get_system_attribute, NULL,
                                   set_system_attribute, NULL,
                                   Sim_Attr_Required,
                                   "o", NULL,
                                   "The system to which this common instance belongs");


      // Cache timing stuff

      pTimingInterface = MM_ZALLOC(1, timing_model_interface_t);
      pTimingInterface->operate = timing_operate;
      SIM_register_interface(pConfClass, TIMING_MODEL_INTERFACE, pTimingInterface);

      // Interface from common to us
      common_osamod_iface = MM_ZALLOC(1, common_osamod_interface_t);
      common_osamod_iface->clear_stat  = clear_stats;
      common_osamod_iface->output_stat = output_stats;
      SIM_register_interface(pConfClass, "common_osamod_interface", common_osamod_iface);


      SIM_register_typed_attribute(
                                   pConfClass, "timing_model",
                                   get_timing_model, 0,
                                   set_timing_model, 0,
                                   Sim_Attr_Optional,
                                   "o|n", NULL,
                                   "Object listening on transactions coming from "
                                   "the syncchar module.");

      SIM_register_typed_attribute(
                                   pConfClass, "caches",
                                   get_caches, 0,
                                   set_caches, 0,
                                   Sim_Attr_Session,
                                   "[o*]", NULL,
                                   "per-cpu caches listening on the bus.");

      // Idle cycles between end of benchmarks and killing simulation
      SIM_register_typed_attribute(
                                   pConfClass, "idle_cycles",
                                   get_idle_cycles, 0,
                                   set_idle_cycles, 0,
                                   Sim_Attr_Optional,
                                   "i", NULL,
                                   "Number of cycles to wait before exiting at end of benchmarks.");
      
      // Benchmark tools
      SIM_register_typed_attribute(
                                   pConfClass, "benchmarks",
                                   get_benchmarks, 0,
                                   set_benchmarks, 0,
                                   Sim_Attr_Optional,
                                   "[s*]", NULL,
                                   "Benchmarks to instruct the simulation to run.");


      SIM_register_typed_attribute(
                                   pConfClass, "ide",
                                   get_ide, 0,
                                   set_ide, 0,
                                   Sim_Attr_Optional,
                                   "[o+]", NULL,
                                   "The ide bus to set disk delay changes");

      SIM_register_typed_attribute(
                                   pConfClass, "cpu_switch_time",
                                   get_cpu_switch_time, 0,
                                   set_cpu_switch_time, 0,
                                   Sim_Attr_Optional,
                                   "i", NULL,
                                   "Sets the cpu switch time on an OSA_BENCHMARK magic instruction.");

      SIM_register_typed_attribute(
                                   pConfClass, "disk_delay",
                                   get_disk_delay, 0,
                                   set_disk_delay, 0,
                                   Sim_Attr_Optional,
                                   "f", NULL,
                                   "Sets the disk delay on an OSA_BENCHMARK magic instruction.");


      SIM_register_typed_attribute(
                                   pConfClass, "preload",
                                   get_preload, NULL,
                                   set_preload, NULL,
                                   Sim_Attr_Optional,
                                   "i", NULL,
                                   "Preload benchmark dataset (if supported)");

      // OS Visibilty 

      SIM_register_typed_attribute(
                                   pConfClass, "os_visibility",
                                   get_os_visibility, 0,
                                   set_os_visibility, 0,
                                   Sim_Attr_Optional,
                                   "D", NULL,
                                   "Operating System State.");

      SIM_register_typed_attribute(
                                   pConfClass, "stat_interval",
                                   get_stat_interval, 0,
                                   set_stat_interval, 0,
                                   Sim_Attr_Optional,
                                   "i", NULL,
                                   "vmstat collection interval. 0 = never.");

      //optional to log the common output to file
      SIM_register_typed_attribute(
                                   pConfClass, "log_init",
                                   get_configuration, 0,
                                   set_common_log, 0,
                                   Sim_Attr_Session,
                                   "s", NULL,
                                   "Writes the simulation config to the log.  Setting this value renames it.");


      // Grub hackery
      SIM_register_typed_attribute(
                                   pConfClass, "grub_option",
                                   get_grub_option, NULL,
                                   set_grub_option, NULL,
                                   Sim_Attr_Optional,
                                   "i", NULL,
                                   "Specify a GRUB selection");

      // Other modules register with us
      SIM_register_typed_attribute(
                                   pConfClass, "modules",
                                   get_next_module, NULL,
                                   set_next_module, NULL,
                                   Sim_Attr_Optional,
                                   "o", NULL,
                                   "Register other modules with common");

      SIM_register_typed_attribute(
                                   pConfClass, "alloc_hist",
                                   get_alloc_hist, 0,
                                   set_alloc_hist, 0,
                                   Sim_Attr_Optional,
                                   "n", NULL,
                                   "Read attribute to dump allocation history");

      SIM_register_typed_attribute(
                                   pConfClass, "fast_caches",
                                   get_fast_caches, 0,
                                   set_fast_caches, 0,
                                   Sim_Attr_Optional,
                                   "i", NULL,
                                   "Squash cache latencies during boot.");

      SIM_register_typed_attribute(
                                   pConfClass, "break_on_sched",
                                   get_break_on_sched, 0,
                                   set_break_on_sched, 0,
                                   Sim_Attr_Optional,
                                   "b", NULL,
                                   "Break on a call to schedule (for debugging).");

      SIM_register_typed_attribute(
                                   pConfClass, "context",
                                   get_context_attribute, NULL,
                                   set_context_attribute, NULL,
                                   Sim_Attr_Required,
                                   "o", NULL,
                                   "A context private to this object");

      SIM_register_typed_attribute(
                                   pConfClass, "profile_log_prefix",
                                   get_profile_log, NULL,
                                   set_profile_log, NULL,
                                   Sim_Attr_Optional,
                                   "s", NULL,
                                   "Filename for ip profile output");

      SIM_register_typed_attribute(
                                   pConfClass, "profile_interval",
                                   get_profile_interval, NULL,
                                   set_profile_interval, NULL,
                                   Sim_Attr_Optional,
                                   "i", NULL,
                                   "Interval in cycles to log IP");

   }

#endif

} // extern "C"

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
