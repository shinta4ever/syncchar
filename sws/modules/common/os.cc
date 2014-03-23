// MetaTM Project
// File Name: os.cc
//
// Description: 
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006, 2007. All Rights Reserved.
// See LICENSE file for license terms.

#include "osaassert.h"
using namespace std;
#include "memaccess.h"
#include "osacommon.h"
#include "os.h"

void init_procs(osamod_t *osamod){
   // we have to use new here because os_data_t contains
   // embedded c++ classes whose contstructors need
   // calling
   os_data_t *os = new os_data_t;

   osamod->os = os;
   os->last_pid = 0;
   os->timer_count = 0;
   os->spid_max = 1;
    
   struct pid_info *p0 = new struct pid_info();
   p0->pid = 0;
   p0->spid = 0;
   p0->kernel = TRUE;
   p0->cpu = 0;
   p0->state = 0;
   p0->syscall = 0;

   os->procs[0] = p0;
   os->sprocs[0] = p0;
   for(int i = 0; i < osamod->minfo->getNumCpus(); i++){
      os->current_process[i] = 0;
   }
   os->last_pid = 0;
   os->stat_interval = 10000000;
   os->kstat_plugin = NULL;
}

void os_sched(osamod_t *osamod){
   os_data_t *os = osamod->os;

   osa_cpu_object_t *cpu = OSA_get_sim_cpu();
   int pid =  (int)osa_read_register(cpu, regECX);
   unsigned int len =  (int)osa_read_register(cpu, regEDX);
   osa_logical_address_t ptr = osa_read_register(cpu, regEBX);

   struct pid_info *pp = os->procs[pid];
   if(pp == NULL){
      pr("Trying to access pid %d, which hasn't been forked yet.  Qua!?!\n", pid);
      osa_break_simulation("Invalid Access");
      return;
   }
   OSA_assert(pp != NULL, osamod);
   if(len > 255){ len = 255; }
	    
   memset(pp->cmd, 0, len + 1);
   if(ptr != 0 && len != 0){
      for(unsigned int i = 0; i < len - 1; i++){
         pp->cmd[i] = osa_read_sim_byte(cpu, DATA_SEGMENT, ptr++);
         if(pp->cmd[i] == 0 && i == 0){
            break;
         }
         if(pp->cmd[i] == 0 && i != len - 1){
            pp->cmd[i] = ' ';
         }
      }
   }
	    
   pp->cpu = osamod->minfo->getCpuNum(cpu);
   os->current_process[pp->cpu] = pp->spid;
   //cout << "Running process " << pp->pid << ", cmd = [" << pp->cmd << "]" << endl;
}

void os_enter_sched(osamod_t *osamod){
   osa_cpu_object_t *cpu = OSA_get_sim_cpu();
   int pid =  (int)osa_read_register(cpu, regEBX);
   int cpunum = osamod->minfo->getCpuNum(cpu);
   bool bverbose = false;

   // we just want to make a note that we've
   // started a context switch on this processor. 
   osamod->ctxtsws[cpunum].last_ctxtsw_start = osa_get_sim_cycle_count(cpu);
   if(bverbose) {
      cout
         << "os_enter_sched: " 
         << pid 
         << endl;
   }
}

void os_exit_switch_to(osamod_t *osamod){
   osa_cpu_object_t *cpu = OSA_get_sim_cpu();
   int pid =  (int)osa_read_register(cpu, regEBX);
   int cpunum = osamod->minfo->getCpuNum(cpu);
   bool bverbose = false; 
   // assume a context switch has just completed. 
   osa_cycles_t ctxtst = osamod->ctxtsws[cpunum].last_ctxtsw_start;
   if(ctxtst) {
      osa_cycles_t ctxtcyc = osa_get_sim_cycle_count(cpu) - ctxtst;
      osamod->ctxtsws[cpunum].tot_ctxtsw_cyc += ctxtcyc;
      osamod->ctxtsws[cpunum].tot_ctxtsw++;
   }
   if(bverbose) {
      cout
         << "os_exit_switch_to: " 
         << pid 
         << " switches: " 
         << osamod->ctxtsws[cpunum].tot_ctxtsw
         << " switch cyc: " 
         << osamod->ctxtsws[cpunum].tot_ctxtsw_cyc
         << endl;
   }
}

