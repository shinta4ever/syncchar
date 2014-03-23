// SyncChar Project
// File Name: sync_char.cc
//
// Description: 
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006, 2007. All Rights Reserved.
// See LICENSE file for license terms.

/* TODO's: 
 *    Add get/set funcs for lockmap
 *    track stacked-post-ed data
 */
 
///////////////////////////////////////////////////////////
// sync_char
// The driver file for the module that characterizes
// synchronization behavior in the kernel.  It reads
// the sync_char.map file created by running objdump on the kernel
// to find all of the inlined calls to synch primitives.  It builds
// a state machine to track synch behavior and gather stats.

/*** Be sure to include STL first, otherwise simics gets cranky */
#include <tr1/unordered_map>
#include <map>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <deque>
#include <list>

#include "../common/simulator.h"
#include "../common/memaccess.h"
#include "../common/osamagic.h"
#include "../common/osacommon.h"
#include "../common/osacache.h"
#include "../common/os.h"

#include "WorkSet.h"

#include "stdio.h"
#include <sys/time.h>
#include <time.h>

using namespace std;
using namespace std::tr1;


//#define DEBUG_INTERACTIVE 1
//#define DEBUG_ADDRESS 0xf7f9237c

#define OSA_PRINT_TO_CONSOLE	false /*true*/
bool gbOsaPrintCout	= OSA_PRINT_TO_CONSOLE;

// XXX These must match the definitions in sync_char_pre.py
#undef F_LOCK
// Don't use F_LOCK and F_UNLOCK because rwsem_atomic_update cannot be
// categorized as a lock or unlock
const unsigned int F_LOCK      = 0x1;    // It locks
const unsigned int F_UNLOCK    = 0x2;    // It unlocks
const unsigned int F_TRYLOCK   = 0x4;    // It trys to get the lock
const unsigned int F_INLINED   = 0x8;    // Inlined function
const unsigned int F_EAX       = 0x10;   // Address in 0(%EAX)
const unsigned int F_TIME_RET  = 0x20;   // Once this instruction executes, note time until
// ret (allowing for additional call/ret pairs
// before ret)
const unsigned int F_NOADDR    = 0x40;  // No address for this lock, it is acq/rel at this pc

//Type of lock
const unsigned int L_SEMA      = 1;
const unsigned int L_RSEMA     = 2;
const unsigned int L_WSEMA     = 3;
const unsigned int L_SPIN      = 4;
const unsigned int L_RSPIN     = 5;
const unsigned int L_WSPIN     = 6;
const unsigned int L_MUTEX     = 7;
const unsigned int L_COMPL     = 8;
const unsigned int L_RCU       = 9;
const unsigned int L_FUTEX     = 10;
const unsigned int L_CXA       = 11;
const unsigned int L_CXE       = 12;

inline int cache_count(osamod_t *osamod) {
   return osamod->minfo->getNumCpus();
}

// Two maps for lock information, one keyed by caller ra, the other by
// lock address
// Map from lock_ra to information for lock address
struct ra {
   unsigned long      lock_off;
   unsigned long      lock_reg;
   unsigned           lock_id;
   unsigned long      flags;
   char*              label;
   // Omit func_pc
};

// Maintain information for average and variance
typedef struct avg_var {
   unsigned int cnt;
   long double sum; // sum of x
   long double xsqr; // sum of x^2 note that xsqr/cnt - (sum/cnt)^2 == variance
} avg_var;

struct caller {
   // Number of times caller calls lock routine. Difference between
   // this number and hold_av[0] + acq_av[0] is # of (test&set) retries.
   unsigned long long count;
   // When first try to acquire, is lock busy?
   unsigned long long q_count;

   // If we queue up, are we data dependent or independent
   //unsigned long long q_count_dependent;
   //unsigned long long q_count_independent;

   // In data dependent conflicts, how many total bytes are in the address set?
   //unsigned long long q_count_dependent_total_bytes;
   // In data dependent conflicts, how many total bytes are in conflict?
   //unsigned long long q_count_dependent_conflicting_bytes;

   // Working sets of critical sections we are waiting on
   vector<WorksetID> contended_worksets;

   // Free lock that is already free.
   unsigned int useless_release;
   /// NB: These are only updated under the acquire RA
   // Average cycle times for holding a lock.  Grab & release is a
   // hold, even if other (e.g., other readers) are also holding 
   avg_var hold_av[3];
   // Time between acquire semaphore and getting scheduled.  For all, short
   // and long cases (short is less than 1,000 cycles)
   avg_var acq_av[3];
};

// Map from lock addr to lock info
const short LKST_OPEN = 1; // Lock available
const short LKST_RLK  = 2; // Read lock, no writers allowed
const short LKST_WRLK = 3; // Locked to readers & writers
const short LKST_CXA  = 4; // Transient state: a transaction is entering
                           // the protected critical region
struct spid_info {
   osa_cycles_t      req_cyc; // First attempt to get lock
   osa_cycles_t      acq_cyc; // When we get lock
   unsigned long acq_ra;  // Lock caller
   int           cnt; // For spid that grabs read locks multiple times
};

//#define BUSTED_GCC 1
#ifdef BUSTED_GCC
typedef map<unsigned int, struct ra> ra_map_t;
typedef map<unsigned int, struct ra>::const_iterator ra_mapcit_t;

// Map caller ra to stats
typedef map<unsigned int, struct caller> caller_map_t;
typedef map<unsigned int, struct caller>::const_iterator caller_mapcit_t;
typedef map<unsigned int, struct caller>::iterator caller_mapit_t;

typedef map<spid_t, int> reader_map_t;
typedef map<spid_t, int>::iterator reader_mapit_t;
typedef map<spid_t, int>::const_iterator reader_mapcit_t;

typedef map <spid_t, struct spid_info> acq_map_t;
typedef map<spid_t, struct spid_info>::const_iterator acq_mapcit_t;

typedef map<unsigned int, spcl_lock_t> spid_spcl_map_t;
typedef map<int, spid_spcl_map_t> spcl_map_t;

#else
typedef unordered_map<unsigned int, struct ra> ra_map_t;
typedef unordered_map<unsigned int, struct ra>::const_iterator ra_mapcit_t;

// Map caller ra to stats
typedef unordered_map<unsigned int, struct caller> caller_map_t;
typedef unordered_map<unsigned int, struct caller>::const_iterator caller_mapcit_t;
typedef unordered_map<unsigned int, struct caller>::iterator caller_mapit_t;

typedef unordered_map<spid_t, int> reader_map_t;
typedef unordered_map<spid_t, int>::iterator reader_mapit_t;
typedef unordered_map<spid_t, int>::const_iterator reader_mapcit_t;

typedef unordered_map <spid_t, struct spid_info> acq_map_t;
typedef unordered_map<spid_t, struct spid_info>::const_iterator acq_mapcit_t;

#endif


#define LOCK_NAME_SIZE 256

struct lock {
   short state;
   int   lkval;
   bool  queue;
   short lock_id;
   
   unsigned int addr;

   // The generation of this lock address
   int generation;
   
   // The number of worksets this lock has seen
   int workset_count;

   // DEP 2/20/07 - I actually use spid_owner for stuff.  I suppose I
   // don't completely expect everything to work for reader/writer
   // locks just yet

   // This is just debug state, but useful.  NOT SET FOR READ LOCKS.
   // It gets weird because there can be multiple readers at once.
   spid_t spid_owner;

   // The pids that hold the reader lock.  The second term is the
   // number of times this pid has acquired the lock.  A pid can get
   // the read_lock more than once (see do_tty_hangup(), which
   // acquires the tasklist_lock and then calls send_group_sig_info(),
   // which also acquires it).
   reader_map_t readers;

   // Track the aggregate workset of all lock data
   WorkSet* aggregate_workset;

   // For each spid, track transient information about lock acquires.
   // The length of this map is printed in stats as how many pids
   // accessed this lock.
   acq_map_t *acq;

   caller_map_t *callers;

   // The name of this lock
   char name[LOCK_NAME_SIZE];

   // Nesting depth of the locks.  i.e. how many other locks does this
   // process have when it gets this one.  Useful for telling when one
   // lock is "occluding" anothers performance tuning.
   avg_var nest_av[3];

   // worksets covered by previous holders of this lock
   //deque<WorkSet*> worksets;
   //avg_var depend_av[3];      // number of dependent bytes
   //avg_var total_av[3];       // total number of bytes in working set
   //avg_var percent_av[3];     // percent of bytes conflicting
};

#ifdef BUSTED_GCC
typedef map<unsigned int, struct lock> lock_map_t;
typedef map<unsigned int, struct lock>::const_iterator lock_mapcit_t;
typedef map<unsigned int, struct lock>::iterator lock_mapit_t;
typedef map<spid_t, struct spid_info>::const_iterator acqcit_t;

#else
typedef unordered_map<unsigned int, struct lock> lock_map_t;
typedef unordered_map<unsigned int, struct lock>::const_iterator lock_mapcit_t;
typedef unordered_map<unsigned int, struct lock>::iterator lock_mapit_t;
typedef unordered_map<spid_t, struct spid_info>::const_iterator acqcit_t;

#endif

// keep track of all the locks held by each pid, and the worksets for each
typedef list< pair<lock_mapcit_t, WorkSet*> > workset_list_t;
#ifdef BUSTED_GCC
typedef map<spid_t, workset_list_t*> lockset_map_t;
#else
typedef unordered_map<spid_t, workset_list_t*> lockset_map_t;
#endif
typedef lockset_map_t::const_iterator lockset_mapcit_t;
typedef workset_list_t::iterator workset_listit_t;

// A structure to hold the information used during a lock state transition
struct transition_info {
   spid_t spid;
   int lkval;
   int old_lkval;
   unsigned int lock_ra;
   unsigned int caller_ra;
   unsigned int lock_addr;
   unsigned int lock_id;
   short old_state;
   short new_state;
   spid_t spid_owner;
   unsigned long flags;
   bool read_lock;
   bool read_unlock;
   osa_cycles_t now_cyc;
   osa_cycles_t bp_cyc;
   int bp_lkval; // Only used for initialization and rw spinlocks
};

// Define this to log all writes to this address
//#define DBG_WATCH_ADDR 0xf781a530
// Define these to print out information before dbg_lk_addr is
// modified by instruction, and after
//#define DBG_LK_ADDR 0xc0329200 /* 0xc0321880*/

// Magic count of address spaces until rotation
#define AS_COUNT 4096

// Per-address-space data - needed for user-mode profiling.  Although
// keyed by pid, a single as_data_t can be shared across threads in
// the same app.
typedef struct _as_data_t {

   ra_map_t ramap;
   char * map_file_name;
   lock_map_t lockmap;
   vector<breakpoint_id_t> bps;
   lockset_map_t locksetmap;
   WorkSet *asym_detector;
   int ad_count;
   int ref_count;
} as_data_t;

// Map pids to syncchar process data
typedef unordered_map<spid_t, as_data_t*> as_map_t;
typedef unordered_map<spid_t, as_data_t*>::iterator as_mapit_t;
typedef unordered_map<spid_t, as_data_t*>::const_iterator as_mapcit_t;

// Request cycles during a transaction that if aborted,
// will be accounted for instead by osatxm
typedef struct _spcl_caller_t {
   osa_cycles_t  req_cyc;
   osa_cycles_t  total_cyc;
   as_data_t *as_data;
} spcl_caller_t;

// Map transactions to speculative synchronization data.
// txid -> lock addr -> caller -> data (complicated!)
typedef unordered_map<unsigned int, spcl_caller_t> spcl_caller_map_t;
typedef unordered_map<unsigned int, spcl_caller_map_t> lock_spcl_caller_map_t;
typedef unordered_map<unsigned int, lock_spcl_caller_map_t>
   tx_spcl_caller_map_t;
typedef unordered_map<unsigned int, spcl_caller_t>::const_iterator
   spcl_caller_mapcit_t;
typedef unordered_map<unsigned int, spcl_caller_map_t>::const_iterator
   lock_spcl_caller_mapcit_t;
typedef unordered_map<unsigned int, lock_spcl_caller_map_t>::const_iterator
   tx_spcl_caller_mapcit_t;

// keep track of speculative spin data
tx_spcl_caller_map_t spcl_map;


// Information for breakpoint callback.
struct bp_rec {
   unsigned int bp_pc;
   unsigned int caller_ra;
   osa_cycles_t cyc; // Time of callback.  If we take a cache miss on the
                 // lock, we will measure it this way
   // Ugh, need this to get old value of locks the first time they are accessed
   unsigned int bp_lkval;
   // Put the as_data pointer in so that we can find our lockmap
   as_data_t *bp_as_data;
};

// Per-syncchar instance information
typedef struct _syncchar_data_t {

   // Per-process data, where zero is the kernel
   as_map_t as_data;

   unsigned int *exception_addrs;

   // context on which we set our breaks
   conf_object_t *context;

   // number of worksets to archive
   int archived_worksets;

   breakpoint_id_t dbg_bp;

   // Interface to osatxm (if we need to read transactional values)
   osatxm_interface_t *osatxm;
   conf_object_t *osatxm_mod;

   // Disable logging worksets - if one doesn't care about data
   // independence projections and just wants basic lock stats, set
   // this to false
   bool logWorksets;

   // Also disable workset logging before boot to save space
   bool afterBoot;

} syncchar_data_t;

// Wrapper to send reads through osatxm if it is hooked up.  This way
// we get the right data if our address is inside a transaction
inline unsigned int read_4bytes(osamod_t * osamod,
                                osa_cpu_object_t *cpu, 
                                osa_segment_t segment,
                                osa_logical_address_t laddr) {

   if(osamod->syncchar->osatxm != NULL){
      return osamod->syncchar->osatxm->read_4bytes((osamod_t *) osamod->syncchar->osatxm_mod, cpu, segment, laddr);
   } else {
      return osa_read_sim_4bytes(cpu, segment, laddr);
   }
}

static void insert_bps(osamod_t *osamod, as_data_t *as_data) {
   // add breakpoints on the return addresses of all locks
   // Maintain a vector of breakpoint ids in case we want to
   // delete and restore them
   unsigned int lock_ra;
   for(ra_mapcit_t i = as_data->ramap.begin(); 
       i != as_data->ramap.end(); i++) {
      lock_ra = i->first;
      as_data->bps.push_back(
                                      SIM_breakpoint(osamod->syncchar->context,
                                                     Sim_Break_Virtual,
                                                     Sim_Access_Execute,
                                                     (uint64)lock_ra, 1,
                                                     Sim_Breakpoint_Simulation));
   }

#ifdef DBG_WATCH_ADDR
   osamod->syncchar->dbg_bp =
      SIM_breakpoint(osa_get_object_by_name(osamod->syncchar->context),
                     Sim_Break_Virtual,
                     Sim_Access_Write,
                     (uint64)DBG_WATCH_ADDR, 4,
                     Sim_Breakpoint_Simulation);
#endif

}

