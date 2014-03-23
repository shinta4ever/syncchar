// MetaTM Project
// File Name: osacommon.cc
//
// Description: 
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006, 2007. All Rights Reserved.
// See LICENSE file for license terms.

#include "osacommon.h"
#include "osacache.h"
#include "memaccess.h"

static osamod_t *head_mod = NULL;
osamod_t *OSA_mod_list() {
   return head_mod;
}
void OSA_add_mod(osamod_t *osamod) {
   osamod->next_mod = head_mod;
   head_mod = osamod;
}


bool OSA_condor(osamod_t *osamod)
{
   return osamod->condor_flag;
}

void OSA_break_simulation(const char *msg, osamod_t *osamod)
{
   if (!osamod->condor_flag)
      {
         osa_break_simulation(msg);
         OSA_stack_trace(OSA_get_sim_cpu(), osamod);
      }
   else
      {
         if (osamod->pStatStream != NULL)
            {
               *osamod->pStatStream <<
                  "BREAKPOINT ENCOUNTERED, TERMINATING CONDOR RUN " << endl;
               *osamod->pStatStream << "BP MESSAGE: " << msg << endl;
               OSA_stack_trace(OSA_get_sim_cpu(), osamod);
            }
         osa_quit(0); // Should return error code to signify BP, if condor won't freak out
      }
}

void OSA_stack_trace(osa_cpu_object_t *cpu, osamod_t *osamod) {
#ifdef _USE_SIMICS
   conf_object_t *st0 = osa_get_object_by_name("st0");
   symtable_interface_t *st_iface = 
      (symtable_interface_t*)SIM_get_interface(st0, "symtable");
   attr_value_t stack_trace = st_iface->stack_trace(cpu, 10);
   int i;
   int cpunum = osamod->minfo->getCpuNum(cpu);

   *osamod->pStatStream << "CPU " << cpunum << ", Stack trace:" << endl;
   for(i = 0; i < stack_trace.u.list.size; i++) {
      *osamod->pStatStream << "  " << hex <<
         stack_trace.u.list.vector[i].u.list.vector[0].u.integer
         << dec << endl;
   }
#elif defined(_USE_QEMU)
#error Define OSA_stack_trace contents for QEMU!
#else
#error Which simulator are you using?
#endif

   /*
   attr_value_t registerNums = SIM_get_all_registers(cpu);
    
   *osamod->pStatStream << "Register values:" << endl;
   for (i = 0 ; i < registerNums.u.list.size ; i++) {
		osa_integer_t regIndex = registerNums.u.list.vector[i].u.integer;
		osa_uinteger_t regValue = osa_read_register(cpu, regIndex);
      *osamod->pStatStream << "  " << regIndex << " " << hex <<
         regValue << dec << endl;
   }
   */

}

void OSA_stack_trace_to_stringstream(osa_cpu_object_t *cpu, stringstream *sstream) {
#ifdef _USE_SIMICS
   conf_object_t *st0 = osa_get_object_by_name("st0");
   symtable_interface_t *st_iface = 
      (symtable_interface_t*)SIM_get_interface(st0, "symtable");
   attr_value_t stack_trace = st_iface->stack_trace(cpu, 10);
   int i;
   *sstream << "Stack trace:" << endl;
   for(i = 0; i < stack_trace.u.list.size; i++) {
      *sstream << "  " << hex <<
         stack_trace.u.list.vector[i].u.list.vector[0].u.integer
         << dec << endl;
   }
#elif defined(_USE_QEMU)
#error Define OSA_stack_trace_to_stringstream contents for QEMU!
#else
#error Which simulator are you using?
#endif

   /*
   attr_value_t registerNums = SIM_get_all_registers(cpu);
    
   *sstream << "Register values:" << endl;
   for (i = 0 ; i < registerNums.u.list.size ; i++) {
		osa_integer_t regIndex = registerNums.u.list.vector[i].u.integer;
		osa_uinteger_t regValue = osa_read_register(cpu, regIndex);
      *sstream << "  " << regIndex << " " << hex <<
         regValue << dec << endl;
   }
   */

}

/**
 * trace_enabled
 * return true if global tracing is enabled.
 */
static int gtx_trace = 0;
bool trace_enabled( void ){
	return (bool) ::gtx_trace;
}


/**
 * set_tx_trace
 * Setter for tx trace attribute allows toggling of
 * tracing of transactional writes, reads stack
 * checkpoint/restore dynamically. Especially useful
 * if you bug is hours into the simulation!
 */
