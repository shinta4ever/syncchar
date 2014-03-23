// SyncChar Project
// File Name: WorkSet.cc
//
// Description: 
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006, 2007. All Rights Reserved.
// See LICENSE file for license terms.

#include "WorkSet.h"

// counts the number of bits set
// x &= x - 1 clears the least significant set bit,
// so the loop executes the number of times there
// are set bits
inline int population(set_chunk x) {
   unsigned int pop = 0;
   for(pop = 0; x;  pop++)
      x &= x - 1;
   return pop;
}

WorkSet::WorkSet(unsigned int lk_addr, spid_t pd,
		 unsigned int lk_generation, 
		 unsigned int ws_index, int cpu) {
   cnt = 0;
   IO = 0;
   pid = pd;
   old_pid = pd;
   twoowners = 0;
   // Make sure we don't wrap around - unlikely
   if(ws_index == 0xfffffffe){
     cout << "XXX: Almost out of workset indices\n";
   }
   id = WorksetID(lk_addr, lk_generation, ws_index);  
   this->cpu = cpu;

}

void WorkSet::grow(osa_logical_address_t addr, int len, osa_memop_basic_type_t type) {
   osa_logical_address_t cur = addr;
   osa_logical_address_t end = addr+len;
   while(cur < end) {
      // get the extents of the byterange we want
      osa_logical_address_t range_start = cur & RANGE_MASK;

      // look up the corresponding map, put in a blank if not found
      ByteRange range;
      set_it range_it = set.find(range_start);
      if(range_it == set.end()) {
         memset(&range, 0, sizeof(ByteRange));
         pair<set_it, bool> inserted =
            set.insert(make_pair(range_start, range)); 
         range_it = inserted.first;
      }
      else {
         range = range_it->second;
      }

      // make a chunk with all reads or writes set
      set_chunk full_chunk;
      if(type == Sim_Trans_Load)
         full_chunk = READ_MASK;
      else if(type == Sim_Trans_Store)
         full_chunk = WRITE_MASK;
      else
         full_chunk = 0;

      unsigned int offset = cur & ~RANGE_MASK;
      unsigned int i = offset/CHUNK_BYTES;
      osa_logical_address_t chunk_start = range_start + i*CHUNK_BYTES;
      osa_logical_address_t chunk_end = chunk_start + CHUNK_BYTES;
      for(; i < BYTEMAP_LEN && cur < end; i++) {
         
         // mask off bits that aren't in this transaction
         set_chunk chunk_mask = (set_chunk)-1;
         if(addr > chunk_start)
            chunk_mask &=
               ~BIT_MASK(2*(8*sizeof(set_chunk) - (addr - chunk_start)));
         if(end < chunk_end)
            chunk_mask &= BIT_MASK(2*(chunk_end - end));

         range.bmap[i] |= full_chunk & chunk_mask;

         chunk_start = chunk_end;
         chunk_end += CHUNK_BYTES;

         cur = chunk_start;
      }
      
      // replace the updated map
      range_it->second = range;
   }
}

int WorkSet::compare(WorkSet *other, int *this_size) {
   set_it self_it = set.begin(); 
   set_it other_it = other->set.begin();

   int dependencies = 0;
   int size = 0;

   // iterate through the sets, comparing byteranges for identical
   // addresses
   while(self_it != set.end() && other_it != other->set.end()) {
      if(self_it->first < other_it->first) {
         // since self_it is advancing, add the size
         unsigned int i;
         for(i = 0; i < BYTEMAP_LEN; i++)
            size += population(
                  (self_it->second.bmap[i] & READ_MASK) | 
                  ((self_it->second.bmap[i] & WRITE_MASK) >> 1));
         self_it++;
      }
      else if(other_it->first < self_it->first)
         other_it++;
      else {
         ByteRange range1 = self_it->second;
         ByteRange range2 = other_it->second;
         set_chunk reads1, writes1, reads2, writes2, depends;
         unsigned int i, chunk_size;

         for(i = 0; i < BYTEMAP_LEN; i++) {
            reads1 = range1.bmap[i] & READ_MASK;
            writes1 = (range1.bmap[i] & WRITE_MASK) >> 1;

            chunk_size = population(reads1 | writes1);
            size += chunk_size;

            // if no bits are set, then the rest of the loop is of little
            // value
            if(!chunk_size)
               continue;

            reads2 = range2.bmap[i] & READ_MASK;
            writes2 = (range2.bmap[i] & WRITE_MASK) >> 1;

	    // RAW, WAR, and WAW hazards
            depends = (reads1 & writes2) | (reads2 & writes1) | (writes1 & writes2);

            // count the number of bits set
            dependencies += population(depends);
         }

         self_it++;
         other_it++;
      }
   }

   if(this_size != NULL)
      *this_size = size;
   return dependencies;
}