static void insert_ra(ra_map_t *ramap,
                      unsigned int lock_ra,  // return address of lock op
                      unsigned int lock_off, // addr of sync fn that owns instr
                      unsigned int lock_reg, 
                      unsigned int lock_id, 
                      unsigned int flags,
                      unsigned int func_pc,
                      const char* label) {
   struct ra ra;
   if(ramap->find(lock_ra) != ramap->end()) {
      pr("XXX [sync_char]: multiple lock ras %08x\n", lock_ra);
      cerr << "XXX [sync_char]: multiple ras " << hex << lock_ra
           << dec << endl;
   }
   memset(&ra, 0, sizeof(ra));
   ra.lock_off    = lock_off;
   ra.lock_reg    = lock_reg;
   ra.lock_id     = lock_id;
   ra.flags       = flags;
   ra.label       = strdup(label); // Just leak for now
   (*ramap)[lock_ra] = ra;
}

static void allocate_lock(short lock_id, osa_uinteger_t lock_addr, int lkval, char * label, osamod_t *osamod, as_data_t *as_data);

static void read_ra2sync(FILE* f, osamod_t *osamod, int first_time, as_data_t *as_data) {
   char buf[256];
   char label[256];
   char lock_reg[4];
   unsigned int lock_ra, lock_off, lock_id, flags, func_pc;
   osa_cpu_object_t *cpu = OSA_get_sim_cpu();
   while(!feof(f)) {
      int matched;
      buf[0] = 0;
      if(fgets(buf, 256, f) == NULL)
         break;
      // sscanf might not clear these values if it has a blank line
      lock_ra = lock_off = lock_id = flags = func_pc = 0;
      lock_reg[0] = 0;
      matched = sscanf(buf, "%x %x %3s %d %x %x %255s", 
                       &lock_ra, &lock_off, &lock_reg[0],
                       &lock_id, &flags, &func_pc, &label[0]);
      if(matched != 7 && buf[0] != 0) {
         unsigned int lock_addr = 0;
         int lock_val = 0;
         // We could have a lock def
         matched = sscanf(buf, "%x %d %d %255s",
                          &lock_addr, &lock_id, &lock_val, &label[0]);
                  
         if(matched != 4){
            pr("%s", buf);
            pr("XXX sync_char.map bad record");
         } else {
            // We have a lock definition
            allocate_lock(lock_id, lock_addr, lock_val, label, osamod, as_data);
         }
      } else {
         if(lock_id > 255) {
            pr("XXX [sync_char]: lock_id is > 255.  Ack!");
         }

         // Only set the breakpoints once
         if(!first_time){
            continue;
         }

         // Disable RW Semaphores until we can put more thought into
         // how to instrument them
         //if(lock_id == L_RSEMA || lock_id == L_WSEMA){
         // Disable everything except spins for debugging
         if(lock_id != L_SPIN && lock_id != L_CXA && lock_id != L_CXE){
            continue;
         }
         
         unsigned int lock_reg_int = (unsigned int) -1;
         if(strcmp("nil", lock_reg) != 0) {
            lock_reg_int = SIM_get_register_number(cpu, lock_reg);
         }
         insert_ra(&as_data->ramap, lock_ra, lock_off, lock_reg_int, lock_id, flags, func_pc, 
                   label);
      }
   }
}

int init_ras(osamod_t *osamod, as_data_t *as_data){
   FILE* f = fopen(as_data->map_file_name, "r");
   if(f == NULL) {
      char buf[128];
      perror("Failure to open sync_char mapfile");
      if(getcwd(buf, 128) == NULL)
         fprintf(stderr, "Can't even look up CWD: %d\n", errno);
      else 
         fprintf(stderr, "CWD: %s\n", buf);
      return -1;
   }
   read_ra2sync(f, osamod, as_data->ramap.size() == 0,
                as_data);
   if( fclose(f) < 0 ) {
      perror("fclose");
   }
   return 0;
}

static void update_avgs(avg_var av[3], long double value, double split) {

   // Index 0 holds totals
   av[0].sum += value;
   av[0].xsqr += (value*value);
   av[0].cnt++;

   int i;
   // Index 1 holds average below split, index 2 above
   if(value < split) {
      i = 1;
   } else {
      i = 2;
   }

   av[i].sum += value;
   av[i].xsqr += (value*value);
   av[i].cnt++;
}

static void update_cyc_avgs(avg_var av[3], osa_cycles_t now_cyc, osa_cycles_t start_cyc,
                       osamod_t *osamod) {
   osa_cycles_t cyc;
   if(start_cyc == (osa_cycles_t)0) {
      *osamod->pStatStream << "XXX update_Avg\n";
      // Lock was free & cached, obtained in 1 cycle
      cyc = 1;
   } else {
      cyc = now_cyc - start_cyc;
   }
   
   if(((long double) cyc) < 0){
      conf_object_t * sim = osa_get_object_by_name("sim");
      attr_value_t av = SIM_get_attribute(sim, "cpu_switch_time");

      // We can have a negative number within the margins of cpu
      // switch time.  This can happen under contention during boot.
      // Just make it 1 for simplicity's sake.  If we have a negative
      // number with an absolute value greater than the cpu switch
      // time, we need to be worried.

      if((cyc * -1) > av.u.integer){
         // This is something to be worried about
         cout << "XXX: update_cyc_avgs negative cycles " << cyc 
              << ", now_cyc = " << now_cyc << ", start_cyc = " << start_cyc
              << ", Switch time = " << av.u.integer << endl;
      }
      cyc = 1;
   }


   update_avgs(av, (long double)cyc, (double)1000);
}

static void print_log(const char* str, int param, const struct transition_info* t,
                      osamod_t *osamod) {
   osa_cpu_object_t *cpu = OSA_get_sim_cpu();
   int cpuNum = osamod->minfo->getCpuNum(cpu);
   if( osamod->pStatStream ) { 
      if(t->lock_ra == t->caller_ra) {
         *osamod->pStatStream << str
                              << param
                              << " spid " << t->spid
                              << " lkval " 
                              << hex << t->lkval
                              << " olkval " << t->old_lkval
                              << " bp_lkval " << t->bp_lkval
                              << " lk_ra " << t->lock_ra
                              << "\n\tlk_addr " << t->lock_addr
                              << dec 
                              << " lkid " << t->lock_id
                              << " oldst " << t->old_state
                              << " newst " << t->new_state
                              << " owner " << (int)t->spid_owner
                              << " cpu " << cpuNum
                              << " now " << osa_get_sim_cycle_count(cpu)
                              << endl; 
      } else {
         *osamod->pStatStream << str
                              << param
                              << " spid " << t->spid
                              << " lkval " 
                              << hex << t->lkval
                              << " olkval " << t->old_lkval
                              << " bp_lkval " << t->bp_lkval
                              << " lk_ra " << t->lock_ra
                              << " cal_ra " << t->caller_ra
                              << "\n\tlk_addr " << t->lock_addr
                              << dec 
                              << " lkid " << t->lock_id
                              << " oldst " << t->old_state
                              << " newst " << t->new_state
                              << " owner " << (int)t->spid_owner
                              << " cpu " << cpuNum
                              << " now " << osa_get_sim_cycle_count(cpu)
                              << endl; 
      }
   }
}

// Helper for record_contention
static void _record_contention(spid_t spid, struct lock *lk,
                               struct transition_info *t,  osamod_t *osamod){
   
   // Does this go to user or kernel?
   spid_t my_spid = lk->addr < 0xc0000000 ? spid : 0;
   as_mapit_t iter = osamod->syncchar->as_data.find(my_spid);
   OSA_assert(iter != osamod->syncchar->as_data.end(), osamod);
   as_data_t *as_data = iter->second;

   lockset_mapcit_t lsit = as_data->locksetmap.find(spid);

   if(lsit == as_data->locksetmap.end()){
      print_log("XXX locked out but empty worksets", 0, t, osamod);
#ifdef DEBUG_INTERACTIVE      
      SIM_break_simulation("XXX");
#endif
      return;
   }

   // Store a reference to the address sets of the current execution
   // of the critical section.  (Why can there be more than one?) We
   // will process this when we release the lock.

   workset_list_t *worksets = lsit->second;
   workset_listit_t wsit;
   int check = 0;
   for(wsit = worksets->begin(); wsit != worksets->end(); wsit++){
      if(wsit->second->id.lock_addr == t->lock_addr){

         // We shouldn't have more than one workset open for the same lock
         if(check > 0){
            *osamod->pStatStream << "XXX More than one open workset for same lock" << endl;
#ifdef DEBUG_INTERACTIVE      
            SIM_break_simulation("XXX");
#endif
         }

         // Make sure we don't add the same lock more than once when
         // we are spinning
         for( vector<WorksetID>::iterator iter = 
                 (*lk->callers)[t->caller_ra].contended_worksets.begin();
              iter != (*lk->callers)[t->caller_ra].contended_worksets.end();
              iter++){
            if(*(iter) == wsit->second->id){
               goto NEXT_LOOP; // break only goes up one loop.  grrr...
            }
         }
         
         (*lk->callers)[t->caller_ra].contended_worksets.push_back(wsit->second->id);
         
         // Increment q_count 
         // Do it here rather than every time we call so that we don't
         // get a false bias. 
         (*lk->callers)[t->caller_ra].q_count++;

      NEXT_LOOP:
         check++;
      }
   }
   if(check == 0){
      print_log("XXX record contention found no open worksets for lock", 0, t, osamod);
#ifdef DEBUG_INTERACTIVE      
      SIM_break_simulation("XXX");
#endif
      return;
   }
}

static void record_contention(struct lock *lk, struct transition_info *t, osamod_t *osamod){

   if(!osamod->syncchar->logWorksets) {
      return;
   }

   // DEP 2/20/07 - lock->worksets is the archive.  The first element
   // is not the current workset.  We have to get this from
   // locksetmap, indexed on spid.
   if(lk->spid_owner == (spid_t) -1
      && lk->state == LKST_RLK){
      // If we are a reader/writer lock that is read-locked, owner
      // should be -1.  In this case, there can be beaucoup readers.
      // Iterate over all of them using the readers list rather than
      // spid_owner.

      // Assert that there are some readers
      if(lk->readers.empty()){
         print_log("XXX rw lock read-locked, but no readers", 0, t, osamod);
#ifdef DEBUG_INTERACTIVE      
         SIM_break_simulation("XXX");
#endif
      }

      for(reader_mapcit_t reader = lk->readers.begin();
          reader != lk->readers.end(); reader++){
         _record_contention(reader->first, lk, t, osamod);
      }
   } else {
      _record_contention(lk->spid_owner, lk, t, osamod);
   }
}


static unsigned int get_lock_addr(ra_mapcit_t raci, osa_cpu_object_t *cpu) {
   unsigned int lock_addr = raci->second.lock_off;
   if( raci->second.lock_reg != -1U ) {
      lock_addr += _osa_read_register(cpu, raci->second.lock_reg);
   }

   return lock_addr;
}
static void zero_av(avg_var av[3]) {
   for(int i = 0; i < 3; ++i) {
      av[i].cnt  = 0;
      av[i].sum  = 0.0;
      av[i].xsqr = 0.0;
   }
}
static void zero_spid_info(struct spid_info* spi) {
   memset(spi, 0, sizeof(*spi));
}
static void unlock_spid_info(struct spid_info* spi, 
                             const struct transition_info* t, osamod_t *osamod) {
   if(--spi->cnt == 0) {
      spi->acq_cyc = 0ULL;
      spi->acq_ra  = 0;
   }
#ifdef DBG_LK_ADDR
   if(t->lock_addr == DBG_LK_ADDR) {
      print_log("unlock cnt ", spi->cnt, t);
   }
#endif
   if(spi->cnt < 0) {
      print_log("XXX spi->cnt ", spi->cnt, t, osamod);
#ifdef DEBUG_INTERACTIVE      
      SIM_break_simulation("XXX");
#endif
      // Try to avoid many repetitions of the same error
      spi->cnt = 0;
   }
}
static void lock_spid_info(struct spid_info* spi,
                           const struct transition_info* t,
                           as_data_t *as_data) {
   if(t->new_state != LKST_CXA && spi->cnt++ == 0) {
      // Recursive acquires stay with the first acquire
      spi->acq_cyc = t->now_cyc;
      spi->acq_ra  = t->caller_ra;
   }
   // Every lock means our request is over
   struct lock* lk = &as_data->lockmap[t->lock_addr];
   (*lk->acq)[t->spid].req_cyc = 0ULL;
#ifdef DBG_LK_ADDR
   if(t->lock_addr == DBG_LK_ADDR)
      print_log("  lock cnt ", spi->cnt, t);
#endif
}
static void caller_zero(struct caller* caller) {
   // New benchmark, new acquire latencies, even if lock is in progress
   caller->count  = 0ULL;
   caller->q_count = 0;
   /*
   caller->q_count_dependent = 0;
   caller->q_count_independent = 0;
   caller->q_count_dependent_total_bytes = 0;
   caller->q_count_dependent_conflicting_bytes = 0;
   */
   caller->useless_release = 0;
   zero_av(caller->acq_av);
   zero_av(caller->hold_av);
   
   // Go ahead and dump the contended worksets for each lock
   caller->contended_worksets.clear();
}

static void dbg_breakpoint_callback(conf_object_t *trigger_obj,
                                    lang_void* _bp_rec) {
#ifdef DBG_LK_ADDR
   struct bp_rec *bp_rec = (struct bp_rec*)_bp_rec;
   osa_cpu_object_t *cpu = OSA_get_sim_cpu();
   int cpuNum = osamod->minfo->getCpuNum(cpu);
   MM_FREE(bp_rec);
   bp_rec = 0;
   spid_t spid = current_process[cpuNum];
   int lkval = read_4bytes(osamod, cpu, DATA_SEGMENT, DBG_LK_ADDR);
   *pStatStream << "DBG PC " << hex 
                << SIM_get_program_counter(cpu)
                << " lkval " << lkval
                << dec 
                << " spid " << spid
                << endl;
#endif
}

static void print_lock(ostream *stat_str, unsigned int lock_addr,
                       const struct lock *lock, as_data_t *as_data);