void os_timer(osamod_t *osamod){
   osamod->os->timer_count++;
}

void os_fork(osamod_t *osamod){
   os_data_t *os = osamod->os;

   osa_cpu_object_t *cpu = OSA_get_sim_cpu();
   int pid = osa_read_register(cpu, regEBX);
   int kernel =  osa_read_register(cpu, regECX);
   
   //cout << "Forked pid " << pid << " in kernel = " << kernel << endl;

   struct pid_info *pp = new struct pid_info();
   OSA_assert(pp != NULL, osamod);
   pp->pid = pid;
   if(pid != os->last_pid + 1){
      pr("XXX Out-of-order pid allocation: last = %d, new pid = %d", os->last_pid,  pid);
      pr(" on CPU %d", osamod->minfo->getCpuNum(cpu));
      //SIM_break_simulation("out of order pid");
      if(os->last_pid > 30000 && pid < 1000){
         pr(", This may be a rollover."); 
      }
      pr("\n");
   }
   os->last_pid = pid;
   pp->spid = os->spid_max;
   os->spid_max++;
   pp->kernel = kernel;
   if(os->procs[pid]){
      // garbage collect old proc_info
      pr("XXX: Garbage collecting info for pid %d\n", pid);
      struct pid_info *old_pp = os->procs[pid];
      delete old_pp;
   }
   os->procs[pid] = pp;
   os->sprocs[pp->spid] = pp;

   // Initialize to unknown
   pp->state = 256;

   //  pr("Created process %d\n", pid);
}

void os_exit(osamod_t *osamod){
   os_data_t *os = osamod->os;

   osa_cpu_object_t *cpu = OSA_get_sim_cpu();
   osa_uinteger_t pid =  osa_read_register(cpu, regECX);

   struct pid_info *pp = os->procs[pid];
   os->sprocs.erase(pp->spid);
   os->procs.erase(pid);
   delete pp;
}

void os_task_state(osamod_t *osamod){
   os_data_t *os = osamod->os;

   osa_cpu_object_t *cpu = OSA_get_sim_cpu();
   int pid =  (int)osa_read_register(cpu, regEBX);
   int state =  (int)osa_read_register(cpu, regECX);

   struct pid_info *pp = os->procs[pid];
   if(pp == NULL){
      pr("Trying to set state on pid %d, which hasn't been forked yet.  Qua!?!\n", pid);
      osa_break_simulation("Invalid Access");
      return;
   }

   pp->state = state;

   //cout << "Setting process state " << pp->pid << " to " << state << endl;
}

void os_cur_syscall(osamod_t *osamod){
   os_data_t *os = osamod->os;

   osa_cpu_object_t *cpu = OSA_get_sim_cpu();
   int pid =  (int)osa_read_register(cpu, regEBX);
   int syscall =  (int)osa_read_register(cpu, regECX);

   struct pid_info *pp = os->procs[pid];
   if(pp == NULL){
      pr("Trying to set syscall on pid %d, which hasn't been forked yet.  Qua!?!\n", pid);
      osa_break_simulation("Invalid Access");
      return;
   }

   pp->syscall = syscall;

   //cout << "Setting process state " << pp->pid << " to " << state << endl;
}


