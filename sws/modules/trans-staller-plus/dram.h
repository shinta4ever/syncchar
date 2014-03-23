// MetaTM Project
// File Name: dram.h
//
// Description: This modules implements a simple
// first_ready DRAM scheduler with permutation based
// page interleaving. It will detect the DRAM row hit
// and return appropriate stall time to the staller module.  
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2008. All Rights Reserved.
// See LICENSE file for license terms.
#include <iostream>
#include <list>
#include <vector>

using namespace std;

#define CACHE_TAG_BITS 7
#define MEM_BANK_ADDR_BITS 2
#define MEM_COL_ADDR_BITS 11 
#define MEM_BANK_ADDR_MASK (1<<MEM_BANK_ADDR_BITS)-1
#define MEM_ROW_ADDR_BITS (PHYSICAL_ADDR_SIZE - MEM_COL_ADDR_BITS - MEM_BANK_ADDR_BITS)
#define MEM_ROW_ADDR_MASK (1<<MEM_ROW_ADDR_BITS)-1
#define MEM_NUM_OF_BANKS 1<<MEM_BANK_ADDR_BITS
#define PHYSICAL_ADDR_SIZE 32
#define UNDEFINED_ROW 0xFFFFFFFF 

#define CAS_DELAY 100  
#define RAS_DELAY 100 
#define PRECHARGE_DELAY 150
/* The above stuff are default values they will be parametrized in staller*/

typedef unsigned physical_address_t;
typedef unsigned trans_time_t;

typedef class mem_controller_element_t {
	unsigned int address;
	trans_time_t complete_time;
	public:
	trans_time_t get_complete_time () {return complete_time;};
	void set_complete_time (trans_time_t t) { complete_time = t;};	
	physical_address_t get_physical_address () {return address;};
	void set_physical_address (physical_address_t a) { address = a;};	
	unsigned get_bank_address (void);
	unsigned get_row_address (void);
	void postpone (trans_time_t delta) { complete_time += delta; };
};

typedef class dram_stat_t {
public:
	void drm_stat_t(void) { current_bank = 0; current_rows.assign(MEM_NUM_OF_BANKS, UNDEFINED_ROW);};
	unsigned current_bank;
	vector <unsigned> current_rows;	
	void update_stat (mem_controller_element_t &);
};

class mem_controller_t {	
	public:
	trans_time_t book_keeping (trans_time_t, dram_stat_t);
	trans_time_t insert_memeop (trans_time_t, mem_controller_element_t &, dram_stat_t);	
	list <mem_controller_element_t> L;
	
};
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