static void allocate_lock(short lock_id, osa_uinteger_t lock_addr, int lkval, 
                          char * label, osamod_t *osamod, 
                          as_data_t *as_data){
   int generation = 0;

   // Log the old lock if there is already one here
   lock_mapit_t iter = as_data->lockmap.find(lock_addr);
   if( iter != as_data->lockmap.end() ) {
      
      // Print the old lock out to the log
      struct lock *old_lock = &(iter->second);
      print_lock(osamod->pStatStream, lock_addr, old_lock, as_data);

      // Get the old lock's generation number, increment
      generation = old_lock->generation + 1;

      // Clean up the memory
      delete old_lock->acq;
      delete old_lock->callers;
      delete old_lock->aggregate_workset;

      as_data->lockmap.erase(iter);
   }

   // New lock address
   struct lock lock;
   lock.state = LKST_OPEN;
   lock.queue = false;
   lock.lkval = lkval,
   lock.lock_id = lock_id;
   lock.spid_owner = (spid_t)-1;
   lock.generation = generation;
   lock.workset_count = 0;
   lock.acq = new acq_map_t();
   // Allocate array of callers of this lock address
   lock.callers = new caller_map_t();
   lock.addr = lock_addr;

   memset(lock.name, 0, LOCK_NAME_SIZE);
   if(label != NULL){
      strcpy(lock.name, label);
   }
   lock.aggregate_workset = new WorkSet(lock_addr, 0, generation, 0xffffffff, 0);
   zero_av(lock.nest_av);
   /*
   zero_av(lock.depend_av);
   zero_av(lock.total_av);
   zero_av(lock.percent_av);
   */
   as_data->lockmap[lock_addr] = lock;
}

static void initialize_transition(struct transition_info* t,
                                  struct bp_rec* bp_rec,
				                      osamod_t *osamod,
                                  as_data_t *as_data) {

   osa_cpu_object_t *cpu = OSA_get_sim_cpu();
   int cpuNum = osamod->minfo->getCpuNum(cpu);
   // Initialize with garbage to make failures more spectacular
   memset(t, 0xAF, sizeof(*t));
   t->lock_ra   = bp_rec->bp_pc;
   t->caller_ra = bp_rec->caller_ra;
   t->bp_cyc    = bp_rec->cyc;
   t->bp_lkval  = bp_rec->bp_lkval;
   MM_FREE(bp_rec);
   bp_rec = 0;
   t->spid = osamod->os->current_process[cpuNum];
   // Keep track of lock state
   t->now_cyc = osa_get_sim_cycle_count(cpu);
   ra_mapcit_t raci = as_data->ramap.find(t->lock_ra);
   t->lock_id = raci->second.lock_id;
   t->lock_addr = get_lock_addr(raci, cpu);
   t->flags = raci->second.flags;
   if( t->flags & F_NOADDR ) {
      // Store all RCU records at 1
      t->lock_addr = 1;
   }
   t->lkval = 0xdeadbeef;

   // OSH cx_ special cased until I find the true meaning of
   // F_EAX (and christmas)
   if(t->lock_id == L_CXA || t->lock_id == L_CXE) {
      t->lkval = read_4bytes(osamod, cpu, DATA_SEGMENT, t->lock_addr);
   }
   else if( (t->flags & F_NOADDR) == 0 && (t->flags & F_EAX) == 0 ) {
      t->lkval = read_4bytes(osamod, cpu, DATA_SEGMENT, t->lock_addr);
   }
   if( as_data->lockmap.find(t->lock_addr) == as_data->lockmap.end() ) {
      allocate_lock(t->lock_id, t->lock_addr, t->bp_lkval, NULL, osamod, as_data);

      if(t->lock_id == L_SPIN || t->lock_id == L_CXE || t->lock_id == L_CXA){
         *osamod->pStatStream << "XXX: Noname spinlock: " << std::hex << t->lock_addr << std::dec << endl;
         OSA_stack_trace(OSA_get_sim_cpu(), osamod);
#ifdef DEBUG_INTERACTIVE      
         SIM_break_simulation("XXX");
#endif
      }

      struct caller caller;
      caller_zero(&caller);
      (*as_data->lockmap[t->lock_addr].callers)[t->caller_ra] = caller;
   }
   if( as_data->lockmap[t->lock_addr].callers->find(t->caller_ra) ==
         as_data->lockmap[t->lock_addr].callers->end() ) {
      caller_zero(&(*as_data->lockmap[t->lock_addr].callers)[t->caller_ra]);
   }

   t->old_state     = as_data->lockmap[t->lock_addr].state;
   struct lock* lk = &as_data->lockmap[t->lock_addr];
   t->spid_owner    = lk->spid_owner;

   // Record old lkval and update with new value
   t->old_lkval     = as_data->lockmap[t->lock_addr].lkval;
   as_data->lockmap[t->lock_addr].lkval = t->lkval;
   // Allocate and zero spid_info record
   if((*lk->acq).find(t->spid) == (*lk->acq).end()) {
      struct spid_info spi;
      zero_spid_info(&spi);
      (*lk->acq)[t->spid] = spi;
   }

}
static short update_current_lock_state(const struct transition_info* t,
                                       osamod_t *osamod, as_data_t *as_data) {

   struct lock* lk = &as_data->lockmap[t->lock_addr];
   // Update current state
   lk->queue = false;  // Is there anyone waiting?
   unsigned int ebx, ecx, edx;
   osa_cpu_object_t *cpu;
   switch(t->lock_id) {
   case L_SEMA:
      // up() is a bit of a special case.  It always releases the lock
      // by incrementing its value, even if it goes from -1 to 0.
      if(((t->flags & F_UNLOCK) != 0)
         && (t->lkval > t->old_lkval)){
         lk->state = LKST_OPEN;
         break;
      }
   case L_MUTEX:
   case L_COMPL:
      if( t->lkval < 1 ){
         lk->state = LKST_WRLK;
         if(t->lock_id == L_MUTEX
            || t->lock_id == L_SEMA){
            // Any time a semaphore or mutex goes from locked->locked,
            // assume we are waiting
            lk->queue = true;
         }
      } else {
         lk->state = LKST_OPEN;
      }
      break;
   case L_RSEMA:
   case L_WSEMA:
      if( t->lkval == 0 ) {
         lk->state = LKST_OPEN;
      } else if( t->lkval == (int)0xFFFF0001 ) {
         lk->state = LKST_WRLK;
      } else if( t->lkval > 0 ) {
         lk->state = LKST_RLK;
      } else {
         lk->state = t->old_state;
         lk->queue = true;
      }
      break;
   case L_SPIN:
      if( t->lkval == 1 ) {
         lk->state = LKST_OPEN;
         // spin lock is a byte, so less than 255
      } else if( t->lkval == 0 || (t->lkval > 1 && t->lkval <= 255 )) {
         lk->state = LKST_WRLK;
         if( t->lkval > 1 && t->lkval <= 255 ) lk->queue = true;
      } else {
         pr("XXX spin lock with value %d lk_ra %#x cal_ra %#x lk_addr %#x\n", 
            t->lkval, t->lock_ra, t->caller_ra, t->lock_addr);
         print_log("XXX slock val", 0, t, osamod);
#ifdef DEBUG_INTERACTIVE      
         SIM_break_simulation("XXX");
#endif
      }
      break;
   case L_RSPIN:
   case L_WSPIN:

      // Unlock in reader/writer locks is tricky, hence we split them out
      if(((t->flags & F_UNLOCK) != 0)){

         if(t->lock_id == L_RSPIN){
            // If we are a read unlock, we are open iff the value mod
            // 0x10000000 is zero (e.g. 0, 0x10000000, -0x1000000, etc).
            // Otherwise, we are just one of a number of readers
            // releasing the lock
            
            if(t->lkval % 0x01000000 == 0){
               lk->state = LKST_OPEN;
            } else {
               lk->state = LKST_RLK;
            }

         } else {
            // If we are a write unlock, we always unlock.  Period.  
            // We can assert that the lock val grew just because the
            // number of asserts is proportional to the number of
            // correctnesses, right?
            lk->state = LKST_OPEN;

            if(t->lkval <= t->bp_lkval){
               print_log("XXX write spinlock unlock yielded a lower value!!!", 0, t, osamod);
            }
         }
      } else { // Lock routine

         if( t->lkval == 0x01000000) {
            lk->state = LKST_OPEN;
         } else if( t->lkval == 0 ) {
            // t->lkval == 0 when lock is RWLOCK, it goes negative
            // temporarily when readers try to enter 
            lk->state = LKST_WRLK;
         } else if( t->lkval > 0 ) {
            lk->state = LKST_RLK;
         } else {
            // t->lkval < 0
            lk->state = t->old_state;
            lk->queue = true;
         }
      }
      break;
   case L_CXA:
      // read the value of edx
      cpu = OSA_get_sim_cpu();
      ebx = osa_read_register(cpu, regEBX);
      ecx = osa_read_register(cpu, regECX);
      edx = osa_read_register(cpu, regEDX);
      if(edx) {
         if(ebx == ecx) {
            // This is an xtest, thus a transaction
            // set the lock to a transient state indicating
            // a transaction is entering
            lk->state = LKST_CXA;
            if(t->lkval != 1 || t->old_state != LKST_OPEN) {
               print_log("XXX cx_exclusive result/lkval/old_state inconsistency!",
                     0, t, osamod);
            }
         }
         else {
            // this is an xcas, thus _exclusive inside _atomic
            lk->state = LKST_WRLK;
            if(t->lkval != 0) {
               print_log("XXX cx_exclusive result/lkval inconsistency!",
                     0, t, osamod);
            }
         }
      }
      break;
   case L_CXE:
      if(t->flags & F_UNLOCK) {
         if(t->lkval != 1 || t->old_state != LKST_WRLK) {
            print_log("XXX cx_end result/lkval/old_state inconsistency!",
                  0, t, osamod);
         }
         lk->state = LKST_OPEN;
      }
      else {
         // read the value of edx
         edx = osa_read_register(OSA_get_sim_cpu(), regEDX);
         if(edx) {
            lk->state = LKST_WRLK;
            if(t->lkval != 0) {
               print_log("XXX cx_exclusive result/lkval inconsistency!",
                     0, t, osamod);
            }
         }
      }
      break;
   default:
      *osamod->pStatStream << "UNK " << hex << t->lkval << dec << endl;
   }
   return lk->state;
}
// Must be done after new_state is initialized
static void detect_read_lock_unlock(struct transition_info *t,
                                    osamod_t *osamod) {
   // Detect read locks/unlocks unlocks by examining the state of the
   // lock and what kind of reader/writer lock it is.  We need to
   // distinguish based on r/w flavor of lock_id, so we need to get it
   // right.

   // read_lock and read_unlock can both are false if it was not a
   // read lock/unlock.
   t->read_lock = false;
   t->read_unlock = false;
   int  lk_incr = 0;
   if(t->new_state != LKST_OPEN && t->new_state != LKST_RLK && 
      t->new_state != LKST_WRLK && t->new_state != LKST_CXA) {
      print_log("XXX detect_read called early newst ", t->new_state, t, osamod);
#ifdef DEBUG_INTERACTIVE      
      SIM_break_simulation("XXX");
#endif
   }

   if(t->new_state == LKST_OPEN
      || (t->old_state == t->new_state
          && t->old_state == LKST_RLK)) {
      if(t->lock_id == L_RSPIN || t->lock_id == L_WSPIN) {
         // Read spin locks count down

         // RW spin locks should also use the bp_lkval, as there are
         // untracked compensating modifications of the lock value on
         // the slowpath

         // Moreover, the method of just looking for a change of 1
         // doesn't work if two read_locks occur concurrently.  You
         // get a transition from 0x01000000 -> 0x00ffffff, and one
         // from 0x0100000 -> 0x00ffffffe.

         if(t->lock_id == L_RSPIN){
            if((t->flags & F_UNLOCK) != 0
               && t->lkval >= t->bp_lkval){
               t->read_unlock = true;
            }

            if(t->lkval > 0 && t->lkval < 0x01000000
               && (t->flags & F_LOCK) != 0){
               t->read_lock = true;
            }            
         }

         return;
      }
      if(t->lock_id == L_RSEMA || t->lock_id == L_WSEMA) {
         // Read semapohres cout up
         lk_incr = 1;
      }
   }

   int diff = t->lkval - t->old_lkval;
   if( diff == lk_incr ) {
      t->read_lock = true;
   } else if( diff == -lk_incr ) {
      t->read_unlock = true;
   }
}

// Encapsulate special case where a runqueue lock changes owners along
// the way
static lockset_mapcit_t get_lsit(spid_t spid, const char *name, spid_t spid_owner, 
                                 as_data_t *as_data){
   lockset_mapcit_t lsit = as_data->locksetmap.find(spid);

   /* DEP 1/20/06 - Special case of runqueue lock.  Lock is
    * acquired by one process and released by another.  This is ok
    * in this special case, but in other cases indicates a bug in
    * syncchar.
    *
    * To handle this, we attribute the workset to the lock that
    * acquired it (spid_owner)
    *
    */
   if(0== strncmp(name, "runqueue_t->lock", LOCK_NAME_SIZE)){
      lsit = as_data->locksetmap.find(spid_owner);
   }

   return lsit;
}

// Keep the algorithmically interesting part of the old code around
// until we get the post-processing stage done
#if 0
static void archive_workset(workset_list_t *worksets, workset_listit_t wsit,
      struct transition_info *t, osamod_t *osamod) {

   // remove workset from list
   WorkSet *ws = wsit->second;
   worksets->erase(wsit);

   // compare to the previous group of worksets for this lock
   struct lock *lk = &osamod->syncchar->lockmap[t->lock_addr];
   deque<WorkSet*>::const_iterator prevws_it;
   for(prevws_it = lk->worksets.begin(); prevws_it != lk->worksets.end();
         prevws_it++) {
      WorkSet *other = *prevws_it;
      // Filter comparisons to locks in same workset
      if(other->pid == ws->pid){
         continue;
      }

      // Filter second pid if we are a runqueue lock
      if(other->twoowners && ws->twoowners
         && other->old_pid == ws->old_pid){
         continue;
      }
      if(other->twoowners
         && other->old_pid == ws->pid){
         continue;
      }
      if(ws->twoowners
         && ws->old_pid == other->pid){
         continue;
      }

      
      int depends, size;
      depends = ws->compare(other, &size);
      update_avgs(lk->depend_av, (long double)depends, (double)0.5);
      update_avgs(lk->total_av, (long double)size, (double)100);
      update_avgs(lk->percent_av,
                  size > 0 ? (long double)depends / (long double)size : 0, 0.1);
   }

   // place workset on queue for this lock
   lk->worksets.push_front(ws);
   if(lk->worksets.size() > (unsigned int)(osamod->syncchar->archived_worksets)) {
      WorkSet *oldws = lk->worksets.back();
      lk->worksets.pop_back();
      if(oldws->refs == 1){
         delete oldws;
      } else {
         oldws->refs--;
      }
   }

}
// #if 0
#endif