void kstat_callback(lang_void *callback_data,
                    system_component_object_t *trigger_obj){

   osamod_t *osamod = (osamod_t*)callback_data;
   osa_cpu_object_t *cpu = OSA_get_sim_cpu();

   // if you set two periodic event handlers with
   // different data, both are called for each
   // other's events, so check if this osamod
   // owns the current cpu.  this seems like a
   // bug in simics
   if(osamod->minfo->getCpuNum(cpu) == -1)
      return;


   if(osamod->os->kstats.size() > 0){
      for(unsigned int i = 0; i < osamod->os->kstats.size(); i++) {
         osa_physical_address_t phys_addr = osamod->os->kstats[i];
         cputime64_t user = osa_read_sim_8bytes_phys(cpu, phys_addr);
         cputime64_t nice = osa_read_sim_8bytes_phys(cpu, phys_addr + 8);
         cputime64_t system  = osa_read_sim_8bytes_phys(cpu, phys_addr + 16);
         cputime64_t softirq  = osa_read_sim_8bytes_phys(cpu, phys_addr + 24);
         cputime64_t irq  = osa_read_sim_8bytes_phys(cpu, phys_addr + 32);
         cputime64_t idle = osa_read_sim_8bytes_phys(cpu, phys_addr + 40);
         cputime64_t iowait = osa_read_sim_8bytes_phys(cpu, phys_addr + 48);
         cputime64_t steal = osa_read_sim_8bytes_phys(cpu, phys_addr + 56);
         /*
           pr("cpu%d, cycle %llu: %llu %llu %llu %llu %llu %llu %llu %llu\n",
           i, osa_get_sim_cycle_count(cpu), user, nice, system, softirq, irq, idle, iowait, steal);
         */

       /* Don't pollute STDERR with kstat messages */
         if(osamod->type != COMMON){
            *osamod->pStatStream << "KSTAT cpu" << i << " cycle " << osa_get_sim_cycle_count(cpu) << " " << user
                                 << " " << nice << " " << system << " " << softirq << " " << irq 
                                 << " " << idle << " " << iowait << " " << steal << endl;
         }
         
      }
      
      // Call the module plugins
      if(osamod->os->kstat_plugin != 0){
         osamod->os->kstat_plugin(callback_data, trigger_obj);
      }
   }
}


void kstat_callback_2_4(lang_void *callback_data,
                    system_component_object_t *trigger_obj){

   osamod_t *osamod = (osamod_t*)callback_data;
   osa_cpu_object_t *cpu = OSA_get_sim_cpu();

   // if you set two periodic event handlers with
   // different data, both are called for each
   // other's events, so check if this osamod
   // owns the current cpu.  this seems like a
   // bug in simics
   if(osamod->minfo->getCpuNum(cpu) == -1)
      return;

    osa_physical_address_t phys_addr = osamod->os->kstat_2_4_paddr;
    int nrcpus = osamod->os->kstat_2_4_nrcpus;
    for(int i = 0; i < osamod->minfo->getNumCpus(); i++){
       cputime64_t user = (cputime64_t) osa_read_sim_4bytes_phys(cpu, phys_addr + i*4);
       cputime64_t nice = (cputime64_t) osa_read_sim_4bytes_phys(cpu, phys_addr + 4*nrcpus + i*4);
       cputime64_t system  = (cputime64_t) osa_read_sim_4bytes_phys(cpu, phys_addr + 2*4*nrcpus + i*4);
       cputime64_t idle = (cputime64_t) osa_read_sim_4bytes_phys(cpu, phys_addr + 3*4*nrcpus + i*4);

       /* Not sure if we can get the following information from kstat on 2.4
        * I'm just leaving them zero.
        * -sangmin
        */
       cputime64_t softirq  = 0; //read_sim_8bytes_phys(cpu, phys_addr + 24);
       cputime64_t irq  = 0; //read_sim_8bytes_phys(cpu, phys_addr + 32);
       cputime64_t iowait = 0; //read_sim_8bytes_phys(cpu, phys_addr + 48);
       cputime64_t steal = 0; //read_sim_8bytes_phys(cpu, phys_addr + 56);
       /* Don't pollute STDERR with kstat messages */
       if(osamod->type != COMMON){
          *osamod->pStatStream << "KSTAT cpu" << i << " cycle " << osa_get_sim_cycle_count(cpu) << " " << user
                               << " " << nice << " " << system << " " << softirq << " " << irq 
                               << " " << idle << " " << iowait << " " << steal << endl;
       }
    }

    // Call the module plugins
    if(osamod->os->kstat_plugin != 0){
       osamod->os->kstat_plugin(callback_data, trigger_obj);
    }
}

#ifdef _USE_SIMICS
void set_kstat_callback(osamod_t *osamod, int new_int){

   if(SIM_hap_callback_exists("Core_Periodic_Event",
                              kstat_callback, osamod)){
      SIM_hap_delete_callback("Core_Periodic_Event",
                              kstat_callback, osamod);
   }
   osamod->os->stat_interval = new_int;
  
   if(osamod->os->stat_interval > 0){
      if( osamod->os->kernel_version >= 2600 )
            SIM_hap_add_callback_index("Core_Periodic_Event",
                                  kstat_callback,
                                  (void*)osamod,
                                  osamod->os->stat_interval);
      else if ( osamod->os->kernel_version >= 2400 &&
                    osamod->os->kernel_version < 2600 )
            SIM_hap_add_callback_index("Core_Periodic_Event",
                                 kstat_callback_2_4,
                                 (void*)osamod,
                                 osamod->os->stat_interval);
      else
         SIM_break_simulation("Unknown Kernel Version");
   }
}
#else
#error Define some form of kstat mechanism for QEMU
#endif

