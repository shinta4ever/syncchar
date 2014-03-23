// MetaTM Project
// File Name: simulator.h
//
// Description: 
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006, 2007. All Rights Reserved.
// See LICENSE file for license terms.


#ifndef OSA_SIMICS_H
#define OSA_SIMICS_H

/* Simics includes go here */
#include <simics/api.h>
#include <simics/alloc.h>
#include <simics/utils.h>
#include <simics/arch/x86.h>
/* Typedefs here */

/* memory transaction should have the following 
 * access_type
 * mode
 * size
 * laddr
 * paddr
 * ID - maybe
 * is read? is data? is from cpu?
 * access_linear? - Simics only!
 * type? I think only simics might need this.. define osa_memop_basic_type anyway..
 * real_address - this is the address of the actual _HOST_ (i.e. outside sim) address where the data is contained
   In other words, the simulator stores the guest data in some buffer, and this is the address of that buffer
   Also defined as "destination for a load or source of a store operation"
 */

/* Note that since we take over control flow in Simics while we will not do so in QEMU. Need to workaround? */
typedef uinteger_t                                        osa_uinteger_t;
typedef integer_t                                         osa_integer_t;
typedef logical_address_t                                 osa_logical_address_t;
typedef physical_address_t                                osa_physical_address_t;
typedef linear_address_t                                  osa_linear_address_t;
typedef cycles_t                                          osa_cycles_t;
typedef conf_object_t                                     osa_cpu_object_t;
typedef data_or_instr_t                                   osa_segment_t;
typedef generic_transaction_t                             osa_sim_inner_memop_t;
typedef x86_memory_transaction_t                          osa_sim_outer_memop_t;
typedef processor_mode_t                                  osa_privilege_level_t;
typedef x86_access_type_t                                 osa_memop_specific_type_t;
typedef mem_op_type_t                                     osa_memop_basic_type_t;
typedef attr_value_t                                      osa_segment_register_t; //do not confuse with osa_segment_t!!
typedef sim_exception_t                                   osa_simulator_error_t;
typedef exception_type_t                                  osa_exception_type_t;
typedef conf_object_t                                     system_object_t;
typedef conf_object_t                                     system_component_object_t;

/* Macros here */
#define CODE_SEGMENT Sim_DI_Instruction
#define DATA_SEGMENT Sim_DI_Data
#define STACK_SEGMENT Sim_DI_Data
#define OSA_get_sim_cpu() SIM_current_processor()
#define _osa_read_register SIM_read_register
#define _osa_write_register SIM_write_register
#define osa_read_phys_memory SIM_read_phys_memory
#define osa_write_phys_memory SIM_write_phys_memory
#define GET_OUTER_MEMOP_PTR_FROM_INNER_PTR(x) (SIM_x86_mem_trans_from_generic(x))
#define GET_INNER_MEMOP_PTR_FROM_OUTER_PTR(x) (&((x)->s))
#define osa_user_mode Sim_CPU_Mode_User
#define osa_supervisor_mode Sim_CPU_Mode_Supervisor
#define osa_get_sim_cycle_count SIM_cycle_count
#define osa_get_proc_privilege SIM_processor_privilege_level
#define osa_logical_to_physical SIM_logical_to_physical //OSA_log... also exists. It checks for simulator exceptions, while this does not
#define osa_release_segment_register SIM_free_attribute  //don't need this for QEMU! Just make it a null
#define NO_ERROR SimExc_No_Exception
#define MEM_ERROR SimExc_Memory 
#define GENERIC_ERROR SimExc_General
#define NO_EXCEPTION Sim_PE_No_Exception
#define STALL Sim_PE_Stall_Cpu
#define osa_get_object_by_name SIM_get_object
#define osa_break_simulation SIM_break_simulation
#define osa_quit SIM_quit
#define osa_sim_get_interface SIM_get_interface
#define osa_sim_clear_error SIM_clear_exception
//#define osa_sim_get_error SIM_get_pending_exception
#define osa_sim_free_attribute SIM_free_attribute //may just make this a null for QEMU
#define osa_sim_allocate_list SIM_alloc_attr_list

#define SIMULATOR_SET_ATTRIBUTE_SIGNATURE      void *arg, \
                                               conf_object_t *obj, \
                                               attr_value_t *pAttrValue, \
                                               attr_value_t *pAttrIdx

#define SIMULATOR_GET_ATTRIBUTE_SIGNATURE      void *arg, \
                                               conf_object_t *obj, \
                                               attr_value_t *pAttrIdx

#define TIMING_SIGNATURE                       conf_object_t* pConfObject, \
                                               conf_object_t* pSpaceConfObject, \
                                               map_list_t* pMapList, \
                                               osa_sim_inner_memop_t* pMemTx 

#define TIMING_ARGUMENTS                       pConfObject, \
                                               pSpaceConfObject, \
                                               pMapList, \
                                               pMemTx