static void close_workset(struct transition_info *t, osamod_t *osamod, 
                          as_data_t *as_data) {
   syncchar_data_t *syncchar = osamod->syncchar;

   // the lock has been unlocked, so remove the workset
   // from the current spid, and place it in the list
   // of old worksets covered by this lock

   lock_mapcit_t lock_cit = as_data->lockmap.find(t->lock_addr);
   lockset_mapcit_t lsit = get_lsit(t->spid, lock_cit->second.name,
                                    t->spid_owner, as_data);

   if(lsit == as_data->locksetmap.end()) {
      print_log("XXX tried to close workset on non-existant spid",
                0, t, osamod);
#ifdef DEBUG_INTERACTIVE      
      SIM_break_simulation("XXX");
#endif
      return;
   }

   // find an open workset for this spid and lock
   workset_list_t *worksets = lsit->second;
   workset_listit_t wsit;
   int found = 0;
   for(wsit = worksets->begin(); wsit != worksets->end(); wsit++) {
      if(wsit->first == lock_cit) {
         found = 1;
         
         if(--(wsit->second->cnt) <= 0){
            // Don't archive it - just log and delete
            //archive_workset(worksets, wsit, t, osamod);
            WorkSet *ws = wsit->second;
            if(syncchar->logWorksets
               && syncchar->afterBoot){
               ws->logWorkset(*osamod->pStatStream);
            }
            worksets->erase(wsit);
            delete ws;
         }
         break;
      }
   }

   if(!found){
      print_log("XXX unable to find workset to close", 0, t, osamod);
#ifdef DEBUG_INTERACTIVE      
      SIM_break_simulation("XXX");
#endif
   }

   // Increment the workset count and rotate the asym detector if needed
   as_data->ad_count++;

   if(as_data->ad_count > AS_COUNT){
      unsigned int lock_addr = as_data->asym_detector->id.lock_addr;
      as_data->ad_count = 0;
      if(syncchar->logWorksets 
         && syncchar->afterBoot)
         as_data->asym_detector->logWorkset(*osamod->pStatStream);
      delete as_data->asym_detector;
      as_data->asym_detector = new WorkSet(lock_addr, t->spid, 0, 0xffffffff, 0);
   }
}

static void process_unlock(struct transition_info* t, osamod_t *osamod,
                           as_data_t *as_data) {

   syncchar_data_t *syncchar = osamod->syncchar;

   struct lock* lk = &as_data->lockmap[t->lock_addr];

   // If the lock was reader-locked, we can remove ourselves from the
   // reader list.  Because we can have multiple calls of a reader
   // lock we must check for this, effectively ignoring the nested
   // acquires (i.e. flat nesting in transaction parlance)
   if(t->old_state == LKST_RLK){
      reader_mapit_t rdr = lk->readers.find(t->spid);
      if(rdr == lk->readers.end()){
         print_log("XXX reader unlock missing reader lock ", 0, t, osamod);
#ifdef DEBUG_INTERACTIVE      
         SIM_break_simulation("XXX");
#endif
      } else if(lk->readers[t->spid] > 1){
         // Just decrement the count if we have multiple acquires of
         // the same read lock
         lk->readers[t->spid]--;
         // Don't do the other stuff until we get to the outermost acquire
         return;
      } else {
         lk->readers.erase(rdr);
      }
   }


   osa_cycles_t acq_cyc = (*lk->acq)[t->spid].acq_cyc;
   unsigned int acq_ra = (*lk->acq)[t->spid].acq_ra;
   spid_t acq_spid = (spid_t)-1;
   if( acq_ra == 0) {
      // DEP Fixme: I don't think we need this code anymore, but leave
      // it alone for now.  Once we get everything else working, let's
      // try to dump it.

      // Another process acquired the spin lock, e.g.,
      // sched.c:304.  Look for it and hope there is only 1
      int nfound = 0;
      for(acq_mapcit_t it = lk->acq->begin(); it != lk->acq->end(); ++it) {
         if( it->first != t->spid
             && it->second.acq_cyc != 0ULL 
             && it->second.acq_ra != 0 ) {
            // Some process who is not us acquired the lock we are releasing
            nfound++;
            acq_cyc = it->second.acq_cyc;
            acq_ra = it->second.acq_ra;
            acq_spid = it->first;
         }
      }
      if(nfound != 1) {
         print_log("XXX acq nfound ", nfound, t, osamod);
         //SIM_break_simulation("nfound");
#ifdef DEBUG_INTERACTIVE      
         SIM_break_simulation("XXX");
#endif
      }
   }
   if(lk->state != LKST_OPEN){
      if(!(t->old_state == lk->state
           && t->old_state == LKST_RLK)) {
         print_log("XXX nopen ", 0, t, osamod);
#ifdef DEBUG_INTERACTIVE      
         SIM_break_simulation("XXX");
#endif
      }
      // owner ok
      //((*lk->acq)[lk->spid_owner].acq_cyc != 0ULL
      //&& (*lk->acq)[lk->spid_owner].acq_ra != 0)
   }
   // Lock release.  It doesn't matter if it made lock available
   // Only do it if we know acquire, otherwise it will throw off
   // stats. 
   update_cyc_avgs((*lk->callers)[acq_ra].hold_av, t->now_cyc, acq_cyc, 
              osamod);
   if(acq_spid != (spid_t)-1) {
      // Change spid in our local copy
      struct transition_info _t = *t;
      _t.spid = acq_spid;
      // Clear our acquirer
      unlock_spid_info(&(*lk->acq)[acq_spid], &_t, osamod);
      if( lk->state == LKST_OPEN
          && (*lk->acq)[acq_spid].cnt > 0 ) {
         print_log("XXX Cnt Higha cnt ", (*lk->acq)[acq_spid].cnt, &_t, osamod);
#ifdef DEBUG_INTERACTIVE      
         SIM_break_simulation("XXX");
#endif
      }
   } else {
      unlock_spid_info(&(*lk->acq)[t->spid], t, osamod);
      if( lk->state == LKST_OPEN
          && (*lk->acq)[t->spid].cnt > 0 ){
         print_log("XXX Cnt High cnt ", (*lk->acq)[t->spid].cnt, t, osamod);
#ifdef DEBUG_INTERACTIVE      
         SIM_break_simulation("XXX");
#endif
      }
   }
   // Only clear owner if the lock is open
   if(lk->state == LKST_OPEN) {
      lk->spid_owner = (spid_t)-1;
   }

   // If we have a non-zero contended worksets vector, we experienced contention
   if((*lk->callers)[acq_ra].contended_worksets.size() > 0){

      int cpuNum = osamod->minfo->getCpuNum(OSA_get_sim_cpu());
      spid_t spid = osamod->os->current_process[cpuNum];
      
      lock_mapcit_t lock_cit = as_data->lockmap.find(t->lock_addr);
      lockset_mapcit_t lsit = get_lsit(spid, lock_cit->second.name,
                                       t->spid_owner, as_data);

      if(lsit == as_data->locksetmap.end()) {
         *osamod->pStatStream << "XXX no open workset in process_unlock\n";
#ifdef DEBUG_INTERACTIVE      
         SIM_break_simulation("XXX");
#endif
         return;
      }
      
      // DEP 2/21/07 - There can be more than one open workset.  Since
      // worksets are associated with a lock addr, find the one that
      // matches.
      WorkSet *ws = NULL;
      workset_list_t *worksets = lsit->second;
      workset_listit_t wsit;
      
      for(wsit = worksets->begin(); wsit != worksets->end(); wsit++){
         if(wsit->second->id.lock_addr == t->lock_addr){
            if(ws == NULL){
               ws = wsit->second;
            } else {
               *osamod->pStatStream << "XXX More than one open ws for same lock!" << endl;
#ifdef DEBUG_INTERACTIVE      
               SIM_break_simulation("XXX");
#endif
            }
         }
      }

      if(ws == NULL){
         *osamod->pStatStream << "XXX No matching workset for lock being closed, "
                              << std::hex << t->lock_addr << std::dec << endl;
#ifdef DEBUG_INTERACTIVE      
         SIM_break_simulation("XXX");
#endif
      } else {

         // Copy the contended worksets list into the workset itself
         for( vector<WorksetID>::iterator iter = (*lk->callers)[acq_ra].contended_worksets.begin();
              iter != (*lk->callers)[acq_ra].contended_worksets.end(); iter++){

            ws->contended_worksets.push_back(*iter);
         }

#if 0
         // Iterate over working sets and determine if we are dependent or not
         for( vector<WorksetID>::iterator iter = (*lk->callers)[acq_ra].contended_worksets.begin();
              iter != (*lk->callers)[acq_ra].contended_worksets.end(); iter++){
            
            int size = 0;
            int dependent = 0;
            
            dependent = ws->compare( *iter, &size);

            if(dependent){ 
               (*lk->callers)[acq_ra].q_count_dependent++;
               (*lk->callers)[acq_ra].q_count_dependent_total_bytes += size;
               (*lk->callers)[acq_ra].q_count_dependent_conflicting_bytes += dependent;
            } else {
               (*lk->callers)[acq_ra].q_count_independent++;
            }
            
            if( (*iter)->refs <= 1){
               delete (*iter);
            } else {
               (*iter)->refs--;
            }
         }
#endif      
      }

      // Drop old contended worksets entries
      (*lk->callers)[acq_ra].contended_worksets.clear();
   }

   if(syncchar->logWorksets) {
      close_workset(t, osamod, as_data);
   }
}

static void open_workset(struct transition_info *t, osamod_t *osamod, 
                         as_data_t *as_data) {

   // this lock has been locked.  Make sure that the spid - workset
   // map has both the current spid and a workset for this lock

   // check if this spid is in the map
   lock_mapit_t lock_it  = as_data->lockmap.find(t->lock_addr);
   lock_mapit_t lock_cit = as_data->lockmap.find(t->lock_addr);
   lockset_mapcit_t lsit = as_data->locksetmap.find(t->spid);
   int cpu = SIM_get_processor_number(OSA_get_sim_cpu());

   // if not, insert with an empty workset
   if(lsit == as_data->locksetmap.end()) {
      as_data->locksetmap[t->spid] =
         new workset_list_t(1, make_pair(lock_cit, new WorkSet(t->lock_addr, t->spid,
                                                               lock_cit->second.generation,
                                                               lock_cit->second.workset_count,
                                                               cpu)));
      // Increment the workset count
      lock_it->second.workset_count++;
      // Update nesting averages
      update_avgs(lock_it->second.nest_av, 0, 1);
      return;
   }

   // check that this lock does not already have an open workset
   workset_list_t *worksets = lsit->second;
   workset_listit_t wsit;
   for(wsit = worksets->begin(); wsit != worksets->end(); wsit++) {
      if(wsit->first == lock_cit) {
         print_log("XXX multiple workset opens (this might be okay)",
               0, t, osamod);
#ifdef DEBUG_INTERACTIVE      
         SIM_break_simulation("XXX");
#endif
         wsit->second->cnt++;
         return;
      }
   }

   // Update nesting averages
   update_avgs(lock_it->second.nest_av, worksets->size(), 1);

   // create a new workset for the current lock
   worksets->push_front(make_pair(lock_cit, new WorkSet(t->lock_addr, t->spid,
                                                        lock_cit->second.generation,
                                                        lock_cit->second.workset_count, 
                                                        cpu)));
   // Increment the workset count
   lock_it->second.workset_count++;

}

// get the speculative lock data for a given transaction and
// lock address, or return a blank structure
static spcl_caller_t get_speculative_lock(int txid, unsigned int addr,
                                          unsigned int caller, as_data_t *as_data) {
   spcl_caller_t result;
   spcl_caller_map_t lsmap = spcl_map[txid][addr];
   spcl_caller_mapcit_t scit = lsmap.find(caller);
   if(scit == lsmap.end()) {
      result.req_cyc = 0ULL;
      result.total_cyc = 0ULL;
   }
   else {
      result = scit->second;
   }
   return result;
}