void os_kstat(osamod_t *osamod){
   os_data_t *os = osamod->os;
   osa_cpu_object_t *cpu = OSA_get_sim_cpu();
   int offset =  osa_read_register(cpu, regEBX);
   unsigned int addr   = osa_read_register(cpu, regECX);

   osa_physical_address_t physical_addr = OSA_logical_to_physical(cpu, DATA_SEGMENT, addr);
   os->kstats.push_back(physical_addr);
   os->snapshot.push_back(new PeriodicData());
   os->kernel_version = 2600;
   if(offset == (osamod->minfo->getNumCpus() - 1)){
      set_kstat_callback(osamod, os->stat_interval);
   }
}

void os_kstat_2_4(osamod_t *osamod){
   os_data_t *os = osamod->os;
   osa_cpu_object_t *cpu = OSA_get_sim_cpu();
   int nrcpus =  osa_read_register(cpu, regEBX);
   unsigned int addr   = osa_read_register(cpu, regECX);

   osa_physical_address_t physical_addr = OSA_logical_to_physical(cpu, DATA_SEGMENT, addr);
   os->kstat_2_4_paddr = physical_addr;
   os->kstat_2_4_nrcpus = nrcpus;
   os->kernel_version = 2400;

    if( osamod->minfo->getNumCpus() > os->kstat_2_4_nrcpus ){
        osa_break_simulation("Number of CPUs kernel is configured to support is less than number of CPUs the machine has.");
    }

    for(int i = 0; i < osamod->minfo->getNumCpus(); i++){
       os->kstats.push_back(physical_addr); // this addresses are not correct so should not be used
       os->snapshot.push_back(new PeriodicData());
    }

   set_kstat_callback(osamod, os->stat_interval);
}

osa_attr_set_t set_stat_interval( SIMULATOR_SET_INTEGER_ATTRIBUTE_SIGNATURE ){

   osamod_t *osamod = (osamod_t*)obj;
   os_data_t *os = osamod->os;
   
#ifdef _USE_SIMICS
   if(pAttrValue->kind != Sim_Val_Integer){
      return Sim_Set_Need_Integer;
   }
#endif
   if(os->stat_interval != INTEGER_ARGUMENT){
      os->stat_interval = INTEGER_ARGUMENT;
      if(os->kstats.size() > 0){
         set_kstat_callback(osamod, pAttrValue->u.integer);
      }
   }
   return ATTR_OK;
}

integer_attribute_t get_stat_interval( SIMULATOR_GET_ATTRIBUTE_SIGNATURE ){

   osamod_t *osamod = (osamod_t*)obj;
   return INT_ATTRIFY(osamod->os->stat_interval);

}

osa_attr_set_t set_kstat_addrs(os_data_t *os, list_attribute_t *pAttrValue){
   for(int i = 0; i < LIST_SIZE_P(pAttrValue); i++){
      os->kstats.push_back(INT_ATTR(LIST_ATTR_P(pAttrValue, i)));
   }
   return ATTR_OK;
}


list_attribute_t get_kstat_addrs(os_data_t *os){
   list_attribute_t avReturn = osa_sim_allocate_list(os->kstats.size());
   for(unsigned int i = 0; i < os->kstats.size(); i++){
      LIST_ATTR(avReturn, i) = INT_ATTRIFY(os->kstats[i]);
   }
   return avReturn; 
}

//OSA_TODO: No idea how to handle the next four functions...
#ifdef _USE_SIMICS
struct pid_info *deserialize_pid_info(attr_value_t av){
   struct pid_info *pid = new struct pid_info();
   map<string, attr_value_t> pinfo_map;
   dict_to_map(av, pinfo_map);