#define TIMING_ARGUMENTS_1                     pCache, \
                                               pSpaceConfObject, \
                                               pMapList, \
                                               pMemTx

#define TIMING_ARGUMENTS_2                     pOsaTxm->timing_model, \
                                               pSpaceConfObject, \
                                               pMapList, \
                                               pMemTx

#define BOOLEAN_ARGUMENT                       pAttrValue->u.boolean
#define INTEGER_ARGUMENT                       pAttrValue->u.integer
#define GENERIC_ARGUMENT                       pAttrValue->u.object
#define FLOAT_ARGUMENT                         pAttrValue->u.floating
#define STRING_ARGUMENT                        pAttrValue->u.string //assumes char* returned... not C++ string
#define LIST_ARGUMENT_SIZE                     pAttrValue->u.list.size
#define LIST_ARGUMENT(i)                       pAttrValue->u.list.vector[(i)]

#define SIMULATOR_SET_BOOLEAN_ATTRIBUTE_SIGNATURE SIMULATOR_SET_ATTRIBUTE_SIGNATURE 
#define SIMULATOR_SET_INTEGER_ATTRIBUTE_SIGNATURE SIMULATOR_SET_ATTRIBUTE_SIGNATURE 
#define SIMULATOR_SET_STRING_ATTRIBUTE_SIGNATURE SIMULATOR_SET_ATTRIBUTE_SIGNATURE 
#define SIMULATOR_SET_GENERIC_ATTRIBUTE_SIGNATURE SIMULATOR_SET_ATTRIBUTE_SIGNATURE 
#define SIMULATOR_SET_LIST_ATTRIBUTE_SIGNATURE SIMULATOR_SET_ATTRIBUTE_SIGNATURE 
#define SIMULATOR_SET_FLOAT_ATTRIBUTE_SIGNATURE SIMULATOR_SET_ATTRIBUTE_SIGNATURE 

#define BOOL_ATTRIFY SIM_make_attr_boolean
#define INT_ATTRIFY SIM_make_attr_integer
#define STRING_ATTRIFY SIM_make_attr_string
#define GENERIC_ATTRIFY SIM_make_attr_object
#define FLOAT_ATTRIFY SIM_make_attr_floating
#define ATTR_OK Sim_Set_Ok
#define ATTR_VALUE_ERR Sim_Set_Illegal_Value
#define INTERFACE_NOT_FOUND_ERR Sim_Set_Interface_Not_Found

#define BOOL_ATTR(x) (x).u.boolean
#define INT_ATTR(x) (x).u.integer
#define STRING_ATTR(x) (x).u.string
#define GENERIC_ATTR(x) (x).u.object
#define FLOAT_ATTR(x) (x).u.floating
#define LIST_ATTR(x,i)(x).u.list.vector[(i)]
#define LIST_SIZE(x) (x).u.list.size
#define LIST_ATTR_P(x, i) (x)->u.list.vector[(i)]
#define LIST_SIZE_P(x) (x)->u.list.size

typedef attr_value_t integer_attribute_t;
typedef attr_value_t boolean_attribute_t;
typedef attr_value_t string_attribute_t;
typedef attr_value_t generic_attribute_t;
typedef attr_value_t float_attribute_t;
typedef attr_value_t list_attribute_t;
typedef set_error_t osa_attr_set_t;

#define osa_sim_set_integer_attribute SIM_set_attribute
#define osa_sim_set_boolean_attribute SIM_set_attribute
#define osa_sim_set_string_attribute SIM_set_attribute
#define osa_sim_set_generic_attribute SIM_set_attribute
#define osa_sim_set_float_attribute SIM_set_attribute

#define osa_sim_get_integer_attribute(x, y) SIM_get_attribute(x, y).u.integer
#define osa_sim_get_boolean_attribute(x, y) SIM_get_attribute(x, y).u.boolean
#define osa_sim_get_string_attribute(x, y) SIM_get_attribute(x, y).u.string
#define osa_sim_get_generic_attribute(x, y) SIM_get_attribute(x, y).u.object
#define osa_sim_get_float_attribute(x, y) SIM_get_attribute(x, y).u.floating

/* Functions go here */
inline unsigned int&
size_memop(osa_sim_inner_memop_t* op) {
   return op->size;
}

inline unsigned int&
size_memop(osa_sim_outer_memop_t* op) {
   return(size_memop(GET_INNER_MEMOP_PTR_FROM_OUTER_PTR(op)));
}

inline int&
id_memop(osa_sim_inner_memop_t* op) {
   return op->id;
}

inline int&
id_memop(osa_sim_outer_memop_t* op) {
   return(id_memop(GET_INNER_MEMOP_PTR_FROM_OUTER_PTR(op)));
}

inline osa_privilege_level_t&
priv_level_memop(osa_sim_outer_memop_t* op) {
   return op->mode;
}