osa_attr_set_t
set_tx_trace( SIMULATOR_SET_INTEGER_ATTRIBUTE_SIGNATURE ) {
   osamod_t *osamod = (osamod_t *) obj;
   osamod->tx_trace = INTEGER_ARGUMENT;
   gtx_trace = osamod->tx_trace;
   return ATTR_OK;

}

/**
 * get_tx_trace
 * Getter: tx trace attribute allows toggling of
 * tracing of transactional writes, reads stack
 * checkpoint/restore dynamically. Especially useful
 * if you bug is hours into the simulation!
 */
integer_attribute_t 
get_tx_trace( SIMULATOR_GET_ATTRIBUTE_SIGNATURE ) {
   osamod_t *osamod = (osamod_t *) obj;
   return INT_ATTRIFY(osamod->tx_trace);
}

osa_attr_set_t
set_log( SIMULATOR_SET_STRING_ATTRIBUTE_SIGNATURE ) {

   osamod_t *osamod = (osamod_t*)obj;

   time_t tim = time(NULL);
   delete osamod->pStatStream;
   osamod->pStatStream = new ofstream(STRING_ARGUMENT);
   if(osamod->pStatStream->good() == false){
      return ATTR_VALUE_ERR;
   }

   *osamod->pStatStream << "Started simulation at " << ctime(&tim) << std::flush;
   osamod->pStatStream->setf(std::ios::showbase);
   return ATTR_OK;

}