   pid->pid = pinfo_map["pid"].u.integer;
   pid->spid = pinfo_map["spid"].u.integer;
   pid->kernel = pinfo_map["kernel"].u.boolean;
   pid->cpu = pinfo_map["cpu"].u.integer;
   strncpy(pid->cmd, pinfo_map["cmd"].u.string, 256);
   pid->state = pinfo_map["state"].u.integer;
   pid->syscall = pinfo_map["syscall"].u.integer;
   return pid;
}

attr_value_t serialize_pid_info(struct pid_info *pid){
   attr_value_t avReturn = SIM_alloc_attr_dict(7);
   avReturn.u.dict.vector[0].key = SIM_make_attr_string("pid");
   avReturn.u.dict.vector[0].value = SIM_make_attr_integer(pid->pid);

   avReturn.u.dict.vector[1].key = SIM_make_attr_string("spid");
   avReturn.u.dict.vector[1].value = SIM_make_attr_integer(pid->spid);

   avReturn.u.dict.vector[2].key = SIM_make_attr_string("kernel");
   avReturn.u.dict.vector[2].value = SIM_make_attr_boolean(pid->kernel);

   avReturn.u.dict.vector[3].key = SIM_make_attr_string("cpu");
   avReturn.u.dict.vector[3].value = SIM_make_attr_integer(pid->cpu);

   avReturn.u.dict.vector[4].key = SIM_make_attr_string("cmd");
   avReturn.u.dict.vector[4].value = SIM_make_attr_string(pid->cmd);

   avReturn.u.dict.vector[5].key = SIM_make_attr_string("state");
   avReturn.u.dict.vector[5].value = SIM_make_attr_integer(pid->state);

   avReturn.u.dict.vector[6].key = SIM_make_attr_string("syscall");
   avReturn.u.dict.vector[6].value = SIM_make_attr_integer(pid->syscall);

   return avReturn;
}

osa_attr_set_t set_os_visibility(void *arg, conf_object_t *obj,
                              attr_value_t *pAttrValue, attr_value_t *pAttrIdx){

   osamod_t *osamod = (osamod_t*)obj;
   os_data_t *os = osamod->os;
   map<string, attr_value_t> os_map;
   dict_to_map(*pAttrValue, os_map);

   os->spid_max = os_map["spid_max"].u.integer;
   for(int i = 0; i < os_map["sprocs"].u.dict.size; i++){
      os->sprocs[os_map["sprocs"].u.dict.vector[i].key.u.integer] = 
         deserialize_pid_info(os_map["sprocs"].u.dict.vector[i].value);
   }

   for(int i = 0; i < os_map["procs"].u.dict.size; i++){
      os->procs[os_map["procs"].u.dict.vector[i].key.u.integer] = 
         os->sprocs[os_map["procs"].u.dict.vector[i].value.u.integer];
   }

   for(int i = 0; i < os_map["current_process"].u.dict.size; i++){
      os->current_process[os_map["current_process"].u.dict.vector[i].key.u.integer] = 
         os_map["current_process"].u.dict.vector[i].value.u.integer;
   }

   os->last_pid = os_map["last_pid"].u.integer;

   os->timer_count = os_map["timer_count"].u.integer;

   set_kstat_addrs(os, &(os_map["kstat_addrs"]));

   return Sim_Set_Ok;
}


