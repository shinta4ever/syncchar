#ifndef __PROFILE_H
#define __PROFILE_H

#include <simulator.h>
#include <osacommon.h>
#include <fstream>

using namespace std::tr1;

#define MAX_FRAMES 100

typedef struct _ip_profiler {
   const char *log_prefix;
   int log_number;
   unsigned int interval;
   unsigned int jitter;
   bool scheduled;

   // cache the object/interface who should service our memory
   // reads (i.e. osatxm)
   unsigned int (*memread)(osamod_t *osamod, osa_cpu_object_t *cpu,
         osa_segment_t segment, osa_logical_address_t addr);
   osamod_t *mem_reader;

   unordered_map<unsigned int, unsigned long long> flat_profile;
   unordered_map<unsigned int, unsigned long long> stack_profile;
   logical_address_t text_min;
   logical_address_t text_max;
} ip_profiler_t;

typedef unordered_map<unsigned int, unsigned long long>::iterator
   profile_iterator_t;


void init_profiler(struct _osamod_t *common);
void profiler_schedule_tick(struct _osamod_t *osamod, osa_cpu_object_t *cpu);
void dump_profile(struct _osamod_t *osamod, ostream *out);
void clear_profile(struct _osamod_t *osamod);

#endif