int WorkSet::size(){

  int size = 0;
  
  for(set_it iter = set.begin(); iter != set.end(); iter++){
    ByteRange range = iter->second;
    for(unsigned int i = 0; i < BYTEMAP_LEN; i++){
      set_chunk reads = range.bmap[i] & READ_MASK;
      set_chunk writes = (range.bmap[i] & WRITE_MASK) >> 1;
      
      size += population(reads | writes);
    }
  }

  return size;

}

int WorkSet::rsize(){

  int size = 0;
  
  for(set_it iter = set.begin(); iter != set.end(); iter++){
    ByteRange range = iter->second;
    for(unsigned int i = 0; i < BYTEMAP_LEN; i++){
      set_chunk reads = range.bmap[i] & READ_MASK;
      size += population(reads);
    }
  }

  return size;

}

int WorkSet::wsize(){

  int size = 0;
  
  for(set_it iter = set.begin(); iter != set.end(); iter++){
    ByteRange range = iter->second;
    for(unsigned int i = 0; i < BYTEMAP_LEN; i++){
      set_chunk writes = (range.bmap[i] & WRITE_MASK) >> 1;
      size += population(writes);
    }
  }

  return size;

}


// Get the workset as a simics attribute value
attr_value_t WorkSet::get_workset(){
  // We set up a dict with the address as the key and the value as 1
  // (read), 2 (write), 3 (rw)

  attr_value_t avReturn = SIM_alloc_attr_dict(size());
  int idx = 0;

  for(set_it iter = set.begin(); iter != set.end(); iter++){
    ByteRange range = iter->second;
    for(unsigned int i = 0; i < BYTEMAP_LEN; i++){
      set_chunk reads = range.bmap[i] & READ_MASK;
      set_chunk writes = (range.bmap[i] & WRITE_MASK) >> 1;

      for(unsigned int j = 0; j < 4 * sizeof(set_chunk); j++){
	if((reads & 1) || (writes & 1)){ // We got a hit 
	  osa_logical_address_t addr = iter->first;
	  addr += (4 * sizeof(set_chunk) * i) + ((4 * sizeof(set_chunk)) - 1 - j);
	  
	  char val = (reads & 1) ? 1 : 0;
	  if(writes & 1){
	    val |= 2;
	  }
	  
	  avReturn.u.dict.vector[idx].key = SIM_make_attr_integer(addr);
	  avReturn.u.dict.vector[idx].value = SIM_make_attr_integer(val);
	  idx++;
	}
	reads >>= 2;
	writes >>= 2;
      }
    }
  }

  return avReturn;
}

// Dump the workset to standard out for debugging
void WorkSet::dumpWorkset(){
  for(set_it iter = set.begin(); iter != set.end(); iter++){
    ByteRange range = iter->second;
    for(unsigned int i = 0; i < BYTEMAP_LEN; i++){
      set_chunk reads = range.bmap[i] & READ_MASK;
      set_chunk writes = (range.bmap[i] & WRITE_MASK) >> 1;

      for(unsigned int j = 0; j < 4 * sizeof(set_chunk); j++){
	if((reads & 1) || (writes & 1)){ // We got a hit 
	  osa_logical_address_t addr = iter->first;
	  addr += (4 * sizeof(set_chunk) * i) + ((4*sizeof(set_chunk)) - 1 - j);
	  
	  int val = (reads & 1) ? 1 : 0;
	  if(writes & 1){
	    val |= 2;
	  }

	  cout << std::hex <<  addr << std::dec << " - " << val << endl;	  
	}
	reads >>= 2;
	writes >>= 2;
      }
    }
  }
}

void WorkSet::logWorkset(ostream &out){
  out << "WS_CLOSE "
      << " " << std::hex << id.lock_addr << std::dec
      << " " << id.lock_generation
      << " " << id.workset_index 
      << " (" << pid;
  if(twoowners){
    out << " " << old_pid;
  }
  out << ") " << cpu << " C["; 
  for(vector<WorksetID>::iterator iter = contended_worksets.begin();
      iter != contended_worksets.end(); iter++){
    // We only wait on the same lock/generation
    out << " " << iter->workset_index;
  }
  out << " ]";
  if(IO){
    out << " IO";
  }
  out << endl;

  // Binary record format - starts on a new line
  char tmp_buf[(sizeof(set_chunk) * BYTEMAP_LEN) + sizeof(int)];

  char size[sizeof(int)];
  *(int *) size = set.size();
  out.write((char *) &size, sizeof(int));

  for(set_it iter = set.begin(); iter != set.end(); iter++){
    // Clear the buffer each time we use it
    memset(tmp_buf, 0, sizeof(tmp_buf));
    
    ByteRange range = iter->second;
    osa_logical_address_t addr = iter->first;
	   
    *((int *) tmp_buf) = addr;

    memcpy(tmp_buf + sizeof(int),
	   range.bmap, sizeof(range.bmap));
    
    out.write(tmp_buf, sizeof(tmp_buf));

  }
  out << "\n";

  //out.flush();
}

