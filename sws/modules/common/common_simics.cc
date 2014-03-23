#include "simulator.h"
#include "osacommon.h"
#include "common.h"

/**** Begin: Code to manage idle cycles between end of benchmarks and killing simulation ***/

void idle_callback(lang_void *callback_data,
                          conf_object_t *trigger_obj){

   osa_quit(0);
}

void set_idle_callback(osamod_t *obj){
   osamod_t *osamod = (osamod_t*)obj;
   common_data_t *common = osamod->common;
   if(common->idle_cycles > 0){
      SIM_hap_add_callback_index("Core_Periodic_Event",
                                 idle_callback,
                                 obj,
                                 common->idle_cycles);
   } else {
      SIM_quit(0);
   }
}

/**** End: Code to manage idle cycles between end of benchmarks and killing simulation ***/

/**** Begin: Code for other modules to register with us ***/

attr_value_t get_next_module( void *arg, conf_object_t *obj,
                                     attr_value_t *pAttrIdx){

   return SIM_make_attr_nil();
}

/**** End: Code for other modules to register with us ***/