inline osa_privilege_level_t&
priv_level_memop(osa_sim_inner_memop_t* op) {
   return(priv_level_memop(GET_OUTER_MEMOP_PTR_FROM_INNER_PTR(op)));
}

inline osa_logical_address_t&
laddr_memop(osa_sim_inner_memop_t* op) {
   return op->logical_address;
}

inline osa_logical_address_t&
laddr_memop(osa_sim_outer_memop_t* op) {
   return(laddr_memop(GET_INNER_MEMOP_PTR_FROM_OUTER_PTR(op)));
}

inline osa_physical_address_t&
paddr_memop(osa_sim_inner_memop_t* op) {
   return op->physical_address;
}

inline osa_physical_address_t&
paddr_memop(osa_sim_outer_memop_t* op) {
   return(paddr_memop(GET_INNER_MEMOP_PTR_FROM_OUTER_PTR(op)));
}

inline osa_linear_address_t& //I don't think QEMU will need this. Will make it a null op
linear_addr_memop(osa_sim_outer_memop_t* op) {
   return op->linear_address;
}

inline osa_linear_address_t&
linear_addr_memop(osa_sim_inner_memop_t* op) {
   return(linear_addr_memop(GET_OUTER_MEMOP_PTR_FROM_INNER_PTR(op)));
}

inline bool
is_read_memop(osa_sim_inner_memop_t* op) {
   return SIM_mem_op_is_read(op);
}

inline bool
is_read_memop(osa_sim_outer_memop_t* op) {
   return(is_read_memop(GET_INNER_MEMOP_PTR_FROM_OUTER_PTR(op)));
}

inline bool
is_data_memop(osa_sim_inner_memop_t* op) {
   return SIM_mem_op_is_data(op);
}

inline bool
is_data_memop(osa_sim_outer_memop_t* op) {
   return(is_data_memop(GET_INNER_MEMOP_PTR_FROM_OUTER_PTR(op)));
}

inline bool
is_from_cpu_memop(osa_sim_inner_memop_t* op) {
   return SIM_mem_op_is_from_cpu(op);
}

inline bool
is_from_cpu_memop(osa_sim_outer_memop_t* op) {
   return(is_from_cpu_memop(GET_INNER_MEMOP_PTR_FROM_OUTER_PTR(op)));
}

inline osa_memop_specific_type_t&
memop_specific_access_type(osa_sim_outer_memop_t *op) {
   return op->access_type;
}

inline osa_memop_specific_type_t&
memop_specific_access_type(osa_sim_inner_memop_t *op) {
   return(memop_specific_access_type(GET_OUTER_MEMOP_PTR_FROM_INNER_PTR(op)));
}

inline char*&
data_ptr_memop(osa_sim_inner_memop_t *op) {
   return op->real_address;
}

inline char*&
data_ptr_memop(osa_sim_outer_memop_t *op) {
   return(data_ptr_memop(GET_INNER_MEMOP_PTR_FROM_OUTER_PTR(op)));
}

inline osa_segment_register_t
osa_get_segment_register(osa_cpu_object_t *cpu, const char *segname) {
   return SIM_get_attribute(cpu, segname);
}

inline osa_uinteger_t 
seg_base(osa_segment_register_t segment) {
   return segment.u.list.vector[7].u.integer;
}

inline osa_uinteger_t 
seg_limit(osa_segment_register_t segment) {
   return segment.u.list.vector[8].u.integer;
}

inline osa_uinteger_t 
seg_valid(osa_segment_register_t segment) {
   return segment.u.list.vector[9].u.integer;
}

static inline osa_simulator_error_t
osa_sim_get_error(void) {  //The API does not support an error queue. You _must_ get the error
   // immediately or be resigned to just not getting one.. QEMU may not do a good job of setting
   // said errors (i.e. will only set errors for situations which we know are queried
   return SIM_get_pending_exception();
}


extern int regEAX;
extern int regAX;
extern int regAL;
extern int regAH;
extern int regECX;
extern int regCX;
extern int regCL;
extern int regCH;
extern int regEDX;
extern int regDX;
extern int regDL;
extern int regDH;
extern int regEBX;
extern int regBX;
extern int regBL;
extern int regBH;
extern int regESP;
extern int regSP;
extern int regEBP;
extern int regBP;
extern int regESI;
extern int regSI;
extern int regEDI;
extern int regDI;
extern int regIP;
extern int regEIP;
extern int regES;
extern int regCS;
extern int regSS;
extern int regDS;
extern int regFS;
extern int regGS;


static inline void dict_to_map(attr_value_t dict,
      std::map<std::string, attr_value_t> &m) {
   for(int i = 0; i < dict.u.dict.size; i++) {
      m[dict.u.dict.vector[i].key.u.string] = dict.u.dict.vector[i].value;
   }
}
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
