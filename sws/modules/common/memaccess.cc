// MetaTM Project
// File Name: memaccess.cc
//
// Description: 
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006, 2007. All Rights Reserved.
// See LICENSE file for license terms.

#include "memaccess.h"
#include "osaassert.h"

using namespace std;

uint64_t ArchitectureWordSizeMask = 0xFFFFFFFF; // Default is 32-bit
int regEAX =  0;
int regAX  =  1;
int regAL  =  2;
int regAH  =  3;
int regECX =  4;
int regCX  =  5;
int regCL  =  6;
int regCH  =  7;
int regEDX =  8;
int regDX  =  9;
int regDL  = 10;
int regDH  = 11;
int regEBX = 12;
int regBX  = 13;
int regBL  = 14;
int regBH  = 15;
int regESP = 16;
int regSP  = 17;
int regEBP = 18;
int regBP  = 19;
int regESI = 20;
int regSI  = 21;
int regEDI = 22;
int regDI  = 23;
int regIP  = 24;
int regEIP = 25;
int regES  = 26;
int regCS  = 30;
int regSS  = 34;
int regDS  = 38;
int regFS  = 42;
int regGS  = 46;


bool ExceptionCheck(osa_logical_address_t addr)
{
   if (osa_sim_get_error() != NO_ERROR)
   {
      // TODO, should be writing this to the log.. and potentially break-point as well.
      cout << "HIT AN EXCEPTION IN HSIM_logical to physical: " 
           << "xcpt#: " 
           << osa_sim_get_error() 
           << " " << SIM_last_error()
           << " addr: 0x" << hex << addr << dec << endl;
      osa_sim_clear_error();
      return false;
   }
   return true;
}

void HandlePendingExceptions() {
   while(osa_sim_get_error() != NO_ERROR) {
      cout << "XXX: OSA_logical_to_physical called with an exception already pending: " 
           << osa_sim_get_error()
           << ": clearing..."
           << endl;
      osa_sim_clear_error();
   }
}

osa_physical_address_t OSA_logical_to_physical(osa_cpu_object_t *cpu,
                              osa_segment_t segment,
                              osa_logical_address_t vaddr)
{
//   cout << " Logical to physical for address " << vaddr << endl;
   HandlePendingExceptions();
   osa_physical_address_t pa = osa_logical_to_physical(cpu, segment, vaddr);
   if (!ExceptionCheck(vaddr))
      return 0;
   return pa;
}

unsigned int 
get_frame_word(osa_cpu_object_t *cpu, int offset)
{
	// Get linear address for stack top
   osa_logical_address_t _bp =  ArchitectureWordSizeMask & _osa_read_register(cpu, regEBP);
   _bp += offset;

   HandlePendingExceptions();
   osa_physical_address_t bp = OSA_logical_to_physical(cpu, STACK_SEGMENT, _bp);

   // Indicates failure of translation
   if(bp == 0) return 0;
   unsigned int retVal = osa_read_phys_memory(cpu, bp, 4);
   if (!ExceptionCheck(_bp))
      return 0;
   return retVal;
}

unsigned int 
osa_read_sim_byte(osa_cpu_object_t *cpu, osa_segment_t segment, osa_logical_address_t laddr)
{
   HandlePendingExceptions();
   laddr &= ArchitectureWordSizeMask;
   osa_physical_address_t paddr = OSA_logical_to_physical(cpu, segment, laddr);
   if(paddr == (osa_physical_address_t)0)  return 0;
   unsigned int retVal = osa_read_phys_memory(cpu, paddr, 1);
   if (!ExceptionCheck(laddr))
      return 0;
   return retVal;
}

// NOTE: This is a physical variant of the function
unsigned int
osa_read_sim_4bytes_phys(osa_cpu_object_t* cpu, osa_physical_address_t paddr) {
   HandlePendingExceptions();
   paddr &= ArchitectureWordSizeMask;
   if(paddr == (osa_physical_address_t)0)  return 0;
   unsigned long long retVal = osa_read_phys_memory(cpu, paddr, 4);
   if (!ExceptionCheck(paddr))
      return 0;
   return retVal;
}

unsigned int
osa_read_sim_4bytes(osa_cpu_object_t *cpu, osa_segment_t segment, osa_logical_address_t laddr) {
   laddr &= ArchitectureWordSizeMask;
   osa_physical_address_t paddr = OSA_logical_to_physical(cpu, segment, laddr);
   if(paddr == (osa_physical_address_t)0)  return 0;
   unsigned int retVal = ArchitectureWordSizeMask & osa_read_phys_memory(cpu, paddr, 4);
   if (!ExceptionCheck(laddr))
      return 0;
   return retVal;
}

// NOTE: This is a physical variant of the function
unsigned long long
osa_read_sim_8bytes_phys(osa_cpu_object_t *cpu, osa_physical_address_t paddr) {
   HandlePendingExceptions();
   paddr &= ArchitectureWordSizeMask;
   if(paddr == (osa_physical_address_t)0)  return 0;
   unsigned long long retVal = osa_read_phys_memory(cpu, paddr, 8);
   if (!ExceptionCheck(paddr))
      return 0;
   return retVal;
}

