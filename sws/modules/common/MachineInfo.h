// MetaTM Project
// File Name: MachineInfo.h
//
// Description: 
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006, 2007. All Rights Reserved.
// See LICENSE file for license terms.

#ifndef MACHINEINFO_H
#define MACHINEINFO_H

#include "simulator.h"
using namespace std;

//////////////////////
// Present a single machine to query
// for modules

class OsaMachineInfo { //all initialization will be done by the derived class
 public:
   int getNumCpus() {
      return numCpus;
   }

   osa_cpu_object_t *getCpu(int cpuNo) {
      return cpus[cpuNo];
   }

   int getCpuNum(osa_cpu_object_t *cpu) {
      int i;
      for (i=0; i<numCpus; i++)
         if (cpus[i] == cpu)
            return i;

      // pr("XXX Request for invalid processor number\n");
      return -1;
   }

   virtual system_component_object_t *getComponent(const char* name) = 0;

   string getPrefix() {
      return prefix;
   }

   // In osatxm, the instruction decoder is shared between
   // all machines, so we need to map cpus to osamods
 static struct _osamod_t *getOwner(osa_cpu_object_t *cpu) { return owners[cpu]; }

 virtual ~OsaMachineInfo() { }
 protected:
   static map<osa_cpu_object_t *, struct _osamod_t *> owners;
   system_object_t *system;
   int numCpus;
   osa_cpu_object_t **cpus;
   string prefix;
};

#ifdef _USE_SIMICS
class SimicsMachineInfo : public OsaMachineInfo {
   //For now we just implement the interface....
 public:
   SimicsMachineInfo(system_object_t *system, struct _osamod_t *osamod = NULL);
   system_component_object_t *getComponent(const char* name);
   virtual ~SimicsMachineInfo() { }
};

typedef SimicsMachineInfo MachineInfo;
#elif defined(_USE_QEMU)
#error "You need to define QemuMachineInfo for QEMU!!"
#else
#error "Which simulator are you developing for?"
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
