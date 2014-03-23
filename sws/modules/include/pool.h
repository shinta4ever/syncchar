// MetaTM Project
// File Name: txsbline.h
//
// Description: 
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2007. All Rights Reserved.
// See LICENSE file for license terms.

#ifndef _POOL_H_
#define _POOL_H_
#include "../common/simulator.h"

static const int def_pool_size = 32;

#ifdef _DEBUG_POOL
#define checkpool() sanity_check()
#else 
#define checkpool()
#endif

template <typename T> class pool {
 private:
  list<T*> m_free;
  list<T*> m_allocated;
  int      m_increment;
  int      m_allocations;
  int      m_deallocations;
  void grow(int n) {
    for(int i=0;i<n;i++)
      m_free.push_back(new T());
  }
  void free_list(list<T*>& l) {
    while(!l.empty()) {
      T* p = l.front();
      l.pop_front();
      delete p;
    }
  }
  void sanity_check() {
    if((m_allocations % 100) == 1) {
      cout << "pool check:" 
	   << " a: " << allocated_count()
	   << " f: " << free_count()
	   << " ta: " << m_allocations
	   << " td: " << m_deallocations
	   << endl;
    }
  }
 public:
  pool<T>(int nsize) {
    m_allocations = 0;
    m_deallocations = 0;
    m_increment = nsize;
    grow(nsize);
  }
  pool<T>() {
    m_allocations = 0;
    m_deallocations = 0;
    m_increment = def_pool_size;
    grow(def_pool_size);
  }
  ~pool<T>() {
    free_list(m_free);
    free_list(m_allocated);   
  }
  T* allocate() {
    m_allocations++;
    if(m_free.empty())
      grow(m_increment);
    T * p = m_free.front();
    m_free.pop_front();
    m_allocated.push_front(p);
    checkpool();
    return p;
  }
  void pfree(T * p) {
    m_deallocations++;
    m_allocated.remove(p);
    m_free.push_front(p);
    checkpool();
  }
  int free_count() {
    return m_free.size();
  }
  int allocated_count() {
    return m_allocated.size();
  }
  int size() {
    return free_count() + allocated_count();
  }
};
#endif