unsigned long long
osa_read_sim_8bytes(osa_cpu_object_t *cpu, osa_segment_t segment, osa_logical_address_t laddr) {
   HandlePendingExceptions();
   laddr &= ArchitectureWordSizeMask;
   osa_physical_address_t paddr = OSA_logical_to_physical(cpu, segment, laddr);
   if(paddr == (osa_physical_address_t)0)  return 0;
   unsigned long long retVal = osa_read_phys_memory(cpu, paddr, 8);
   if (!ExceptionCheck(laddr))
      return 0;
   return retVal;
}


void
osa_write_sim_byte(osa_cpu_object_t *cpu, osa_segment_t segment, osa_logical_address_t laddr, unsigned int byte) {
   HandlePendingExceptions();
   laddr &= ArchitectureWordSizeMask;
   osa_physical_address_t paddr = OSA_logical_to_physical(cpu, segment, laddr);
   if(paddr == (osa_physical_address_t)0)  return;
   osa_write_phys_memory(cpu, paddr, (uint8)byte, 1);
   if (!ExceptionCheck(laddr))
      return;
}

void
osa_write_sim_4bytes(osa_cpu_object_t *cpu, osa_segment_t segment, osa_logical_address_t laddr, unsigned int val) {
   HandlePendingExceptions();
   laddr &= ArchitectureWordSizeMask;
   osa_physical_address_t paddr = OSA_logical_to_physical(cpu, segment, laddr);
   if(paddr == (osa_physical_address_t)0)  return;
   osa_write_phys_memory(cpu, paddr, val, 4);
   if (!ExceptionCheck(laddr))
      return;
}

int
read_string(osa_cpu_object_t *cpu, osa_logical_address_t start_addr, char* str, 
            int str_limit, bool null_terminate)
{
   HandlePendingExceptions();
   start_addr &= ArchitectureWordSizeMask;
   int i = 0;
   str[str_limit - 1] = 0;
   for (; i < str_limit - 1; i++) {
      str[i] = osa_read_sim_byte(cpu, DATA_SEGMENT, start_addr++); //assuming always DATA_SEGMENT for now..
      if (!ExceptionCheck(start_addr))
         return 0;
      if(null_terminate && str[i] == 0) break;
   }
   return i;
}

int
write_string(osa_cpu_object_t *cpu, osa_logical_address_t start_addr, char* str, 
             int str_limit, bool null_terminate)
{
   HandlePendingExceptions();
   start_addr &= ArchitectureWordSizeMask;
   int i = 0;
   osa_write_sim_byte(cpu, DATA_SEGMENT, start_addr + str_limit, 0);
   for (; i < str_limit; i++) {
      osa_write_sim_byte(cpu, DATA_SEGMENT, start_addr++, str[i]);
      if (!ExceptionCheck(start_addr)){
         return 0;
      }
      if(null_terminate && str[i] == 0) break;
   }
   return i;
}


string
osa_get_name_val(osa_cpu_object_t *cpu) {
   char name[1024];
   char msg[1024];
   osa_logical_address_t str_ptr = osa_read_register(cpu, regEBX);
   osa_uinteger_t val =  osa_read_register(cpu,regECX);

   unsigned long int uval = val;

   read_string(cpu, str_ptr, name, 1024, true);
   if(val == 0xdeadcafe) {
      snprintf(msg, 1024, "%s", name);
   } else if(val < 1000) {
      snprintf(msg, 1024, "%s=%lu", name, uval);
   } else if(val < 100000) {
      snprintf(msg, 1024, "%s=%#08lx (%lu)", name, uval, uval);
   } else {
      snprintf(msg, 1024, "%s=%#08lx", name, uval);
   }
   return string(msg);
}

string
osa_get_num_char(osa_cpu_object_t *cpu) {
   // kernel/printk.c::vprintk::printk_buf is 1024 characters
   char msg[1025];
   osa_logical_address_t str_ptr = osa_read_register(cpu, regEBX);
   osa_uinteger_t num_chars =  osa_read_register(cpu, regECX);

   OSA_assert(num_chars < 1025, 0);
   if(num_chars > 1024) 
      num_chars = 1024;
   read_string(cpu, str_ptr, msg, num_chars + 1, false);
   msg[num_chars + 1] = 0;
   string ret(msg);
   string::size_type pos = 0;
   ret.replace(pos, 0, "PRINTK "); // Prefix begining of message
   while((pos = ret.find("\n", pos)) != string::npos
         // Don't replace the last \n if that is the last character 
         // in the string.
         // Output sent to console seems to end with \n
         && pos < ret.length() - 1) {
      // TXGRPH is 6 chars, and that seems like a good standard
      ret.replace(pos, 1, "\nPRINTK ");
      // Don't look at \n again
      pos++;
   }
   // Make sure message ends with a newline
   if(ret[ret.length() - 1] != '\n') {
      ret += "\n";
   }
   return ret;
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
