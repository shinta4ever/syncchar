#include "profile.h"
#include "common.h"
#include "memaccess.h"

static unsigned int std_read_4bytes(osamod_t *osamod,
      osa_cpu_object_t *cpu, osa_segment_t segment,
      osa_logical_address_t addr) {
   return osa_read_sim_4bytes(cpu, segment, addr);
}

void init_profiler(osamod_t *osamod) {
   osamod->common->prof.interval = 0;
   osamod->common->prof.jitter = 0;
   osamod->common->prof.log_prefix = MM_STRDUP("profiler");
   osamod->common->prof.log_number = 0;
   osamod->common->prof.scheduled = false;
   osamod->common->prof.memread = std_read_4bytes;
   osamod->common->prof.mem_reader = osamod;

   // look up the limits of kernel text in the symtable
   conf_object_t *st0 = osa_get_object_by_name("st0");

   attr_value_t addr;

   attr_value_t symbol = SIM_make_attr_string("_text");
   addr = SIM_get_attribute_idx(st0, "symbol_value", &symbol);
   logical_address_t text_min = addr.u.integer;

   symbol = SIM_make_attr_string("_end");
   addr = SIM_get_attribute_idx(st0, "symbol_value", &symbol);
   logical_address_t text_max = addr.u.integer;

   osamod->common->prof.text_min = text_min;
   osamod->common->prof.text_max = text_max;
}

static void profiler_tick(conf_object_t *cpu, void* osamod_arg) {
   osamod_t *osamod = (osamod_t*)osamod_arg;
   common_data_t *common = osamod->common;

   logical_address_t eip = SIM_get_program_counter(cpu);
   if(eip >= common->prof.text_min && eip < common->prof.text_max) {
      profile_iterator_t fpit = common->prof.flat_profile.find(eip);
      if(fpit != common->prof.flat_profile.end()) {
         fpit->second++;
      }
      else {
         common->prof.flat_profile[eip] = 1;
      }

      profile_iterator_t spit = common->prof.stack_profile.find(eip);
      if(spit != common->prof.stack_profile.end()) {
         spit->second++;
      }
      else {
         common->prof.stack_profile[eip] = 1;
      }

      uinteger_t ebp = _osa_read_register(cpu, regEBP);
      uinteger_t esp = _osa_read_register(cpu, regESP);
      for(int n_frames = 0; n_frames < MAX_FRAMES; n_frames++) {
         // check to see if ebp points to the same stack as esp
         uinteger_t esp_stack = esp & ~8191;
         uinteger_t ebp_stack = ebp & ~8191;
         if(esp_stack != ebp_stack) {
            break;
         }

         // read the return address and check if its in kernel text
         uinteger_t ra = common->prof.memread(common->prof.mem_reader, cpu,
               STACK_SEGMENT, ebp+4);
         if(ra < common->prof.text_min || ra >= common->prof.text_max) {
            break;
         }

         spit = common->prof.stack_profile.find(ra);
         if(spit != common->prof.stack_profile.end()) {
            spit->second++;
         }
         else {
            common->prof.stack_profile[ra] = 1;
         }

         // read previous ebp and check that it actually goes up the stack
         uinteger_t prev_ebp = common->prof.memread(common->prof.mem_reader,
               cpu, STACK_SEGMENT, ebp);
         if(prev_ebp <= ebp) {
            break;
         }
         ebp = prev_ebp;
         n_frames++;
      }
   }

   profiler_schedule_tick(osamod, cpu);
}

void profiler_schedule_tick(osamod_t *osamod, osa_cpu_object_t *cpu) {
   if(cpu == NULL || osamod->common->prof.interval <= 0) {

      // schedule or remove a callback on all cpus
      int num_cpus = osamod->minfo->getNumCpus();
      for(int i = 0; i < num_cpus; i++) {
         cpu = osamod->minfo->getCpu(i);
         if(osamod->common->prof.interval > 0) {
            SIM_time_post_cycle(cpu, osamod->common->prof.interval,
                  Sim_Sync_Processor, profiler_tick, osamod);
         }
         else {
            SIM_time_clean(cpu, Sim_Sync_Processor, profiler_tick, osamod);
         }
      }

      osamod->common->prof.scheduled = (osamod->common->prof.interval > 0);
   }
   else {
      // schedule the next callback on this cpu
      SIM_time_post_cycle(cpu, osamod->common->prof.interval,
            Sim_Sync_Processor, profiler_tick, osamod);
   }
}

void dump_profile(osamod_t *osamod, ostream *out) {
   if(osamod->common->prof.flat_profile.empty()) {
      return;
   }

   bool cleanup_out = false;
   if(out == NULL) {
      if(osamod->common->prof.log_prefix == NULL) {
         return;
      }

      int len = strlen(osamod->common->prof.log_prefix);
      char *logname = (char*)alloca(len + 7);
      strcpy(logname, osamod->common->prof.log_prefix);
      logname[len] = '-';
      logname[len+1] = 'a' + (unsigned char)osamod->common->prof.log_number;
      strcpy(logname + len + 2, ".log");
      out = new ofstream(logname);
      cleanup_out = true;
   }

   *out << "BEGIN FLAT PROFILE" << endl;
   profile_iterator_t pit = osamod->common->prof.flat_profile.begin();
   uinteger_t total = 0;
   for(; pit != osamod->common->prof.flat_profile.end(); pit++) {
      unsigned int addr = pit->first;
      unsigned int count = pit->second;
      *out << hex << addr << " " << dec << count << endl;
      total += count;
   }
   *out << "TOTAL: " << total << endl << endl;

   *out << "BEGIN CALL PROFILE" << endl;
   pit = osamod->common->prof.stack_profile.begin();
   total = 0;
   for(; pit != osamod->common->prof.stack_profile.end(); pit++) {
      unsigned int addr = pit->first;
      unsigned int count = pit->second;
      *out << hex << addr << " " << dec << count << endl;
      total += count;
   }
   *out << "TOTAL: " << total << endl << endl;

   if(cleanup_out) {
      delete out;
   }
}

void clear_profile(osamod_t *osamod) {
   osamod->common->prof.flat_profile.clear();
   osamod->common->prof.stack_profile.clear();
   osamod->common->prof.log_number++;
}