attr_value_t get_os_visibility( void *arg, conf_object_t *obj,
                                attr_value_t *pAttrIdx){
   osamod_t *osamod = (osamod_t*)obj;
   os_data_t *os = osamod->os;

   attr_value_t avReturn = SIM_alloc_attr_dict(7);
   avReturn.u.dict.vector[0].key = SIM_make_attr_string("spid_max");
   avReturn.u.dict.vector[0].value = SIM_make_attr_integer(os->spid_max);
   // serialize sprocs
   // then organize procs and current_process by spid
   avReturn.u.dict.vector[1].key = SIM_make_attr_string("sprocs");
   avReturn.u.dict.vector[1].value = SIM_alloc_attr_dict(os->sprocs.size());
   int i = 0;
   for(map<spid_t, struct pid_info*>::iterator iter = os->sprocs.begin(); iter != os->sprocs.end(); iter++, i++){
      avReturn.u.dict.vector[1].value.u.dict.vector[i].key = SIM_make_attr_integer(iter->first);
      avReturn.u.dict.vector[1].value.u.dict.vector[i].value = serialize_pid_info(iter->second);
   }

   avReturn.u.dict.vector[2].key = SIM_make_attr_string("procs");
   avReturn.u.dict.vector[2].value = SIM_alloc_attr_dict(os->procs.size());
   i = 0;
   for(map<int, struct pid_info*>::iterator iter = os->procs.begin(); iter != os->procs.end(); iter++, i++){
      avReturn.u.dict.vector[2].value.u.dict.vector[i].key = SIM_make_attr_integer(iter->first);
      avReturn.u.dict.vector[2].value.u.dict.vector[i].value = SIM_make_attr_integer(iter->second->spid);
   }

   avReturn.u.dict.vector[3].key = SIM_make_attr_string("current_process");
   avReturn.u.dict.vector[3].value = SIM_alloc_attr_dict(os->current_process.size());
   i = 0;
   for(map<int, spid_t>::iterator iter = os->current_process.begin(); iter != os->current_process.end(); iter++, i++){
      avReturn.u.dict.vector[3].value.u.dict.vector[i].key = SIM_make_attr_integer(iter->first);
      avReturn.u.dict.vector[3].value.u.dict.vector[i].value = SIM_make_attr_integer(iter->second);
   }

   avReturn.u.dict.vector[4].key = SIM_make_attr_string("last_pid");
   avReturn.u.dict.vector[4].value = SIM_make_attr_integer(os->last_pid);

   avReturn.u.dict.vector[5].key = SIM_make_attr_string("timer_count");
   avReturn.u.dict.vector[5].value = SIM_make_attr_integer(os->timer_count);

   avReturn.u.dict.vector[6].key = SIM_make_attr_string("kstat_addrs");
   avReturn.u.dict.vector[6].value = get_kstat_addrs(os);

   return avReturn;
}
#else
#error I have to figure out this stuff in os.cc
#endif

void prepare_kstats_snapshot(osamod_t *osamod){
   osa_cpu_object_t *cpu = osamod->minfo->getCpu(0);

   // if you set two periodic event handlers with
   // different data, both are called for each
   // other's events, so check if this osamod
   // owns the current cpu.  this seems like a
   // bug in simics

   if(osamod->os->kernel_version >= 2600){
       if(osamod->os->snapshot.size() > 0){
            for(unsigned int i = 0; i < osamod->os->snapshot.size(); i++) {
                 osa_physical_address_t phys_addr = osamod->os->kstats[i];
         
                 osamod->os->snapshot[i]->user = osa_read_sim_8bytes_phys(cpu, phys_addr);
                 osamod->os->snapshot[i]->nice = osa_read_sim_8bytes_phys(cpu, phys_addr + 8);
                 osamod->os->snapshot[i]->system  = osa_read_sim_8bytes_phys(cpu, phys_addr + 16);
                 osamod->os->snapshot[i]->softirq  = osa_read_sim_8bytes_phys(cpu, phys_addr + 24);
                 osamod->os->snapshot[i]->irq  = osa_read_sim_8bytes_phys(cpu, phys_addr + 32);
                 osamod->os->snapshot[i]->idle = osa_read_sim_8bytes_phys(cpu, phys_addr + 40);
                 osamod->os->snapshot[i]->iowait = osa_read_sim_8bytes_phys(cpu, phys_addr + 48);
                 osamod->os->snapshot[i]->steal = osa_read_sim_8bytes_phys(cpu, phys_addr + 56);
          }
       }

   } else {
      /* For 2.4 kernel kstat */
      osa_physical_address_t phys_addr = osamod->os->kstat_2_4_paddr;
      int nrcpus = osamod->os->kstat_2_4_nrcpus;
      for(unsigned int i = 0; i < osamod->os->snapshot.size(); i++){
         osamod->os->snapshot[i]->user = (cputime64_t)osa_read_sim_4bytes_phys(cpu, phys_addr + i*4 );
         osamod->os->snapshot[i]->nice = (cputime64_t)osa_read_sim_4bytes_phys(cpu, phys_addr + 4*nrcpus + i*4 );
         osamod->os->snapshot[i]->system = (cputime64_t)osa_read_sim_4bytes_phys(cpu, phys_addr + 2*4*nrcpus + i*4 );
         osamod->os->snapshot[i]->softirq = 0;
         osamod->os->snapshot[i]->irq     = 0;
         osamod->os->snapshot[i]->idle    = (cputime64_t)osa_read_sim_4bytes_phys(cpu, phys_addr + 3*4*nrcpus + i*4 );
         osamod->os->snapshot[i]->iowait  = 0;
         osamod->os->snapshot[i]->steal   = 0;
      }
   }
}