string_attribute_t
get_configuration( SIMULATOR_GET_ATTRIBUTE_SIGNATURE ){
  
   osamod_t *osamod = (osamod_t*)obj;
   ostream *pStatStream = osamod->pStatStream;

   // list num cpus & freq
   int mhz = osa_sim_get_integer_attribute(OSA_get_sim_cpu(), "freq_mhz");
   *pStatStream << osamod->minfo->getNumCpus() << " " << mhz << "MHz processors " << endl;
   // cpu switch time
#ifdef _USE_SIMICS
   attr_value_t avSwitch = SIM_get_attribute(osa_get_object_by_name("sim"), "new-switch-time");
   int switch_time;
   if(avSwitch.kind == Sim_Val_Invalid){
      // we didn't change the switch time for some reason...
      switch_time = SIM_get_attribute(osa_get_object_by_name("sim"), "cpu-switch-time").u.integer;
   } else {
      switch_time = avSwitch.u.integer;
   }
#elif _USE_QEMU
   int switch_time = osa_sim_get_integer_attribute(osa_get_object_by_name("sim"), "new-switch-time");
   if(switch_time == 0) {
      switch_time = osa_sim_get_integer_attribute(osa_get_object_by_name("sim"), "cpu-switch-time");
   }
#else
#error Which Sim in osacommon.cc get_configuration()
#endif
   *pStatStream << "cpu switch time (boot) " << switch_time << endl;
   *pStatStream << "cpu switch time (after boot) " << osamod->cpu_switch_time << endl;

   // memory size
   int mem_size = osa_sim_get_integer_attribute(osamod->minfo->getComponent("ram"), "mapped_size");
   mem_size /= 1024;
   *pStatStream << mem_size << " KB RAM" << endl;
   
   // cache connected + params
   int nCaches = osamod->nCaches;
   if(nCaches > 0){
      int i;
      system_component_object_t * pstorebuffer = NULL;
      system_component_object_t * psplitter = NULL;
      system_component_object_t * pibranch = NULL;
      system_component_object_t * pdbranch = NULL;
      system_component_object_t * picache = NULL;
      system_component_object_t * pdcache = NULL;
      system_component_object_t * pl2cache = NULL;
      system_component_object_t * staller = NULL;  
    
      *pStatStream << "Cache hierarchy connected\n";
      if(nCaches) {
         // certain cache attributes are implemented
         // with global flags shared by all instances of the
         // txcache object. It suffices to use the first
         // cache to log these attributes for the entire
         // memory hierarchy. 
         psplitter = osamod->ppCaches[0];
         pdbranch = osa_sim_get_generic_attribute(psplitter, "dbranch");
         pstorebuffer = osa_sim_get_generic_attribute(pdbranch, "cache");
         pdcache = osa_sim_get_generic_attribute(pstorebuffer, "timing_model");
         int stress_test = osa_sim_get_integer_attribute(pdcache, "stress_test");
         int logmax = osa_sim_get_integer_attribute(pdcache, "logmax");
         int ringlog = osa_sim_get_integer_attribute(pdcache, "ringlog");
         int failstop = osa_sim_get_integer_attribute(pdcache, "failstop");
         *pStatStream << "txcache::stress_test:" << stress_test << endl;
         *pStatStream << "txcache::ringlog:" << ringlog << endl;
         *pStatStream << "txcache::logmax:" << logmax << endl;
         *pStatStream << "txcache::failstop:" << failstop << endl;         
      }

      *pStatStream << "-----------------------------------------------------------------------------------------------------------------------\n";
      *pStatStream << "Cache    | lines  | line size | assoc | mode  | penalty_read | penalty_read_next | penalty_write | penalty_write_next |\n";
      *pStatStream << "-----------------------------------------------------------------------------------------------------------------------\n";

      pStatStream->setf(ios_base::left, ios_base::adjustfield);

      for (i=0; i<nCaches; i++) {

         psplitter = osamod->ppCaches[i];

         // get the cache connected to the instruction branch
         pibranch = osa_sim_get_generic_attribute(psplitter, "ibranch");
         picache = osa_sim_get_generic_attribute(pibranch, "cache");

         // and the data branch
         pdbranch = osa_sim_get_generic_attribute(psplitter, "dbranch");
         pstorebuffer = osa_sim_get_generic_attribute(pdbranch, "cache");
         pdcache = osa_sim_get_generic_attribute(pstorebuffer, "timing_model");

         // get the l2cache connected to these
         pl2cache = osa_sim_get_generic_attribute(pdcache, "timing_model");

         if(pstorebuffer) {
            int passthru =  osa_sim_get_integer_attribute(pstorebuffer, "passthru");
            int buffer_tx = osa_sim_get_integer_attribute(pstorebuffer, "buffer_tx_writes");
            int nslots = osa_sim_get_integer_attribute(pstorebuffer, "slots");
            int unbounded = osa_sim_get_integer_attribute(pstorebuffer, "unbounded_tx_mode");
            pStatStream->width(8);
            *pStatStream << pstorebuffer->name << " | ";
            pStatStream->width(6);
            if(unbounded) 
               *pStatStream << "inf" << " | ";
            else 
               *pStatStream << nslots << " | ";
            pStatStream->width(9);
            *pStatStream <<
               osa_sim_get_integer_attribute(pstorebuffer, "blocksize") << " | ";
            pStatStream->width(5);
            *pStatStream << "full" << " | ";
            pStatStream->width(5);
            if(passthru) 
               *pStatStream << "OFF" << " | ";
            else 
               *pStatStream << (buffer_tx?"ROCK":"LogTM") << " | ";
            pStatStream->width(12);
            *pStatStream <<
               osa_sim_get_integer_attribute(pstorebuffer, "read_latency") << " | ";          
            pStatStream->width(17);
            *pStatStream << "--" << " | ";
            pStatStream->width(13);
            *pStatStream <<
               osa_sim_get_integer_attribute(pstorebuffer, "write_latency") << " | ";
            pStatStream->width(18);
            *pStatStream << "--" << " | " << endl;
         }

         pStatStream->width(8);
         *pStatStream << picache->name << " | ";
         pStatStream->width(6);
         *pStatStream <<
            osa_sim_get_integer_attribute(picache, "config_line_number") << " | ";
         pStatStream->width(9);
         *pStatStream <<
            osa_sim_get_integer_attribute(picache, "config_line_size") << " | ";
         pStatStream->width(5);
         *pStatStream <<
            osa_sim_get_integer_attribute(picache, "config_assoc") << " | ";
         pStatStream->width(5);
         int need_tx = osa_sim_get_integer_attribute(picache, "need_tx");
         int buffer_tx = osa_sim_get_integer_attribute(picache, "buffer_tx_writes");
         int pistress_test = osa_sim_get_integer_attribute(picache, "stress_test");
         int pistress_range = osa_sim_get_integer_attribute(picache, "stress_range");
         if(need_tx) {
            if(buffer_tx) 
               *pStatStream << "tmesi" << " | ";
            else
               *pStatStream << "xmesi" << " | ";
         } else 
            *pStatStream << "mesi" << " | ";
         pStatStream->width(pistress_range?5:12);
         *pStatStream << osa_sim_get_integer_attribute(picache, "penalty_read");
         if(pistress_test && pistress_range)
            *pStatStream << "(+/-" << pistress_range << ")";
         *pStatStream << " | ";

         pStatStream->width(pistress_range?10:17);
         *pStatStream <<
            osa_sim_get_integer_attribute(picache, "penalty_read_next"); 
         if(pistress_test && pistress_range)
            *pStatStream << "(+/-" << pistress_range << ")";
         *pStatStream << " | ";

         pStatStream->width(pistress_range?6:13);
         *pStatStream <<
            osa_sim_get_integer_attribute(picache, "penalty_write"); 
         if(pistress_test && pistress_range)
            *pStatStream << "(+/-" << pistress_range << ")";
         *pStatStream << " | ";
         pStatStream->width(pistress_range?11:18);
         *pStatStream <<
            osa_sim_get_integer_attribute(picache, "penalty_write_next");
         if(pistress_test && pistress_range)
            *pStatStream << "(+/-" << pistress_range << ")";
         *pStatStream << " | " << endl;


         pStatStream->width(8);
         *pStatStream << pdcache->name << " | ";
         pStatStream->width(6);
         *pStatStream <<
            osa_sim_get_integer_attribute(pdcache, "config_line_number") << " | ";
         pStatStream->width(9);
         *pStatStream <<
            osa_sim_get_integer_attribute(pdcache, "config_line_size") << " | ";
         pStatStream->width(5);
         *pStatStream <<
            osa_sim_get_integer_attribute(pdcache, "config_assoc") << " | ";
         pStatStream->width(5);
         need_tx = osa_sim_get_integer_attribute(pdcache, "need_tx");
         buffer_tx = osa_sim_get_integer_attribute(pdcache, "buffer_tx_writes");         
         int dstress_test = osa_sim_get_integer_attribute(pdcache, "stress_test");
         int dstress_range = osa_sim_get_integer_attribute(pdcache, "stress_range");
         if(need_tx) {
            if(buffer_tx) 
               *pStatStream << "tmesi" << " | ";
            else
               *pStatStream << "xmesi" << " | ";
         } else 
            *pStatStream << "mesi" << " | ";
         pStatStream->width(dstress_range?5:12);
         *pStatStream << osa_sim_get_integer_attribute(pdcache, "penalty_read");
         if(dstress_test && dstress_range)
            *pStatStream << "(+/-" << dstress_range << ")";
         *pStatStream << " | ";

         pStatStream->width(dstress_range?10:17);
         *pStatStream <<
            osa_sim_get_integer_attribute(pdcache, "penalty_read_next");
         if(dstress_test && dstress_range)
            *pStatStream << "(+/-" << dstress_range << ")";
         *pStatStream << " | ";
         pStatStream->width(dstress_range?6:13);
         *pStatStream <<
            osa_sim_get_integer_attribute(pdcache, "penalty_write");
         if(dstress_test && dstress_range)
            *pStatStream << "(+/-" << dstress_range << ")";
         *pStatStream << " | ";
         pStatStream->width(dstress_range?11:18);
         *pStatStream <<
            osa_sim_get_integer_attribute(pdcache, "penalty_write_next");
         if(dstress_test && dstress_range)
            *pStatStream << "(+/-" << dstress_range << ")";
         *pStatStream << " | " << endl;;

         if(strncmp(pl2cache->name, "l2cache", strlen("l2cache")))
            continue; // single-level cache configuration!

         pStatStream->width(8);
         *pStatStream << pl2cache->name << " | ";
         pStatStream->width(6);
         *pStatStream <<
            osa_sim_get_integer_attribute(pl2cache, "config_line_number") << " | ";
         pStatStream->width(9);
         *pStatStream <<
            osa_sim_get_integer_attribute(pl2cache, "config_line_size") << " | ";
         pStatStream->width(5);
         *pStatStream <<
            osa_sim_get_integer_attribute(pl2cache, "config_assoc") << " | ";
         pStatStream->width(5);
         need_tx = osa_sim_get_integer_attribute(pl2cache, "need_tx");
         buffer_tx = osa_sim_get_integer_attribute(pl2cache, "buffer_tx_writes");         
         dstress_test = osa_sim_get_integer_attribute(pl2cache, "stress_test");
         dstress_range = osa_sim_get_integer_attribute(pl2cache, "stress_range");
         if(need_tx) {
            if(buffer_tx) 
               *pStatStream << "tmesi" << " | ";
            else
               *pStatStream << "xmesi" << " | ";
         } else 
            *pStatStream << "mesi" << " | ";
         pStatStream->width(dstress_range?5:12);
         *pStatStream <<
            osa_sim_get_integer_attribute(pl2cache, "penalty_read");
         if(dstress_test && dstress_range)
            *pStatStream << "(+/-" << dstress_range << ")";
         *pStatStream << " | ";
         pStatStream->width(dstress_range?10:17);
         *pStatStream <<
            osa_sim_get_integer_attribute(pl2cache, "penalty_read_next");
         if(dstress_test && dstress_range)
            *pStatStream << "(+/-" << dstress_range << ")";
         *pStatStream << " | ";
         pStatStream->width(dstress_range?6:13);
         *pStatStream <<
            osa_sim_get_integer_attribute(pl2cache, "penalty_write");
         if(dstress_test && dstress_range)
            *pStatStream << "(+/-" << dstress_range << ")";
         *pStatStream << " | ";
         pStatStream->width(dstress_range?11:18);
         *pStatStream <<
            osa_sim_get_integer_attribute(pl2cache, "penalty_write_next");
         if(dstress_test && dstress_range)
            *pStatStream << "(+/-" << dstress_range << ")";
         *pStatStream << " | " << endl;;

         // Don't clear the general exception that may have arisen if the
         // caches don't exist - this is a bug!
      }

      staller = osa_get_object_by_name("staller");
      if(staller) {
         *pStatStream << "-----------------------------------------------------------------------------------------------------------------------\n";
         if(osa_sim_get_integer_attribute(staller, "stress_test")) 
            *pStatStream << "Staller WARNING: stress test mode is ON->stall times randomized!" << endl;
         *pStatStream << "Staller:frontbus_bandwidth_limit_pbc: " <<
            osa_sim_get_integer_attribute(staller, "frontbus_bandwidth_limit_bpc") << endl;;
         *pStatStream << "Staller:frontbus_busy_penalty_factor: " <<
            osa_sim_get_integer_attribute(staller, "frontbus_busy_penalty_factor") << endl;;
         *pStatStream << "Staller:frontbus_check_interval: " <<
            osa_sim_get_integer_attribute(staller, "frontbus_check_interval") << endl;;
         *pStatStream << "Staller:frontbus_stat_size: " <<
            osa_sim_get_integer_attribute(staller, "frontbus_stat_size") << endl;;
         *pStatStream << "Staller:frontbus_stat_interval: " <<
            osa_sim_get_integer_attribute(staller, "frontbus_stat_interval") << endl;;
         *pStatStream << "Staller:stall_time: " <<
            osa_sim_get_integer_attribute(staller, "stall_time") << endl;;
         *pStatStream << "Staller:stress_test: " <<
            osa_sim_get_integer_attribute(staller, "stress_test") << endl;;
         *pStatStream << "Staller:stress_range: " <<
            osa_sim_get_integer_attribute(staller, "stress_range") << endl;;
      }
         
      *pStatStream << "-----------------------------------------------------------------------------------------------------------------------\n";
 
      // put it back to right adjusted
      pStatStream->setf(ios_base::right, ios_base::adjustfield);

   } else {
      *pStatStream << "Caches not connected\n";
   }

   // Clear the general exception that may have arisen if the
   // caches don't exist
   if(osa_sim_get_error() == GENERIC_ERROR){
      osa_sim_clear_error();
   }


   // disk delay modeling data
   // getting the object like this is a bit
   // of a hack, but otherwise it's alot
   // of indirection
   char ide_name[256];
   sprintf(ide_name, "%s%s", osamod->minfo->getPrefix().c_str(), "ide0");
   system_component_object_t *ide0 = osa_get_object_by_name(ide_name);
  
   int model_dma_delay = osa_sim_get_integer_attribute(ide0, "model_dma_delay");

   if(model_dma_delay){
      double interrupt_delay = osa_sim_get_float_attribute(ide0, "interrupt_delay");
      *pStatStream << "Disk delay enabled during boot at " << interrupt_delay << ", ";
   } else {
      *pStatStream << "Disk delay not enabled during boot, ";
   }

   if(osamod->disk_delay > 0){
      *pStatStream << "enabled after boot at " << osamod->disk_delay << endl;
   } else {
      *pStatStream << "not enabled after boot." << endl;
   }

   // Cache perturbation info
   if(nCaches > 0) {
      int cache_seed = 0, cache_perturb = 0;      
      get_cache_perturb(obj, 0, &cache_perturb, &cache_seed); // use cpu 0
      *pStatStream << "cache_perturb_ns: " << cache_perturb << endl;
      *pStatStream << "cache_seed: " << cache_seed << endl;
   }
   pStatStream->flush();
   
   if (osa_sim_get_error() != NO_ERROR)
   {
      // TODO, should be writing this to the log.. and potentially break-point as well.
      cout << "Warning: high-falutin' cache attributes missing\n";
      osa_sim_clear_error();
   }


   return STRING_ATTRIFY("Ok");
}

