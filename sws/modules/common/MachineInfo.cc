// MetaTM Project
// File Name: MachineInfo.cc
//
// Description: 
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006, 2007. All Rights Reserved.
// See LICENSE file for license terms.

#include "MachineInfo.h"

map<osa_cpu_object_t *, struct _osamod_t *> OsaMachineInfo::owners;

#ifdef _USE_SIMICS
SimicsMachineInfo::SimicsMachineInfo(system_object_t *system, struct _osamod_t *osamod) {
	attr_value_t cpulist;

	this->system = system;

	// get the cpu_list attribute from the system, make
	// sure that it is a list
	cpulist = SIM_get_attribute(system, "cpu_list");
	if (cpulist.kind != Sim_Val_List) {
		// Die somehow ?
      SIM_free_attribute(cpulist);
		return;
	}

	// build the list of cpu conf_object_t-s
	numCpus = 0;
	cpus = new conf_object_t*[cpulist.u.list.size];
	while (numCpus < cpulist.u.list.size) {
      osa_cpu_object_t *cpu = cpulist.u.list.vector[numCpus].u.object;
		cpus[numCpus] = cpu;

      if(osamod != NULL)
         owners[cpu] = osamod;

		numCpus++;
	}
   SIM_free_attribute(cpulist);

   prefix = SIM_get_attribute(this->system, "object_prefix").u.string;
}

system_component_object_t *SimicsMachineInfo::getComponent(const char *name) {
	attr_value_t objList;
	int i;

	objList = SIM_get_attribute(this->system, "object_list");
	if (objList.kind != Sim_Val_Dict) {
		// wtf?
		return NULL;
	}

	// run thru the dictionary, look for name
	for (i=0; i < objList.u.dict.size; i++) {
		if (0 == strcmp(objList.u.dict.vector[i].key.u.string, name))
			return objList.u.dict.vector[i].value.u.object;
	}

   SIM_free_attribute(objList);

	return NULL;
}

#elif defined(_USE_QEMU)
#error Define QemuMachineInfo
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
