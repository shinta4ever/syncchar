// MetaTM Project
// File Name: allochist.h
//
// Description: 
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2007. All Rights Reserved.
// See LICENSE file for license terms.

#ifndef ALLOCHIST_H
#define ALLOCHIST_H

#include <stdlib.h>

// #define LOG_ALLOC

#ifdef LOG_ALLOC

// how many stack frames back to tabulate data for
#define NUM_FRAMES 3

void *operator new(size_t);
void operator delete(void*);

void dump_alloc_hist();

#else

#define dump_alloc_hist() do{}while(0)

#endif

#endif