osa_attr_set_t 
set_proc_cycles( SIMULATOR_SET_LIST_ATTRIBUTE_SIGNATURE ){
  
   int i;
   osamod_t *osamod = (osamod_t *) obj;

   if(osamod->procCycles != NULL){
      MM_FREE( osamod->procCycles );
   }

   osamod->procCycles = MM_ZALLOC(LIST_ARGUMENT_SIZE, osa_cycles_t);

   for( i = 0; i < LIST_ARGUMENT_SIZE; i++){
      osamod->procCycles[i] = INT_ATTR(LIST_ARGUMENT(i));
   }

   return ATTR_OK;
}

list_attribute_t
get_proc_cycles( SIMULATOR_GET_ATTRIBUTE_SIGNATURE ){
   int i;
   osamod_t *osamod = (osamod_t *) obj;
   /*
   attr_value_t avReturn;
   avReturn.kind = Sim_Val_List;
   if(osamod->procCycles != NULL){
      avReturn.u.list.size = osamod->minfo->getNumCpus();
      avReturn.u.list.vector = MM_MALLOC( osamod->minfo->getNumCpus(),
                                          attr_value_t);

      for(i = 0; i < osamod->minfo->getNumCpus(); i++){
         avReturn.u.list.vector[i].kind = Sim_Val_Integer;
         avReturn.u.list.vector[i].u.integer = osamod->procCycles[i];
      }

   } else {
      avReturn.u.list.size = 0;
   }
   */
   /* Yes, I know this switches of that memory debugging stuff, 
      but you can always switch the commenting if you really need it */
   list_attribute_t avReturn;
   if(osamod->procCycles != NULL) {
      avReturn = osa_sim_allocate_list(osamod->minfo->getNumCpus());
      for(i = 0; i < osamod->minfo->getNumCpus(); i++){
         LIST_ATTR(avReturn, i) = INT_ATTRIFY(osamod->procCycles[i]);
      }
   }
   else {
      avReturn = osa_sim_allocate_list(0);
   }
   return avReturn;
}