static void process_transition(struct transition_info *t, osamod_t *osamod, 
                               as_data_t *as_data) {
   syncchar_data_t *syncchar = osamod->syncchar;

   struct lock* lk = &as_data->lockmap[t->lock_addr];
   switch(t->old_state) {
   case LKST_OPEN: // old_state
      switch(lk->state) {
      case LKST_OPEN:
         if(t->lock_id == L_CXA || t->lock_id == L_CXE) {
            // unlocked->unlocked is possible for acquiring locks
            // with XCAS, because it waits for transactions that
            // might have the lock.  But, we have to not be 
            // in a transaction
            int txid = 0;
            txid = syncchar->osatxm->get_current_transaction(
                     (osamod_t*)syncchar->osatxm_mod, OSA_get_sim_cpu());
            if(txid == 0) {
               // this is actually like a spin (LKST_WRLK->LKST_WRLK)
               if((t->flags & F_TRYLOCK) == 0
                  && (*lk->acq)[t->spid].req_cyc == (osa_cycles_t)0) {
                  (*lk->acq)[t->spid].req_cyc = t->bp_cyc;
                  record_contention(lk, t, osamod);
               }
               break;
            }
         }
         // unlocked -> unlocked, huh?
         (*lk->callers)[t->caller_ra].useless_release++;
         print_log("XXX useless ", 0, t, osamod);
#ifdef DEBUG_INTERACTIVE      
         SIM_break_simulation("XXX");
#endif
         break;
      case LKST_RLK:
      case LKST_WRLK:
      case LKST_CXA:
         {
            if(lk->state == LKST_WRLK) {
               if(lk->spid_owner == (spid_t)-1) {
                  lk->spid_owner = t->spid;
               } else {
                  print_log("XXX OWNER ", 0, t, osamod);
#ifdef DEBUG_INTERACTIVE      
                  SIM_break_simulation("XXX");
#endif
               }
            } else if(lk->state == LKST_RLK) {
               // Push our pid on the reader list, if it isn't
               // already.  Otherwise, increment the count
               reader_mapit_t rdr = lk->readers.find(t->spid);
               if(rdr == lk->readers.end()){
                  lk->readers[t->spid] = 1;
               } else {
                  lk->readers[t->spid]++;
                  // Don't do the other bookkeeping on nested acquires
                  // of a read lock
                  break;
               }
            }

            // if this acquire is in a transaction, add a speculative
            // cycle count to avoid double-counting with aborts
            osa_cycles_t req_cyc = t->bp_cyc;
            int txid = 0;
            if(syncchar->osatxm != NULL) {
               txid = syncchar->osatxm->get_current_transaction(
                     (osamod_t*)syncchar->osatxm_mod, OSA_get_sim_cpu());
            }
            if(txid == 0 && lk->state == LKST_CXA && t->flags & F_LOCK) {
               print_log("XXX cx_atomic in a non-transaction?", 0, t, osamod);
            }
            else if(txid != 0 && t->lock_id == L_CXE) {
               print_log("XXX cx_exclusive in a transaction?", 0, t, osamod);
            }
            if(syncchar->osatxm != NULL) {
               txid = syncchar->osatxm->get_current_transaction(
                     (osamod_t*)syncchar->osatxm_mod, OSA_get_sim_cpu());
            }
            if(txid == 0) {
               if((*lk->acq)[t->spid].req_cyc != 0ULL) {
                  req_cyc = (*lk->acq)[t->spid].req_cyc;
               }
               update_cyc_avgs((*lk->callers)[t->caller_ra].acq_av, t->now_cyc,
                          req_cyc, osamod);
            }
            else {
               spcl_caller_t scaller = get_speculative_lock(txid, t->lock_addr,
                                                            t->caller_ra, as_data);
               osa_cycles_t req_cyc = t->bp_cyc;
               if(scaller.req_cyc != 0ULL) {
                  req_cyc = scaller.req_cyc;
                  scaller.req_cyc = 0ULL;
               }
               scaller.total_cyc += t->now_cyc - req_cyc;
               spcl_map[txid][t->lock_addr][t->caller_ra] = scaller;
            }

            lock_spid_info(&(*lk->acq)[t->spid], t, as_data);
            if(syncchar->logWorksets) {
               open_workset(t, osamod, as_data);
            }
 
            // CXA is a transient state
            if(lk->state == LKST_CXA) {
               lk->state = t->old_state;
            }
         }
      }
      break;
   case LKST_RLK: // old_state
      switch(lk->state) {
      case LKST_OPEN:
         if(!t->read_unlock) {
            print_log("XXX RLK->OPEN read_unlock ", 0, t, osamod);
#ifdef DEBUG_INTERACTIVE      
            SIM_break_simulation("XXX");
#endif
         }
         process_unlock(t, osamod, as_data);
         break;
      case LKST_RLK:
         if(t->read_lock) {
            // Push our pid on the reader list, if it isn't
            // already.  Otherwise, increment the count
            reader_mapit_t rdr = lk->readers.find(t->spid);
            if(rdr == lk->readers.end()){
               lk->readers[t->spid] = 1;
               open_workset(t, osamod, as_data);
            } else {
               lk->readers[t->spid]++;
               // Don't do the other bookkeeping on nested acquires of
               // a read lock
               break;
            }

            // We are a reader and we acquired the read lock
            osa_cycles_t req_cyc = t->bp_cyc;
            if((*lk->acq)[t->spid].req_cyc != 0ULL) {
               req_cyc = (*lk->acq)[t->spid].req_cyc;
            }
            update_cyc_avgs((*lk->callers)[t->caller_ra].acq_av, t->now_cyc,
                       req_cyc, osamod);
            lock_spid_info(&(*lk->acq)[t->spid], t, as_data);

         } else if( t->read_unlock ) {
            // Reader unlocked
            process_unlock(t, osamod, as_data);
         } else {
            // We are a writer and we are locked out.  Record when
            // we start trying to get in.  Trying the lock obviously
            // failed, but that doesn't mean we are trying to get it,
            // we can go do something else.
            if((t->flags & F_TRYLOCK) == 0
               && (*lk->acq)[t->spid].req_cyc == 0ULL) {
               (*lk->acq)[t->spid].req_cyc = t->bp_cyc;
               if((*lk->acq)[t->spid].acq_ra != 0) {
                  *osamod->pStatStream << "XXXr acq_ra "
                                       << hex << (*lk->acq)[t->spid].acq_ra
                                       << dec
                                       << " acq_cyc "
                                       << (*lk->acq)[t->spid].acq_cyc
                                       << endl;
#ifdef DEBUG_INTERACTIVE      
                  SIM_break_simulation("XXX");
#endif
                  print_log("", 0, t, osamod);
               }
               record_contention(lk, t, osamod);
            }
         }
         break;
      case LKST_WRLK:
         // Writer lock upgrade, not expected
         pr("XXX Yes it can happen RLK->WRLK\n");
         print_log("XXX RLK->WRLK ", 0, t, osamod);
#ifdef DEBUG_INTERACTIVE      
         SIM_break_simulation("XXX");
#endif
         lock_spid_info(&(*lk->acq)[t->spid], t, as_data);
         break;
      }
      break;
   case LKST_WRLK: // old_state
      switch( lk->state ) {
      case LKST_OPEN:
         process_unlock(t, osamod, as_data);
         break;
      case LKST_RLK:
         // Writer downgrade, not expected
         print_log("XXX WRLK->RLK ", 0, t, osamod);
#ifdef DEBUG_INTERACTIVE      
         SIM_break_simulation("XXX");
#endif
         break;
      case LKST_WRLK:

         // DEP 6/1/07: Well, there is a special case where this is ok
         // - a semaphore on the down slowpath (__down()), can
         // transition from a negative value to 0 in acquiring the
         // lock.  If this happens, it is ok.  I don't think we should
         // actually hit this, though, as we still transition the lock
         // state to OPEN and the next cpu to execute the 'lock addl'
         // instruction should be the one to get the lock.  Thus, I am
         // going to leave this code out for a bit and just see what
         // happens.

         // We are locked out.  Record when we start trying to get in.
         // We can have the lock and do a try lock, which we should ignore
         int txid = 0;
         if(syncchar->osatxm != NULL) {
            txid = syncchar->osatxm->get_current_transaction(
                  (osamod_t*)syncchar->osatxm_mod, OSA_get_sim_cpu());
         }
         if(txid == 0) {
            if((t->flags & F_TRYLOCK) == 0
               && (*lk->acq)[t->spid].req_cyc == (osa_cycles_t)0) {
               (*lk->acq)[t->spid].req_cyc = t->bp_cyc;
               if((*lk->acq)[t->spid].acq_ra != 0
                  && !lk->queue) {

                  *osamod->pStatStream << "XXXw acq_ra "
                                       << hex << (*lk->acq)[t->spid].acq_ra
                                       << dec
                                       << " acq_cyc "
                                       << (*lk->acq)[t->spid].acq_cyc
                                       << endl;
                  print_log("", 0, t, osamod);
#ifdef DEBUG_INTERACTIVE      
                  SIM_break_simulation("XXX");
#endif
               }
               record_contention(lk, t, osamod);
            }
         }
         else {
            if((t->flags & F_TRYLOCK) == 0) {
               spcl_caller_t scaller = get_speculative_lock(txid, t->lock_addr,
                                                            t->caller_ra, as_data);
               if(scaller.req_cyc == 0ULL) {
                  scaller.req_cyc = t->bp_cyc;
               }
               spcl_map[txid][t->lock_addr][t->caller_ra] = scaller;
            }
         }
         break;
      }
      break;
   }
}

// An instruction that changed the state of a lock completed.  Track
// the transition 
static void lock_transition(conf_object_t *trigger_obj,
                            lang_void* _bp_rec) {
	
   osamod_t *osamod = (osamod_t *) trigger_obj;
   syncchar_data_t *syncchar = osamod->syncchar;

   struct transition_info _t;
   struct transition_info *t = &_t;
   as_data_t *as_data = ((struct bp_rec*)_bp_rec)->bp_as_data;

   initialize_transition(t, (struct bp_rec*)_bp_rec, osamod, as_data);

   // If we are interrupted on the lock instruction, we need to ignore
   // the stacked_post, as the operation didn't actually complete.  In
   // this case, the breakpoint will be called a second time.
   if(syncchar->exception_addrs[osamod->minfo->getCpuNum(OSA_get_sim_cpu())] == t->lock_ra){
      return;
   }

   struct lock* lk = &as_data->lockmap[t->lock_addr];

#ifdef DEBUG_ADDRESS
   if(t->lock_addr == DEBUG_ADDRESS){
      *osamod->pStatStream << "lock in question:" << endl;
      *osamod->pStatStream << "\told_state = " << t->old_state << endl;
      *osamod->pStatStream << "\towner = " << lk->spid_owner << endl;
      *osamod->pStatStream << "\tCurrent pid = " << t->spid << endl;
      *osamod->pStatStream << "\tOld lock value = " << std::hex << t->bp_lkval << std::dec  << endl;
      *osamod->pStatStream << "\tNew lock value = " << std::hex << t->lkval << std::dec  << endl;
      *osamod->pStatStream << "\tPC = " << std::hex << t->lock_ra << std::dec << endl;
      *osamod->pStatStream << "\tCPU = " << osamod->minfo->getCpuNum(OSA_get_sim_cpu()) << endl;
      *osamod->pStatStream << "\tflags = " << std::hex << t->flags << std::dec << endl;
      //OSA_stack_trace(OSA_get_sim_cpu(), osamod);
   }
#endif

   // Increment counts
   (*lk->callers)[t->caller_ra].count++;
   if( (t->flags & F_NOADDR) != 0 ) {
      // RCU, count & short count increment, thats it
      (*lk->callers)[t->caller_ra].acq_av[0].cnt++;
      (*lk->callers)[t->caller_ra].acq_av[1].cnt++;
   } else if( t->lock_id == L_COMPL ) {
      // Just count completions
      (*lk->callers)[t->caller_ra].acq_av[0].cnt++;
      (*lk->callers)[t->caller_ra].acq_av[1].cnt++;
   } else {
      t->new_state = update_current_lock_state(t, osamod, as_data);
      // Process transition from old_state -> new_state (lk->state)
      detect_read_lock_unlock(t, osamod);
      process_transition(t, osamod, as_data);
   }

#ifdef DEBUG_ADDRESS
   if(t->lock_addr == DEBUG_ADDRESS){
      *osamod->pStatStream << "\tnew_state = " << lk->state << endl;
      if(t->lock_id == L_RSPIN
         || t->lock_id == L_WSPIN){
         *osamod->pStatStream << "\tReaders:" << endl;
         for(reader_mapit_t rdr = lk->readers.begin();
             rdr != lk->readers.end(); rdr++){
            *osamod->pStatStream << "\t\t" << rdr->first << ": " << rdr->second << endl;
         }
      }
   }
#endif
}

static void exception_callback(lang_void* callback_data,
                               conf_object_t *trigger_obj,
                               osa_integer_t break_number,
                               osa_sim_inner_memop_t *memop) {

   osamod_t *osamod = (osamod_t*)callback_data;   

   osa_cpu_object_t *cpu = OSA_get_sim_cpu();
   osamod->syncchar->exception_addrs[osamod->minfo->getCpuNum(cpu)] = osa_read_register(cpu, regEIP);
}

static void breakpoint_callback(lang_void* callback_data,
                                conf_object_t *trigger_obj,
                                osa_integer_t break_number,
                                osa_sim_inner_memop_t *memop) {
   osamod_t *osamod = (osamod_t *) callback_data;
   syncchar_data_t *syncchar = osamod->syncchar;

   struct bp_rec* bp_rec = (struct bp_rec*)MM_ZALLOC(1, struct bp_rec);
   osa_sim_outer_memop_t* xmt = (osa_sim_outer_memop_t*) memop;
   bp_rec->bp_pc = (unsigned int)xmt->linear_address;
   bp_rec->caller_ra = bp_rec->bp_pc; // True for inlined functions
   osa_cpu_object_t *cpu = OSA_get_sim_cpu();
   bp_rec->cyc = osa_get_sim_cycle_count(cpu);
   int cpuNum = osamod->minfo->getCpuNum(cpu);
   syncchar->exception_addrs[cpuNum] = 0;

   // Set the proc info on the bp_rec
   spid_t my_spid = bp_rec->bp_pc < 0xC0000000 ? 
      osamod->os->current_process[cpuNum] : 0;

   as_mapit_t iter = syncchar->as_data.find(my_spid);
   if(iter == syncchar->as_data.end())
      return;

   as_data_t *as_data = iter->second;
   bp_rec->bp_as_data = as_data;

   if(break_number == syncchar->dbg_bp) {
      SIM_stacked_post(NULL, dbg_breakpoint_callback, bp_rec);
      return;
   }
   ra_mapcit_t raci = as_data->ramap.find(bp_rec->bp_pc);
   if(raci == as_data->ramap.end()) {
      /* This is ok in usermode */
      if(my_spid == 0){
         pr("XXX breakpoint on RA that we did not set %#x\n", 
            bp_rec->bp_pc);
         pr("BIOS code or kernel out of date with sync_char.map?\n");
#ifdef DEBUG_INTERACTIVE      
         SIM_break_simulation("XXX");
#endif
      }
      return;
   }
   unsigned int lock_addr = get_lock_addr(raci, cpu);

   // This is not the "real" previous lock value.  It is only used if
   // we get to lock_transition and have never seen this lock before.
   // The real previous lock value is in the lockmap because another
   // processor can write the value between now and lock_transition
   // OSH more cx_ special case
   if( ((raci->second.flags & F_NOADDR) == 0 
       && (raci->second.flags & F_EAX) == 0) 
       || raci->second.lock_id == L_CXA || raci->second.lock_id == L_CXE) {
      bp_rec->bp_lkval = read_4bytes(osamod, cpu, DATA_SEGMENT, lock_addr);
   }
   if((raci->second.flags & F_INLINED) == 0) {
      // Find caller
      unsigned int stack_addr;

      // x86 call instruction pushes RA on stack second, so just read it
      // This works because every function in which we need to read
      // this (at entry, at spin & major retry pcs) does not build its
      // own stack.  __lockfunc is fastcall.

      // On spin/retry, however, we can be inlined and need to get the
      // RA from *(%ebp - 4)
      if(1 || 0 == strcmp(raci->second.label, "spin")
         || 0 == strcmp( raci->second.label, "retry")){
         // spin/retry
         stack_addr = 
            _osa_read_register(cpu, SIM_get_register_number(cpu, "ebp"));
         stack_addr += 4;
      } else {
         stack_addr = 
            _osa_read_register(cpu, SIM_get_register_number(cpu, "esp"));
      }

      bp_rec->caller_ra = read_4bytes(osamod, cpu, STACK_SEGMENT, stack_addr);
   }

   SIM_stacked_post((conf_object_t * )osamod, lock_transition, bp_rec);
}

void handle_transaction_commit(void *syncchar_osamod, conf_object_t *osamod_obj,
      osa_cpu_object_t *cpu, int txid) {
   // walk through all of the locks and callers used by this
   // transaction and update the actual cycle averages
   tx_spcl_caller_mapcit_t tsit = spcl_map.find(txid);
   if(tsit != spcl_map.end()) {
      lock_spcl_caller_map_t lsmap = tsit->second;
      lock_spcl_caller_mapcit_t lsit = lsmap.begin();

      for(; lsit != lsmap.end(); lsit++) {
         unsigned int lock_addr = lsit->first;
         spcl_caller_map_t cmap = lsit->second;
         spcl_caller_mapcit_t cit = cmap.begin();
         struct lock *lk = NULL;

         for(; cit != cmap.end(); cit++) {
            unsigned int caller = cit->first;
            spcl_caller_t scaller = cit->second;
            if(!lk)
               *lk = scaller.as_data->lockmap[lock_addr];

            update_avgs((*lk->callers)[caller].acq_av,
                  (long double)scaller.total_cyc, (double)1000);
         }
      }
   }
   spcl_map.erase(txid);
}

void handle_transaction_abort(void *, conf_object_t *osamod,
      osa_cpu_object_t *cpu, int txid) {
   spcl_map.erase(txid);
}

