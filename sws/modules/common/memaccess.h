// MetaTM Project
// File Name: memaccess.h
//
// Description: 
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006, 2007. All Rights Reserved.
// See LICENSE file for license terms.

#ifndef _MEMACCESS_H
#define _MEMACCESS_H

using namespace std;
#include "simulator.h"

extern uint64_t ArchitectureWordSizeMask;

osa_physical_address_t OSA_logical_to_physical(osa_cpu_object_t *cpu,
                                           osa_segment_t segment,
                                           osa_logical_address_t vaddr);

unsigned int 
get_frame_word(osa_cpu_object_t *cpu, int offset);
unsigned int 
osa_read_sim_byte(osa_cpu_object_t *cpu, osa_segment_t segment, osa_logical_address_t laddr);
unsigned int
osa_read_sim_4bytes(osa_cpu_object_t *cpu, osa_segment_t segment, osa_logical_address_t laddr);
unsigned int
osa_read_sim_4bytes_phys(conf_object_t* cpu, physical_address_t paddr);
unsigned long long
osa_read_sim_8bytes(osa_cpu_object_t *cpu, osa_segment_t segment, osa_logical_address_t laddr);
unsigned long long
osa_read_sim_8bytes_phys(osa_cpu_object_t *cpu, osa_physical_address_t paddr);
void
osa_write_sim_byte(osa_cpu_object_t *cpu, osa_segment_t segment, osa_logical_address_t laddr, unsigned int byte);
void
osa_write_sim_4bytes(osa_cpu_object_t *cpu, osa_segment_t segment, osa_logical_address_t laddr, unsigned int val);
int
read_string(osa_cpu_object_t *cpu, osa_logical_address_t start_addr, char* str, 
            int str_limit, bool null_terminate);
int
write_string(osa_cpu_object_t *cpu, osa_logical_address_t start_addr, char* str, 
            int str_limit, bool null_terminate);

string
osa_get_name_val(osa_cpu_object_t *cpu);
string
osa_get_num_char(osa_cpu_object_t *cpu);

osa_uinteger_t inline
osa_read_register(osa_cpu_object_t *cpu, int register_number){
   return ArchitectureWordSizeMask & _osa_read_register(cpu, register_number);
}

void inline
osa_write_register(osa_cpu_object_t *cpu, int register_number, osa_uinteger_t value){
   _osa_write_register(cpu, register_number, ArchitectureWordSizeMask & value);
}

#endif // _MEMACCESS_H

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
