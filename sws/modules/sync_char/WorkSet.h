// SyncChar Project
// File Name: WorkSet.h
//
// Description: 
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006, 2007. All Rights Reserved.
// See LICENSE file for license terms.

#ifndef WORKSET_H
#define WORKSET_H

#include "../common/simulator.h"
#include "../common/os.h"


using namespace std;

// SET_GRANULARITY should be a power of 2 that is greater than
// 4*sizeof(set_chunk) = (2^4)
// i.e. SET_ORDER must be greater than 4
// Because we are tight on memory these days, let's keep this at the
// minimum
typedef unsigned int set_chunk;
#define SET_ORDER          5
#define SET_GRANULARITY    (1<<SET_ORDER)

// number of bytes represented by a set_chunk (each byte represents 4)
#define CHUNK_BYTES        (sizeof(set_chunk)*4)

// number of bytes necessary to represent a whole set
#define BYTEMAP_BYTES      (SET_GRANULARITY/4)

// length of the bmap array
#define BYTEMAP_LEN        (BYTEMAP_BYTES/sizeof(set_chunk))

// mask to get set number and offset
#define BIT_MASK(x)        (~((1<<x) - 1))
#define RANGE_MASK         BIT_MASK(SET_ORDER)

// masks with all read or all write bits set
#define READ_MASK          0x55555555
#define WRITE_MASK         0xAAAAAAAA

struct ByteRange {
   set_chunk bmap[BYTEMAP_LEN];
};

class WorksetID {
 public:
  // address of corresponding lock
  unsigned int lock_addr;
  // generation count of the lock
  unsigned int lock_generation;
  // workset index for this lock - i.e. this is the nth workset
  // for this lock.  0xFFFFFFFF is used for the aggregate workset
  unsigned int workset_index;
  
  WorksetID(){
    WorksetID(0,0,0);
  }

  WorksetID(unsigned int lk_addr, 
	    unsigned int lk_gen, 
	    unsigned int ws_idx){
    lock_addr = lk_addr;
    lock_generation = lk_gen;
    workset_index = ws_idx;
  }

  bool operator==(WorksetID &other){
    if(this->lock_addr == other.lock_addr
       && this->lock_generation == other.lock_generation
       && this->workset_index == other.workset_index){
      return true;
    }
    return false;
  }
};

class WorkSet {
   public:
      WorkSet(unsigned int lk_addr, spid_t pd, unsigned int lk_generation,
	      unsigned int ws_index, int cpu);
      void grow(osa_logical_address_t addr, int len, osa_memop_basic_type_t type);

      int compare(WorkSet*, int*);  // returns the number of conflicting bytes
                                    // and puts the total size of this workset
                                    // in the second argument

      // Convert the workset into an attr_value_t (list of addresses
      // and r/w bits) for simics/debugging
      attr_value_t get_workset(); 
      void dumpWorkset();

      // Get the size of the workset
      int size();
      int rsize();
      int wsize();

      // the number of times this workset has been opened
      int cnt;

      //Unique id for this workset
      WorksetID id;

      // CPU number of processor we ran on
      // Assume just one for now
      int cpu;

      // The pid this workset came from
      spid_t pid;

      // This is only for runqueue locks, which are handed off across a
      // context switch.  We want to eliminate conflicts on both pids
      int twoowners;
      spid_t old_pid;

      // Does this workset include an IO operation?
      int IO;

      // Worksets we have waited on while trying to acquire a lock
      vector<struct WorksetID> contended_worksets;

      // Log the workset
      void logWorkset(ostream &out);

   private:
      map<osa_logical_address_t, ByteRange> set;
      typedef map<osa_logical_address_t, ByteRange>::iterator set_it;
};

#endif