static void reset_stats(osamod_t *osamod) {
   syncchar_data_t *syncchar = osamod->syncchar;

   for( as_mapit_t asit = syncchar->as_data.begin();
        asit != syncchar->as_data.end(); asit++){
      
      as_data_t *as_data = asit->second;

      // Leave state & zero counters
      for( lock_mapit_t lkit = as_data->lockmap.begin();
           lkit != as_data->lockmap.end();
           ++lkit ) {

         //zero_av(lkit->second.depend_av);
         
         // New benchmark, all new timings
         for( caller_mapit_t cait = lkit->second.callers->begin();
              cait != lkit->second.callers->end(); ++cait ) {
            caller_zero(&cait->second);
         }
         
         // Clear aggregate workset
         delete(lkit->second.aggregate_workset);
         lkit->second.aggregate_workset = new WorkSet(lkit->first, 0, 
                                                      lkit->second.generation,
                                                      0xffffffff,
                                                      0);
      }
   }

   for (int i = 0; i < osamod->minfo->getNumCpus() ; i++){
      osamod->procCycles[i] = osa_get_sim_cycle_count(osamod->minfo->getCpu(i));
   }
   const char *prefix = osamod->minfo->getPrefix().c_str();
   reset_cache_statistics(cache_count(osamod), prefix);
   prepare_kstats_snapshot(osamod);

   // Put a line in the log so that we dump the worksets we have read
   // thus far
   *osamod->pStatStream << "RESET_STATS" << endl;

}

static void print_av(ostream* stat_str, const avg_var av[3],
      int precision) {
   int rv, size = 8192;
   for(int i = 0; i < 3; ++i) {
      char buf[size];
      rv = snprintf(buf, size, "%u %.*Lf %.*Lf ", av[i].cnt, precision, 
                    av[i].sum, precision, av[i].xsqr);

      if(rv > size - 1){
         pr("Can't fit %d in %d buffer\n", rv, size);
         size *=2;
         i--;
         continue;
      }
      
      *stat_str << buf;
   }
}

static void print_lock(ostream *stat_str, unsigned int lock_addr,
                       const struct lock *lock, as_data_t *as_data){
   // Lock address, lock id, number of accessing spids, r/w/tot aggregate workset size
   *stat_str << hex << lock_addr << dec;
   if(lock->generation > 0){
      *stat_str << "_" << lock->generation;
   }
   *stat_str << "(" << lock->name << ")"
            << " " << lock->generation
            << " " << lock->workset_count
            << " " << lock->lock_id
            << " " << lock->acq->size()
            << " " << lock->aggregate_workset->rsize()
            << " " << lock->aggregate_workset->wsize()
            << " " << lock->aggregate_workset->size()
            << " ";
      
   print_av(stat_str, lock->nest_av, 0);

   // print data set depend averages
   /*
   print_av(stat_str, lock->depend_av, 0);
   print_av(stat_str, lock->total_av, 0);
   print_av(stat_str, lock->percent_av, 2);
   */

   for( caller_mapcit_t cacit = lock->callers->begin();
        cacit != lock->callers->end(); ++cacit ) {
      // Print [caller_ra flags count q_count useless_release avg_var's]
      *stat_str << " [" 
               << " " << hex << cacit->first << dec
               << " " << as_data->ramap[cacit->first].flags
               << " " << cacit->second.count 
               << " " << cacit->second.q_count 
         /*
               << " " << cacit->second.q_count_dependent
               << " " << cacit->second.q_count_independent
               << " " << cacit->second.q_count_dependent_total_bytes
               << " " << cacit->second.q_count_dependent_conflicting_bytes
         */
               << " " << cacit->second.useless_release
               << " ";
      print_av(stat_str, cacit->second.acq_av, 0);
      print_av(stat_str, cacit->second.hold_av, 0);
      *stat_str << "] ";
   }

   *stat_str << '\n';
}

static void get_stats(osamod_t *osamod) {
   syncchar_data_t *syncchar = osamod->syncchar;
   
   // Make sure we don't take a null pointer on the stat stream
   if(!*osamod->pStatStream){
      cerr << "XXX: Syncchar has no stat stream defined!" << endl;
      return;
   }
   
   // beware empty lines. Apparently they are dangerous.

   const char *prefix = osamod->minfo->getPrefix().c_str();
   dump_cache_statistics( cache_count(osamod), 
                          osamod->pStatStream,
                          OSATXM_STATS_TABULAR,
                          OSATXM_STATS_DELIMITER,
                          OSATXM_STATS_SHOWROWNAMES,
                          OSATXM_STATS_SHOWCOLNAMES,
                          prefix);
   *osamod->pStatStream << take_kstats_snapshot(osamod);

   // beware empty lines. No, really.
   for (int i = 0; i < osamod->minfo->getNumCpus() ; i++){
      *osamod->pStatStream << "Cycles " << i << ": " 
               <<osa_get_sim_cycle_count(osamod->minfo->getCpu(i)) -  osamod->procCycles[i]
               <<'\n';
   }

   unordered_map<as_data_t *, int> already_seen;

   for( as_mapit_t asit = syncchar->as_data.begin();
        asit != syncchar->as_data.end(); asit++){
      
      as_data_t *as_data = asit->second;

      if(already_seen.find(as_data) != already_seen.end())
         continue;
      else 
         already_seen[as_data] = 1;

      // Active locks
      for( lock_mapcit_t lkcit = as_data->lockmap.begin();
           lkcit != as_data->lockmap.end();
           ++lkcit ) {
         print_lock(osamod->pStatStream, lkcit->first, &(lkcit->second), as_data);
      }
      
      // Reduce acq maps to contain only the spids that are using it (and
      // hence are valid users for the next measurement period 
      acq_map_t* new_acq
         = new acq_map_t();
      for( lock_mapcit_t lkcit = as_data->lockmap.begin();
           lkcit != as_data->lockmap.end();
           ++lkcit ) {
         for( acqcit_t acqcit = lkcit->second.acq->begin(); 
              acqcit != lkcit->second.acq->end(); ++acqcit) {
            if(acqcit->second.acq_cyc != (osa_cycles_t)0ULL) {
               (*new_acq)[acqcit->first] = acqcit->second;
            }
         }
         lkcit->second.acq->clear();
         lkcit->second.acq->insert(new_acq->begin(), new_acq->end());
         new_acq->clear();
      }
      delete new_acq;
   }

   *osamod->pStatStream << "SYNCCHAR: End of Stats" << endl; 
}

static attr_value_t get_stats_attribute(void *arg, conf_object_t *obj, 
                                        attr_value_t *idx) {
   osamod_t *osamod = (osamod_t*)obj;

   if(osamod->pStatStream)
      get_stats((osamod_t *) obj);
   return SIM_make_attr_string("Output stats to log file");
}

static set_error_t
set_stats_attribute(void *arg, conf_object_t *obj,
                    attr_value_t *val, attr_value_t *idx) {
   reset_stats((osamod_t *) obj);
   return Sim_Set_Ok;
}

static void output_stats(osamod_t *osamod){
   get_stats(osamod);
}

static void osa_sched_callback(osamod_t *osamod){
   // First we need to update the runqueue lock's owner.  A lot of
   // work for one lock, imho
   osa_cpu_object_t *cpu = OSA_get_sim_cpu();
   spid_t new_pid =  (int)osa_read_register(cpu, regECX);
   int cpuNum = osamod->minfo->getCpuNum(cpu);
   spid_t old_pid = osamod->os->current_process[cpuNum];
   syncchar_data_t *syncchar = osamod->syncchar;

   // Assume we are in the kernel
   as_data_t *as_data = syncchar->as_data[0];

   for( lock_mapit_t lkit = as_data->lockmap.begin();
        lkit != as_data->lockmap.end();
        ++lkit ) {
         
      if(lkit->second.spid_owner == old_pid
         && strncmp(lkit->second.name, "runqueue_t->lock", LOCK_NAME_SIZE) == 0){
         // Update the pid of the runqueue lock
         lkit->second.spid_owner = new_pid;

         // Find the runqueue locks's workset and update it too
         lockset_mapcit_t lsit = as_data->locksetmap.find(old_pid);
         if(lsit != as_data->locksetmap.end()) {
            workset_list_t *worksets = lsit->second;
            workset_listit_t wsit, wsit2;
               
            for(wsit = worksets->begin(); wsit != worksets->end(); ){
               // Increment wsit early so that we can delete and
               // keep going
               wsit2 = wsit;
               wsit++;

               if(wsit2->second->id.lock_addr == lkit->first){
                  WorkSet * ws = wsit2->second;
                  // Update spid
                  ws->pid = new_pid;
                  ws->old_pid = old_pid;
                  ws->twoowners = 1;
                  // Take it out of this workset
                  worksets->erase(wsit2);

                  // Put it in the workset of the new pid
                  lock_mapcit_t lock_cit = as_data->lockmap.find(lkit->first);
                  lockset_mapcit_t lsit2 = as_data->locksetmap.find(new_pid);
                  if(lsit2 == as_data->locksetmap.end()) {
                     // We don't have a workset list
                     as_data->locksetmap[new_pid] =
                        new workset_list_t(1, make_pair(lock_cit, ws));
                  } else {
                     // We do have a workset list, just insert it
                     lsit2->second->push_front(make_pair(lock_cit, ws));
                  }
               }
            }
         }
      }
   }
}
 

static void free_as_data(as_data_t *as_data){
   // Clear the ras
   as_data->ramap.clear();
   
   // clear the lock definitions
   for(lock_mapit_t iter = as_data->lockmap.begin();
       iter != as_data->lockmap.end(); iter++){
      struct lock *old_lock = &(iter->second);
      delete old_lock->acq;
      delete old_lock->callers;
      delete old_lock->aggregate_workset;
      
      as_data->lockmap.erase(iter);
   }

   // clear the bps
   for(vector<breakpoint_id_t>::iterator iter = as_data->bps.begin(); 
       iter != as_data->bps.end(); iter++){
      SIM_delete_breakpoint(*iter);
   }
   as_data->bps.clear();

   delete as_data;
}

/* On a fork, propagate the as_reference, if exists.  Assume that the
   current pid is tha parent. */
 static void osa_fork_callback(osamod_t *osamod){
    osa_cpu_object_t *cpu = OSA_get_sim_cpu();
    int cpuNum = osamod->minfo->getCpuNum(OSA_get_sim_cpu());
    spid_t ppid = osamod->os->current_process[cpuNum];

    // Don't bother with the kernel/idle process
    if(ppid == 0)
       return;

    as_mapit_t iter = osamod->syncchar->as_data.find(ppid);

    // If the ppid has a user-level map, propagate it to the child
    if(iter != osamod->syncchar->as_data.end()){
       int pid = osa_read_register(cpu, regEBX);
       as_data_t *as_data = iter->second;
       osamod->syncchar->as_data[pid] = as_data;
       as_data->ref_count++;
    }
 }


 // Clean up the map so that we don't waste space
 static void osa_exit_callback(osamod_t *osamod){
   syncchar_data_t *syncchar = osamod->syncchar;

   osa_cpu_object_t *cpu = OSA_get_sim_cpu();
   int cpuNum = osamod->minfo->getCpuNum(cpu);
   spid_t spid = osamod->os->current_process[cpuNum];

   // Don't free kernel data
   if(spid == 0){
      *osamod->pStatStream << "XXX: Attempt to free kernel as_data thwarted." << endl;
      return;
   }
   as_mapit_t iter = syncchar->as_data.find(spid);
   if(iter != syncchar->as_data.end()){

      as_data_t *as_data = iter->second;
      as_data->ref_count--;
      
      if(as_data->ref_count == 0){
         free_as_data(iter->second);         
         pr("[syncchar] Cleared map for pid %d\n", spid);
      }

      syncchar->as_data.erase(iter);
   }
 }
 
static void osa_register_spinlock_callback(osamod_t *osamod){
   osa_cpu_object_t *cpu = OSA_get_sim_cpu();
   int cpuNum = osamod->minfo->getCpuNum(cpu);
   spid_t spid = osamod->os->current_process[cpuNum];

   // As we register other lock types, just fall through other cases 
   osa_uinteger_t lock_addr = osa_read_register(cpu, regEDX);
   osa_logical_address_t str_ptr = osa_read_register(cpu, regECX);
   int len = osa_read_register(cpu, regEBX);

   bool in_kernel = lock_addr >= 0xc0000000;
   as_mapit_t iter = osamod->syncchar->as_data.find(in_kernel ? 0 : spid);
   OSA_assert(iter != osamod->syncchar->as_data.end(), osamod);

   as_data_t *as_data = iter->second;
         
   char tmp[LOCK_NAME_SIZE];
   read_string(cpu, str_ptr, tmp, len < LOCK_NAME_SIZE ? len + 1 : 256, 1);

   int lkval = read_4bytes(osamod, cpu, DATA_SEGMENT, lock_addr);
   // Create a new lock entry
   // Only support spins for now
   allocate_lock(L_SPIN, lock_addr, lkval, tmp, osamod, as_data);
}

static void osa_after_boot_callback(osamod_t *osamod){
   syncchar_data_t *scd = osamod->syncchar;
   scd->afterBoot = true;
}


/*
 * timing_operate()
 * return the number of cycles 
 * spent on the current memory transaction
 * forward to lower level caches where
 * appropriate.
 */
static osa_cycles_t timing_operate( conf_object_t* pConfObject, 
                                conf_object_t* pSpaceConfObject,
                                map_list_t* pMapList,
                                osa_sim_inner_memop_t* pMemTx ) {

   osamod_t *osamod = (osamod_t*)pConfObject;

   osa_cpu_object_t *cpu = OSA_get_sim_cpu();
   int cpuNum = osamod->minfo->getCpuNum(cpu);
   spid_t spid = osamod->os->current_process[cpuNum];
      
   // Hack to figure out if we are in the kernel or not
   bool in_kernel = osa_read_register(cpu, regEIP) >= 0xc0000000;
   
   // Let's also add filtering for stack addresses while
   // we are at it
   unsigned int ebp, esp;
   ebp = osa_read_register(cpu, regEBP);
   esp = osa_read_register(cpu, regESP);
   if(pMemTx->logical_address > ebp
      || pMemTx->logical_address < esp){

      if(((osa_sim_outer_memop_t*)pMemTx)->access_type == X86_Vanilla &&
         (pMemTx->type == Sim_Trans_Load || pMemTx->type == Sim_Trans_Store)) {
         // This is a memory reference we care about

         as_mapit_t iter = osamod->syncchar->as_data.find(in_kernel ? 0 : spid);
         if(iter != osamod->syncchar->as_data.end()){
            // This is an address space (user or kernel) that we care about
            as_data_t *as_data = iter->second;
         
            lockset_mapcit_t lsit = as_data->locksetmap.find(spid);
            if(lsit != as_data->locksetmap.end()) {
               // And we have a lockmap for this address space
               workset_list_t *worksets = lsit->second;
               workset_listit_t wsit;
            
               
               if(!worksets->empty()){
                  
                  // add this access to each workset for the current spid
                  for(wsit = worksets->begin(); wsit != worksets->end(); wsit++){
                     
                     // We need to filter out the lock address itself from the
                     // workset, so that we don't just get 100% data
                     // dependence!  Actually, let's filter all lock addresses,
                     // just for good measure
                     if(as_data->lockmap.find(pMemTx->logical_address) 
                        == as_data->lockmap.end()){
                     
                     
                        wsit->second->grow(pMemTx->logical_address, pMemTx->size,
                                           pMemTx->type);
                     
                        // Also, add this to the aggregate workset for the lock
                        lock_mapcit_t lkit = as_data->lockmap.find(wsit->second->id.lock_addr);
                        if(lkit == as_data->lockmap.end()){
                           cout << "XXX: Can't find lock " << std::hex 
                                << wsit->second->id.lock_addr << std::dec << endl;
#ifdef DEBUG_INTERACTIVE      
                           SIM_break_simulation("XXX");
#endif
                        }
                     
                        lkit->second.aggregate_workset->grow(pMemTx->logical_address, pMemTx->size,
                                                             pMemTx->type);
                     }
                  }
                  goto cct;
               }
            }

            // If we don't have any active worksets, add it to the asymmetric conflict detector
            as_data->asym_detector->grow(pMemTx->logical_address, pMemTx->size,
                                         pMemTx->type);
         }
      }
   }
 cct:
   return collect_cache_timing( pConfObject,
                                pSpaceConfObject,
                                pMapList,
                                pMemTx );
}