string take_kstats_snapshot(osamod_t *osamod){

   osa_cpu_object_t *cpu = osamod->minfo->getCpu(0);
   ostringstream stat_str;
   stat_str.setf(std::ios::showbase);

   if(osamod->os->kernel_version >= 2600){
       if(osamod->os->kstats.size() > 0){
          for(unsigned int i = 0; i < osamod->os->kstats.size(); i++) {
             osa_physical_address_t phys_addr = osamod->os->kstats[i];
             cputime64_t user = osa_read_sim_8bytes_phys(cpu, phys_addr);
             cputime64_t nice = osa_read_sim_8bytes_phys(cpu, phys_addr + 8);
             cputime64_t system  = osa_read_sim_8bytes_phys(cpu, phys_addr + 16);
             cputime64_t softirq  = osa_read_sim_8bytes_phys(cpu, phys_addr + 24);
             cputime64_t irq  = osa_read_sim_8bytes_phys(cpu, phys_addr + 32);
             cputime64_t idle = osa_read_sim_8bytes_phys(cpu, phys_addr + 40);
             cputime64_t iowait = osa_read_sim_8bytes_phys(cpu, phys_addr + 48);
             cputime64_t steal = osa_read_sim_8bytes_phys(cpu, phys_addr + 56);

             user = user - osamod->os->snapshot[i]->user;
             nice = nice - osamod->os->snapshot[i]->nice;
             system = system - osamod->os->snapshot[i]->system;
             softirq = softirq - osamod->os->snapshot[i]->softirq;
             irq = irq - osamod->os->snapshot[i]->irq;
             idle = idle - osamod->os->snapshot[i]->idle;
             iowait = iowait - osamod->os->snapshot[i]->iowait;
             steal = steal - osamod->os->snapshot[i]->steal;

         stat_str << "KSTAT_SNAPSHOT cpu" << i << " cycle " << osa_get_sim_cycle_count(cpu) << " " << user
                  << " " << nice << " " << system << " " << softirq << " " << irq 
                  << " " << idle << " " << iowait << " " << steal << endl;
             
          }
       }

   } else {
      /* For 2.4 kernel kstat */
      osa_physical_address_t phys_addr = osamod->os->kstat_2_4_paddr;
      int nrcpus = osamod->os->kstat_2_4_nrcpus;
      for(int i = 0; i < osamod->minfo->getNumCpus(); i++){
          cputime64_t user = (cputime64_t)osa_read_sim_4bytes_phys(cpu, phys_addr +  i*4);
          cputime64_t nice = (cputime64_t)osa_read_sim_4bytes_phys(cpu, phys_addr + 4*nrcpus + i*4 );
          cputime64_t system  = (cputime64_t)osa_read_sim_4bytes_phys(cpu, phys_addr + 2*4*nrcpus + i*4);
          cputime64_t softirq  = 0;
          cputime64_t irq  = 0;
          cputime64_t idle  = (cputime64_t)osa_read_sim_4bytes_phys(cpu, phys_addr + 3*4*nrcpus + i*4);
          cputime64_t iowait = 0;
          cputime64_t steal = 0;

          if(osamod->os->snapshot.size()) {
             user = user - osamod->os->snapshot[i]->user;
             nice = nice - osamod->os->snapshot[i]->nice;
             system = system - osamod->os->snapshot[i]->system;
             idle  = idle - osamod->os->snapshot[i]->idle;
             
             stat_str << "KSTAT_SNAPSHOT cpu" << i << " cycle " << osa_get_sim_cycle_count(cpu) << " " << user
                      << " " << nice << " " << system << " " << softirq << " " << irq 
                      << " " << idle << " " << iowait << " " << steal << endl;
          }
      }

   }

   stat_str.flush(); // Empty buffers
   return stat_str.str();
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
