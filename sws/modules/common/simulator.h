// MetaTM Project
// File Name: simulator.h
//
// Description: 
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006, 2007. All Rights Reserved.
// See LICENSE file for license terms.


#ifndef OSA_SIMULATOR_H
#define OSA_SIMULATOR_H

/* All STL includes go here */
#include <map>
#include <vector>
#include <list>
#include <set>
#include <tr1/unordered_map>
#include <deque>
#include <stack>
#include <iostream>
#include <cstddef>
#include <string>
#include <sstream>
#include <fstream>
#include <ostream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <errno.h>
#include "allochist.h"


#if defined(_USE_SIMICS)
#include "simics.h"
#elif defined(_USE_QEMU)
#include "qemu.h"
#endif

bool OSA_condor();
void OSA_break_simulation(const char *msg, struct _osamod_t *osamod);
void OSA_stack_trace(osa_cpu_object_t *cpu, struct _osamod_t *osamod);

static bool OSA_attr_equal(attr_value_t a, attr_value_t b){

   if(a.kind != b.kind){
      std::cerr << "Different kinds " << a.kind << ", " << b.kind << std::endl;
      return false;

   }

   switch(a.kind){

   case Sim_Val_Invalid:
   case Sim_Val_Nil:
      return true;

   case Sim_Val_Integer:
      if(a.u.integer != b.u.integer){
         std::cerr << "Different int " << a.u.integer << ", " << b.u.integer << std::endl;
      }
      return a.u.integer == b.u.integer;

   case Sim_Val_Boolean:

      if(a.u.boolean != b.u.boolean){
         std::cerr << "Different bool " << a.u.boolean << ", " << b.u.boolean << std::endl;
      }
      return a.u.boolean == b.u.boolean;

   case Sim_Val_Floating:
      if(a.u.floating != b.u.floating){
         std::cerr << "Different float " << a.u.floating << ", " << b.u.floating << std::endl;
      }
      return a.u.floating == b.u.floating;

   case Sim_Val_List:
      {
         if(a.u.list.size != b.u.list.size){
            std::cerr << "Different size list " << a.u.list.size << ", " << b.u.list.size << std::endl;
            return false;
         }
         
         for(int i = 0; i < a.u.list.size; i++){
            if(!OSA_attr_equal(a.u.list.vector[i], b.u.list.vector[i]))
               return false;
         }
      }
      return true;
   default:
      std::cerr << "XXX: Sim_attr_equal called on unimplemented type: " << a.kind << std::endl;
      return false;
   }


   return true;
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