static void handle_device_access_memop(lang_void *callback_data, 
                                       conf_object_t *trigger_obj, 
                                       osa_sim_inner_memop_t *pMemTx) {
   osamod_t *osamod = (osamod_t*)callback_data;
   osa_cpu_object_t *cpu = OSA_get_sim_cpu();
   // Make sure this is the correct machine
   if(osamod->minfo->getOwner(cpu) != osamod){
      cout << std::hex << "cpu = " << cpu 
           << ", Machine 1: " << osamod->minfo->getOwner(cpu)
           << ", mach 2 : " << osamod << std::dec << endl;
      SIM_break_simulation("Incorrect machine");
      return;
   }

   int cpuNum = osamod->minfo->getCpuNum(cpu);
   spid_t spid = osamod->os->current_process[cpuNum];

   // Hack to figure out if we are in the kernel or not
   bool in_kernel = osa_read_register(cpu, regEIP) >= 0xc0000000;
   as_mapit_t iter = osamod->syncchar->as_data.find(in_kernel ? 0 : spid);

   if(iter == osamod->syncchar->as_data.end())
      return;

   as_data_t *as_data = iter->second;

   // We need to mark all locks held during this operation as having
   // io.
   lockset_mapcit_t lsit = as_data->locksetmap.find(spid);
   if(lsit != as_data->locksetmap.end()) {
      workset_list_t *worksets = lsit->second;
      workset_listit_t wsit;
      for(wsit = worksets->begin(); wsit != worksets->end(); wsit++){

         // Testing code
         //SIM_break_simulation("IO With lock held!");

         wsit->second->IO = 1;
         
         // Also, mark the aggregate workset for the lock
         lock_mapcit_t lkit = as_data->lockmap.find(wsit->second->id.lock_addr);
         if(lkit == as_data->lockmap.end()){
            cout << "XXX: Can't find lock " << std::hex 
                 << wsit->second->id.lock_addr << std::dec << endl;
#ifdef DEBUG_INTERACTIVE      
            SIM_break_simulation("XXX");
#endif
         }
         lkit->second.aggregate_workset->IO = 1;
      }
   }
}


static attr_value_t get_context_attribute(void*, conf_object_t *osamod,
                                          attr_value_t *idx) {
   return SIM_make_attr_object( ((osamod_t*)osamod)->syncchar->context );
}

static set_error_t set_context_attribute(void*, conf_object_t *osamod_obj,
                                         attr_value_t *val, attr_value_t *idx) {
   osamod_t *osamod = (osamod_t*)osamod_obj;

   osamod->syncchar->context = val->u.object;

   // set the symtable of the context
   conf_object_t *st0 = osa_get_object_by_name("st0");
   attr_value_t attr = SIM_make_attr_object(st0);
   SIM_set_attribute(osamod->syncchar->context, "symtable", &attr);

   // make sure all of the cpus that we own have
   // this context set
   attr_value_t context_attr = SIM_make_attr_object(osamod->syncchar->context);
   int i;
   for(i = 0; i < osamod->minfo->getNumCpus(); i++)
      SIM_set_attribute(osamod->minfo->getCpu(i), "current_context",
                        &context_attr);
   
   // make sure context has breakpoint callback set
   // there doesn't seem to be a way to check if
   // there's a callback set only on an object,
   // so just delete it
   SIM_hap_delete_callback_obj(
                               "Core_Breakpoint_Memop",
                               osamod->syncchar->context,
                               (obj_hap_func_t)breakpoint_callback,
                               (void*)osamod);

   hap_handle_t handle;
   handle = SIM_hap_add_callback_obj(
                                     "Core_Breakpoint_Memop",
                                     osamod->syncchar->context,
                                     (hap_flags_t)0,
                                     (obj_hap_func_t)breakpoint_callback,
                                     (void*)osamod);

   // Detect io with a lock held
   handle = SIM_hap_add_callback(
                                 "Core_Device_Access_Memop",
                                 (obj_hap_func_t)handle_device_access_memop,
                                 osamod);
   

   return Sim_Set_Ok;
}

/**
 * set_osatxm()
 * set the osatxm attribute 
 * which allows syncchar to read transactional data
 */
set_error_t
set_osatxm( void *dont_care, 
            conf_object_t *pConfObject,
            attr_value_t *pAttrValue, 
            attr_value_t *pAttrIndex )
{
   osamod_t *osamod = (osamod_t *) pConfObject;
   if( pAttrValue->kind == Sim_Val_Nil) {
      osamod->osatxm = NULL;
   } else {
      osamod->syncchar->osatxm = (osatxm_interface_t *)
         SIM_get_interface( pAttrValue->u.object,
                            "osatxm_interface");
      if( SIM_clear_exception() ) {
         SIM_log_error( &osamod->log, 1,
                        "osatxm::set_osatxm: "
                        "object '%s' does not provide the osatxm memaccess "
                        "interface.", pAttrValue->u.object->name );
         return Sim_Set_Interface_Not_Found;
      }
      osamod->syncchar->osatxm_mod = pAttrValue->u.object;

      SIM_hap_delete_callback("Osatxm_Transaction_Commit",
            (obj_hap_func_t)handle_transaction_commit, osamod);
      SIM_hap_delete_callback("Osatxm_Transaction_Restart",
            (obj_hap_func_t)handle_transaction_abort, osamod);
      SIM_hap_add_callback("Osatxm_Transaction_Commit",
            (obj_hap_func_t)handle_transaction_commit, osamod);
      SIM_hap_add_callback("Osatxm_Transaction_Restart",
            (obj_hap_func_t)handle_transaction_abort, osamod);
   }
   return Sim_Set_Ok;
}
    
attr_value_t
get_osatxm( void *dont_care, 
                  conf_object_t *pConfObject,
                  attr_value_t *pAttrIdx )
{
   osamod_t *osamod = (osamod_t *)pConfObject;
   return SIM_make_attr_object(osamod->syncchar->osatxm_mod);
}

osa_attr_set_t set_txcache_attribute( SIMULATOR_SET_INTEGER_ATTRIBUTE_SIGNATURE ){
   osamod_t* osamod = (osamod_t*) obj;
   osamod->use_txcache = INTEGER_ARGUMENT;
   initialize_cache_stats(osamod->use_txcache);
   return ATTR_OK;
}

integer_attribute_t get_txcache_attribute( SIMULATOR_GET_ATTRIBUTE_SIGNATURE ){
   osamod_t* osamod = (osamod_t*) obj;
   return INT_ATTRIFY(osamod->use_txcache);
}

static attr_value_t get_system_attribute(void*, conf_object_t *sc,
                                         attr_value_t *idx) {
   return SIM_make_attr_object(((osamod_t*)sc)->system);
}

static set_error_t set_system_attribute(void*, conf_object_t *osa_obj,
                                        attr_value_t *val, attr_value_t *idx) {

   osamod_t *osamod = (osamod_t*)osa_obj;
   syncchar_data_t *syncchar = osamod->syncchar;

   conf_object_t *system = val->u.object;

   osamod->system = system;
   osamod->minfo = new MachineInfo(system, osamod);

   // now set all machine-specific attributes

   syncchar->exception_addrs = new unsigned int[osamod->minfo->getNumCpus()];
   memset(syncchar->exception_addrs, 0,
          osamod->minfo->getNumCpus()*sizeof(*syncchar->exception_addrs));


   /* init os visibility */
   init_procs(osamod);

   /* init procCycles */
   osamod->procCycles = NULL;
   attr_value_t avReturn = SIM_alloc_attr_list(osamod->minfo->getNumCpus());
   for(int i = 0; i < osamod->minfo->getNumCpus(); i++){
      avReturn.u.list.vector[i] = SIM_make_attr_integer(0);
   }

   SIM_set_attribute((conf_object_t *) osamod,
                     "procCycles",
                     &avReturn);

   SIM_free_attribute(avReturn);


   attr_value_t cpu_switch_time = SIM_make_attr_integer(5);
   SIM_set_attribute((conf_object_t *)osamod,
                     "cpu_switch_time",
                     &cpu_switch_time);

   attr_value_t disk_delay = SIM_make_attr_floating(0.0055);
   SIM_set_attribute((conf_object_t *)osamod,
                     "disk_delay",
                     &disk_delay);


   // set callback handlers for only the cpus that we own
   int i;
   osa_cpu_object_t *cpu;
   hap_handle_t hapHandle;
   for(i = 0; i < osamod->minfo->getNumCpus(); i++) {
      cpu = osamod->minfo->getCpu(i);
      
      hapHandle = SIM_hap_add_callback_obj(
                                           "Core_Exception",
                                           cpu, (hap_flags_t)0,
                                           (obj_hap_func_t)exception_callback,
                                           (void*)osamod );
   }

   return Sim_Set_Ok;
}

static void syncchar_clear_map_callback(osamod_t *osamod){

   syncchar_data_t *syncchar = osamod->syncchar;

   osa_cpu_object_t *cpu = OSA_get_sim_cpu();
   int cpuNum = osamod->minfo->getCpuNum(cpu);
   spid_t spid = osamod->os->current_process[cpuNum];

   as_mapit_t iter = syncchar->as_data.find(spid);
   if(iter != syncchar->as_data.end()){
      free_as_data(iter->second);
   }
   
   pr("[syncchar] Cleared map\n");
}

static attr_value_t get_mapfile(void*, conf_object_t *sc,
                                attr_value_t *idx) {
   return SIM_make_attr_string(((osamod_t*)sc)->syncchar->as_data[0]->map_file_name);
}

// This always loads the mapfile for the kernel.  Apps must use the
// magic instruction interface.
static set_error_t set_mapfile(void*, conf_object_t *osa_obj,
                               attr_value_t *val, attr_value_t *idx) {
   
   osamod_t *osamod = (osamod_t*) osa_obj;
   syncchar_data_t *scd = osamod->syncchar;
   int rv;

   // Create/init kernel as_data
   as_data_t *as_data = new as_data_t();
   as_data->asym_detector = new WorkSet(0xc0000000, 0, 0, 0xffffffff, 0);
   as_data->ad_count = 0;
      
   scd->as_data[0] = as_data;
   as_data->ref_count = 1;

   as_data->map_file_name = MM_ZALLOC(strlen(val->u.string) + 1, char);
   strcpy(as_data->map_file_name, val->u.string);

   // this really should go in init_local, but we need a current cpu -
   // and the filename.  Go ahead and call it each time so that
   // statically defined locks get registered in all machines.
   rv = init_ras(osamod, as_data);

   insert_bps(osamod, as_data);

   // Only say success if we really have it.  It isn't a catastrophe
   // if no mapfile is provided - we can delay load them.
   if(rv == 0)
      pr("[syncchar] Successfully loaded mapfile %s\n", as_data->map_file_name);

   return Sim_Set_Ok;
}

#define MAX_PATH 256
// This is how apps load a syncchar map, through a magic instruction.
static void syncchar_load_map_callback(osamod_t *osamod){
   int rv;

   cout << "Loading a file via magic instruction \n";

   syncchar_data_t *scd = osamod->syncchar;

   osa_cpu_object_t *cpu = OSA_get_sim_cpu();
   int cpuNum = osamod->minfo->getCpuNum(cpu);

   spid_t pid = osamod->os->current_process[cpuNum];

   // Don't let the kernel map change.
   OSA_assert(pid != 0, osamod);

   osa_logical_address_t str_ptr = osa_read_register(cpu, regEBX);
   int len = osa_read_register(cpu, regECX);

   // Allocate a as_data entry if one doesn't exist
   as_mapit_t iter = scd->as_data.find(pid);
   as_data_t *as_data;
   if(iter == scd->as_data.end()) {
      as_data = new as_data_t();
      scd->as_data[pid] = as_data;
      as_data->ref_count = 1;
      as_data->asym_detector = new WorkSet(0, pid, 0, 0xffffffff, cpuNum);
      as_data->ad_count = 0;
   } else {
      as_data = iter->second;
      if(as_data->map_file_name)
         MM_FREE(as_data->map_file_name);
   }

   as_data->map_file_name = MM_ZALLOC(len, char);
   read_string(cpu, str_ptr, as_data->map_file_name, len, 1);  

   // this really should go in init_local, but we need a current cpu -
   // and the filename.  Go ahead and call it each time so that
   // statically defined locks get registered in all machines.
   rv = init_ras(osamod, as_data);

   insert_bps(osamod, as_data);

   if(rv == 0)
      pr("[syncchar] Successfully loaded mapfile %s\n", as_data->map_file_name);

}

static attr_value_t get_archived_worksets(void*, conf_object_t *sc,
      attr_value_t *idx) {
   return SIM_make_attr_integer(((osamod_t*)sc)->syncchar->archived_worksets);
}

static set_error_t set_archived_worksets(void*, conf_object_t *osa_obj,
      attr_value_t *val, attr_value_t *idx) {
   osamod_t *osamod = (osamod_t*)osa_obj;

   osamod->syncchar->archived_worksets = val->u.integer;

   return Sim_Set_Ok;
}

static attr_value_t get_logWorksets(void*, conf_object_t *sc,
      attr_value_t *idx) {
   return SIM_make_attr_boolean(((osamod_t*)sc)->syncchar->logWorksets);
}

static set_error_t set_logWorksets(void*, conf_object_t *osa_obj,
      attr_value_t *val, attr_value_t *idx) {
   osamod_t *osamod = (osamod_t*)osa_obj;

   osamod->syncchar->logWorksets = val->u.boolean;

   return Sim_Set_Ok;
}


