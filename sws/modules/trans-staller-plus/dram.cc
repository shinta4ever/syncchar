// MetaTM Project
// File Name: dram.cc 
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

#include "dram.h"

dram_stat_t dram_stat;
mem_controller_t mem_controller;

unsigned mem_controller_element_t::get_bank_address (void)
{
	
	return (((( address >> MEM_COL_ADDR_BITS ) && MEM_BANK_ADDR_MASK ) ^ (address >> (PHYSICAL_ADDR_SIZE - CACHE_TAG_BITS))) & MEM_BANK_ADDR_MASK);
}

unsigned mem_controller_element_t::get_row_address (void)
{
	
	return ( (address >> (MEM_COL_ADDR_BITS + MEM_BANK_ADDR_BITS)) & MEM_ROW_ADDR_MASK);
}

void dram_stat_t::update_stat(mem_controller_element_t &mce)
{
	current_bank = mce.get_bank_address();
	current_rows[current_bank] = mce.get_row_address();
}



trans_time_t mem_controller_t::book_keeping (trans_time_t current_time, dram_stat_t dram_stat) 
{
	list <mem_controller_element_t>:: iterator i;
	trans_time_t next_complete_time = 0;

	for (i = L.begin(); i != L.end(); i++){
		if (i->get_complete_time() <= current_time) {
			dram_stat.update_stat(*i);
			L.erase(i);
		} else {
			next_complete_time = i->get_complete_time();
		}
	}
	/*return zero if there is no more outstanding memop in scheduler*/
	return next_complete_time;
}
trans_time_t mem_controller_t::insert_memeop (trans_time_t current_time, mem_controller_element_t &mce, dram_stat_t dram_stat)	
{
	list <mem_controller_element_t>:: iterator i;
	trans_time_t next_complete_time= 0;
	dram_stat_t dram_stat_speculated;

	next_complete_time = book_keeping(current_time, dram_stat);

	/*see if there is a pending memop whith same active bank and same row, if so insert the memop there and return the CAS delay*/
	dram_stat_speculated = dram_stat;
	int found_matched_mce = 0;
	for (i = L.begin(); i != L.end(); i++){
		if (!found_matched_mce && (mce.get_bank_address() == dram_stat_speculated.current_bank && 
		mce.get_row_address() == dram_stat_speculated.current_rows[dram_stat_speculated.current_bank])){
		/*insert the new memop here */
		mce.set_complete_time(i->get_complete_time());
		i->postpone(CAS_DELAY); 
		L.insert(i, mce);
		/*postpone the next memops from here to end*/
		} else if (found_matched_mce)
			i->postpone(CAS_DELAY);
	}			
	if (found_matched_mce) {
		i = L.end();
		return i->get_complete_time();
	}
	/*see if there is a pending memop whith same bank (not active) and same row, if so insert the memop there and return the CAS delay*/
	for (i = L.begin(); i != L.end(); i++){
		if (!found_matched_mce && (mce.get_bank_address() == i->get_bank_address() && 
		mce.get_row_address() == i->get_row_address() )){
		/*insert the new memop here */
		mce.set_complete_time(i->get_complete_time());
		i->postpone(CAS_DELAY+RAS_DELAY); 
		L.insert(i, mce);
		/*postpone the next memops from here to end*/
		} else if (found_matched_mce)
			i->postpone(CAS_DELAY+RAS_DELAY);
	}			
	if (found_matched_mce) {
		i = L.end();
		return i->get_complete_time();
	}
	/*no bank hit lets add the memop at the end of the memory controller list*/
	i = L.end();
	mce.set_complete_time(i->get_complete_time()+CAS_DELAY + RAS_DELAY + PRECHARGE_DELAY);
	L.insert(i, mce);
	return(mce.get_complete_time());
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