int sameMachine(osamod_t *a, osamod_t *b){

   return (a->system == b->system);

}

osa_attr_set_t 
set_cpu_switch_time( SIMULATOR_SET_INTEGER_ATTRIBUTE_SIGNATURE ){
   
   osamod_t *osamod = (osamod_t *) obj;
   /*
   if(pAttrValue->kind != Sim_Val_Integer){
      return Sim_Set_Need_Integer;
   }
   */
   osamod->cpu_switch_time = INTEGER_ARGUMENT;
   return ATTR_OK;

}

integer_attribute_t 
get_cpu_switch_time( SIMULATOR_GET_ATTRIBUTE_SIGNATURE ){
   osamod_t *osamod = (osamod_t *) obj;
   return INT_ATTRIFY(osamod->cpu_switch_time);
}

osa_attr_set_t set_disk_delay( SIMULATOR_SET_FLOAT_ATTRIBUTE_SIGNATURE ){
   
   osamod_t *osamod = (osamod_t *) obj;
   /*
   if(pAttrValue->kind != Sim_Val_Floating){
      return Sim_Set_Need_Floating;
   }
   */
   osamod->disk_delay = FLOAT_ARGUMENT;
   return ATTR_OK;

}

float_attribute_t get_disk_delay( SIMULATOR_GET_ATTRIBUTE_SIGNATURE ){
   osamod_t *osamod = (osamod_t *) obj;
   return FLOAT_ATTRIFY(osamod->disk_delay);
}

osa_attr_set_t set_alloc_hist(SIMULATOR_SET_INTEGER_ATTRIBUTE_SIGNATURE) {
   return ATTR_OK;
}

integer_attribute_t get_alloc_hist( SIMULATOR_GET_ATTRIBUTE_SIGNATURE ) {
   dump_alloc_hist();
#ifdef _USE_SIMICS
   return SIM_make_attr_nil();
#else
   return INT_ATTRIFY(1);
#endif
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