static attr_value_t get_afterBoot(void*, conf_object_t *sc,
      attr_value_t *idx) {
   return SIM_make_attr_boolean(((osamod_t*)sc)->syncchar->afterBoot);
}

static set_error_t set_afterBoot(void*, conf_object_t *osa_obj,
      attr_value_t *val, attr_value_t *idx) {
   osamod_t *osamod = (osamod_t*)osa_obj;

   osamod->syncchar->afterBoot = val->u.boolean;

   return Sim_Set_Ok;
}


static attr_value_t get_lockmap(void*, conf_object_t *osamod,
      attr_value_t *idx) {
   syncchar_data_t *syncchar = ((osamod_t*)osamod)->syncchar;

   // Just return the kernel lockmap for now
   lock_map_t * lockmap = &(syncchar->as_data[0]->lockmap);

   // Allocate a dict
   attr_value_t avReturn = SIM_alloc_attr_dict(lockmap->size());

   // Populate it, keyed on lock address
   int i = 0;
   for(lock_mapcit_t lsit = lockmap->begin(); 
       lsit != lockmap->end(); lsit++, i++){

      // Allocate another dict to represent the struct lock
      attr_value_t avStructLock = SIM_alloc_attr_dict(12);
      
      avStructLock.u.dict.vector[0].key = SIM_make_attr_string("state");
      switch(lsit->second.state){
      case LKST_OPEN:
         avStructLock.u.dict.vector[0].value = SIM_make_attr_string("LKST_OPEN");
         break;
      case LKST_RLK:
         avStructLock.u.dict.vector[0].value = SIM_make_attr_string("LKST_RLK");
         break;
      case LKST_WRLK:
         avStructLock.u.dict.vector[0].value = SIM_make_attr_string("LKST_WRLK");
         break;
      default:
         cout << "Unknown lock state - " << lsit->second.state << endl;
         avStructLock.u.dict.vector[0].value = SIM_make_attr_string("Unknown State");
      }

      avStructLock.u.dict.vector[1].key = SIM_make_attr_string("lkval");
      avStructLock.u.dict.vector[1].value = SIM_make_attr_integer(lsit->second.lkval);

      avStructLock.u.dict.vector[2].key = SIM_make_attr_string("queue");
      avStructLock.u.dict.vector[2].value = SIM_make_attr_boolean(lsit->second.queue);

      avStructLock.u.dict.vector[3].key = SIM_make_attr_string("lock_id");
      switch(lsit->second.lock_id){
      case L_SEMA:
         avStructLock.u.dict.vector[3].value = SIM_make_attr_string("L_SEMA");
         break;
      case L_RSEMA:
         avStructLock.u.dict.vector[3].value = SIM_make_attr_string("L_RSEMA");
         break;
      case L_WSEMA:
         avStructLock.u.dict.vector[3].value = SIM_make_attr_string("L_WSEMA");
         break;
      case L_SPIN:
         avStructLock.u.dict.vector[3].value = SIM_make_attr_string("L_SPIN");
         break;
      case L_RSPIN:
         avStructLock.u.dict.vector[3].value = SIM_make_attr_string("L_RSPIN");
         break;
      case L_WSPIN:
         avStructLock.u.dict.vector[3].value = SIM_make_attr_string("L_WSPIN");
         break;
      case L_MUTEX:
         avStructLock.u.dict.vector[3].value = SIM_make_attr_string("L_MUTEX");
         break;
      case L_COMPL:
         avStructLock.u.dict.vector[3].value = SIM_make_attr_string("L_COMPL");
         break;
      case L_RCU:
         avStructLock.u.dict.vector[3].value = SIM_make_attr_string("L_RCU");
         break;
      case L_FUTEX:
         avStructLock.u.dict.vector[3].value = SIM_make_attr_string("L_FUTEX");
         break;
      case L_CXA:
         avStructLock.u.dict.vector[3].value = SIM_make_attr_string("L_CXA");
         break;
      case L_CXE:
         avStructLock.u.dict.vector[3].value = SIM_make_attr_string("L_CXE");
         break;
      default:
         cout << "Unknown lock id - " << lsit->second.lock_id << endl;
         avStructLock.u.dict.vector[3].value = SIM_make_attr_string("Unknown Lock ID");

      }


      avStructLock.u.dict.vector[4].key = SIM_make_attr_string("spid_owner");
      avStructLock.u.dict.vector[4].value = SIM_make_attr_integer(lsit->second.spid_owner);

      avStructLock.u.dict.vector[5].key = SIM_make_attr_string("acq");
      avStructLock.u.dict.vector[5].value = SIM_make_attr_nil(); //FIXME

      avStructLock.u.dict.vector[6].key = SIM_make_attr_string("callers");
      avStructLock.u.dict.vector[6].value = SIM_make_attr_nil(); // FIXME

      avStructLock.u.dict.vector[7].key = SIM_make_attr_string("name");
      avStructLock.u.dict.vector[7].value = SIM_make_attr_string(lsit->second.name);

      // Not used any more //
      avStructLock.u.dict.vector[8].key = SIM_make_attr_string("worksets");
      avStructLock.u.dict.vector[6].value = SIM_make_attr_nil(); // FIXME
      /*
      avStructLock.u.dict.vector[8].value = SIM_alloc_attr_list(lsit->second.worksets.size());
      { // Put the worksets in the attr_value_t
         int idx = 0;
         for(deque<WorkSet*>::const_iterator ws_it = lsit->second.worksets.begin(); 
             ws_it != lsit->second.worksets.end(); ws_it++){
            avStructLock.u.dict.vector[8].value.u.list.vector[idx] = (*ws_it)->get_workset();
            idx++;
         }
      }
      */
         
      avStructLock.u.dict.vector[9].key = SIM_make_attr_string("depend_av");
      avStructLock.u.dict.vector[9].value = SIM_make_attr_nil(); // FIXME

      avStructLock.u.dict.vector[10].key = SIM_make_attr_string("total_av");
      avStructLock.u.dict.vector[10].value = SIM_make_attr_nil(); // FIXME

      avStructLock.u.dict.vector[11].key = SIM_make_attr_string("percent_av");
      avStructLock.u.dict.vector[11].value = SIM_make_attr_nil(); // FIXME


      // Put it back in the output dict
      avReturn.u.dict.vector[i].key = SIM_make_attr_integer(lsit->first);
      avReturn.u.dict.vector[i].value = avStructLock;
   }
   
   return avReturn;
}

static set_error_t set_lockmap(void*, conf_object_t *osa_obj,
      attr_value_t *val, attr_value_t *idx) {
   // FIXME: Write me!

      //      osamod_t *osamod = (osamod_t*)osa_obj;

   //   osamod->syncchar->archived_worksets = val->u.integer;

   return Sim_Set_Ok;
}


extern "C" {
   static conf_object_t* new_sync_char_instance(parse_object_t *parse_obj) {

      pr("[sync_char] new instance\n");

      osamod_t *osamod = MM_ZALLOC(1, osamod_t);
      SIM_log_constructor(&osamod->log, parse_obj);
		osamod = (osamod_t *) &osamod->log.obj;

      osamod->type = SYNCCHAR;
      osamod->timing_model = NULL;
      osamod->timing_ifc = NULL;
      osamod->system = NULL;

      // have to use new here, because syncchar_data has
      // embedded c++ objects
      syncchar_data_t *syncchar = new syncchar_data_t;
      osamod->syncchar = syncchar;
      osamod->syncchar->dbg_bp = -1;
      osamod->syncchar->archived_worksets = 1;
      osamod->syncchar->logWorksets = true;
      osamod->syncchar->afterBoot = false;

      osamod->syncchar->osatxm = NULL;
      osamod->syncchar->osatxm_mod = NULL;

      time_t tim = time(NULL);
      
      ostream *pStatStream = new ofstream("sync_char.log");
      osamod->pStatStream = pStatStream;
      
      if(pStatStream->good() == false) {
         std::cerr << "Error opening sync_char.log\n";
      }
      *pStatStream << "Started simulation at " << ctime(&tim);
      pStatStream->setf(std::ios::showbase);
      
      return &osamod->log.obj;
   }

   void init_local(void) {
      class_data_t classData;
      conf_class_t* pConfClass;
      timing_model_interface_t *pTimingInterface;
      common_osamod_interface_t *common_osamod_iface;
      common_syncchar_interface_t *common_syncchar_iface;

      // Ignore error message so creation of module on build succeeds.
      memset(&classData, 0, sizeof(classData));
      classData.new_instance = new_sync_char_instance;
      classData.description = "Synch Characterization by OSA";
    
      pConfClass = SIM_register_class("sync_char", &classData);

      // Interface from common to us
      common_osamod_iface = MM_ZALLOC(1, common_osamod_interface_t);
      common_osamod_iface->clear_stat  = reset_stats;
      common_osamod_iface->output_stat = output_stats;
      SIM_register_interface(pConfClass, "common_osamod_interface", common_osamod_iface);

      // Syncchar specific interface from common to us
      common_syncchar_iface = MM_ZALLOC(1, common_syncchar_interface_t);
      common_syncchar_iface->osa_sched             = osa_sched_callback;
      common_syncchar_iface->osa_register_spinlock = osa_register_spinlock_callback;
      common_syncchar_iface->osa_after_boot        = osa_after_boot_callback;
      common_syncchar_iface->osa_fork              = osa_fork_callback;
      common_syncchar_iface->osa_exit              = osa_exit_callback;
      common_syncchar_iface->syncchar_clear_map    = syncchar_clear_map_callback;
      common_syncchar_iface->syncchar_load_map     = syncchar_load_map_callback;
      SIM_register_interface(pConfClass, "common_syncchar_interface", common_syncchar_iface);


      SIM_register_typed_attribute(
                                   pConfClass, "system",
                                   get_system_attribute, NULL,
                                   set_system_attribute, NULL,
                                   Sim_Attr_Required,
                                   "o", NULL,
                                   "The system to which this sync_char instance belongs");

      SIM_register_typed_attribute(
                                   pConfClass, "context",
                                   get_context_attribute, NULL,
                                   set_context_attribute, NULL,
                                   Sim_Attr_Required,
                                   "o", NULL,
                                   "A context private to this object");

      SIM_register_typed_attribute(
                                   pConfClass, "stats",
                                   get_stats_attribute, NULL,
                                   set_stats_attribute, NULL,
                                   Sim_Attr_Session,
                                   "s", NULL,
                                   "Write stats to log");

      pTimingInterface = MM_ZALLOC(1, timing_model_interface_t);
      pTimingInterface->operate = timing_operate;
      SIM_register_interface(pConfClass, TIMING_MODEL_INTERFACE, pTimingInterface);

      SIM_register_typed_attribute(
                                   pConfClass, "timing_model",
                                   get_timing_model, 0,
                                   set_timing_model, 0,
                                   Sim_Attr_Optional,
                                   "o|n", NULL,
                                   "Object listening on transactions coming from "
                                   "the syncchar module.");

      SIM_register_typed_attribute(
                                   pConfClass, "caches",
                                   get_caches, 0,
                                   set_caches, 0,
                                   Sim_Attr_Session,
                                   "[o*]", NULL,
                                   "per-cpu caches listening on the bus.");

      SIM_register_typed_attribute(
                                   pConfClass, "os_visibility",
                                   get_os_visibility, 0,
                                   set_os_visibility, 0,
                                   Sim_Attr_Optional,
                                   "D", NULL,
                                   "Operating System State.");

      SIM_register_typed_attribute(
                                   pConfClass, "stat_interval",
                                   get_stat_interval, 0,
                                   set_stat_interval, 0,
                                   Sim_Attr_Optional,
                                   "i", NULL,
                                   "vmstat collection interval. 0 = never.");

      SIM_register_typed_attribute(
                                   pConfClass, "log_init",
                                   get_configuration, 0,
                                   set_log, 0,
                                   Sim_Attr_Session,
                                   "s", NULL,
                                   "Writes the simulation config to the log.  Setting this value renames it.");
      SIM_register_typed_attribute(
                                   pConfClass, "procCycles",
                                   get_proc_cycles, 0,
                                   set_proc_cycles, 0,
                                   Sim_Attr_Optional,
                                   "[i*]", NULL,
                                   "Cycle count of each cpu when stats were last reset.");

      SIM_register_typed_attribute(
                                   pConfClass, "mapfile",
                                   get_mapfile, NULL,
                                   set_mapfile, NULL,
                                   Sim_Attr_Required,
                                   "s", NULL,
                                   "Syncchar map file");

      SIM_register_typed_attribute(
                                   pConfClass, "archived_worksets",
                                   get_archived_worksets, NULL,
                                   set_archived_worksets, NULL,
                                   Sim_Attr_Optional,
                                   "i", NULL,
                                   "Number of previous worksets to compare.");

      SIM_register_typed_attribute(
                                   pConfClass, "lockmap",
                                   get_lockmap, NULL,
                                   set_lockmap, NULL,
                                   Sim_Attr_Optional,
                                   "D", NULL,
                                   "Set of locks and their state.");


      SIM_register_typed_attribute(
                                   pConfClass, "cpu_switch_time",
                                   get_cpu_switch_time, 0,
                                   set_cpu_switch_time, 0,
                                   Sim_Attr_Optional,
                                   "i", NULL,
                                   "Sets the cpu switch time on an OSA_BENCHMARK magic instruction.");

      SIM_register_typed_attribute(
                                   pConfClass, "disk_delay",
                                   get_disk_delay, 0,
                                   set_disk_delay, 0,
                                   Sim_Attr_Optional,
                                   "f", NULL,
                                   "Sets the disk delay on an OSA_BENCHMARK magic instruction.");

      SIM_register_typed_attribute(
                                   pConfClass, "osatxm",
                                   get_osatxm, 0,
                                   set_osatxm, 0,
                                   Sim_Attr_Optional,
                                   "o|n", NULL,
                                   "Osatxm - so that syncchar can read tx memory.");

      SIM_register_typed_attribute(
                                   pConfClass, "log_worksets",
                                   get_logWorksets, 0,
                                   set_logWorksets, 0,
                                   Sim_Attr_Optional,
                                   "b", NULL,
                                   "Should syncchar log its worksets?");



      SIM_register_typed_attribute(pConfClass, "use_txcache",
                                   get_txcache_attribute, NULL,
                                   set_txcache_attribute, NULL,
                                   Sim_Attr_Optional,
                                   "i", NULL,
                                   "a flag for using the txcache");

      SIM_register_typed_attribute(
                                   pConfClass, "after_boot",
                                   get_afterBoot, 0,
                                   set_afterBoot, 0,
                                   Sim_Attr_Optional,
                                   "b", NULL,
                                   "Have we passed boot? (set manually if loading after a checkpoint)");




   }



} // extern "C"

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
