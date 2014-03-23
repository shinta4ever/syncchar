/* =============================================================================
 *
 * pair.h
 *
 * =============================================================================
 *
 * Copyright (C) Stanford University, 2006.  All Rights Reserved.
 * Author: Chi Cao Minh
 *
 * =============================================================================
 *
 * For the license of kmeans, please see kmeans/LICENSE.kmeans
 * 
 * ------------------------------------------------------------------------
 * 
 * For the license of lib/mt19937ar.c and lib/mt19937ar.h, please see the
 * header of the files.
 * 
 * ------------------------------------------------------------------------
 * 
 * For the license of lib/rbtree.h and lib/rbtree.c, please see
 * lib/LEGALNOTICE.rbtree and lib/LICENSE.rbtree
 * 
 * ------------------------------------------------------------------------
 * 
 * Unless otherwise noted, the following license applies to STAMP files:
 * 
 * Copyright (c) 2007, Stanford University
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 * 
 *     * Neither the name of Stanford University nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY STANFORD UNIVERSITY ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL STANFORD UNIVERSITY BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * =============================================================================
 */


#ifndef PAIR_H
#define PAIR_H 1

#include "tm.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct pair {
    void* firstPtr;
    void* secondPtr;
} pair_t;


/* =============================================================================
 * pair_alloc
 * -- Returns NULL if failure
 * =============================================================================
 */
pair_t*
pair_alloc (void* firstPtr, void* secondPtr);


/* =============================================================================
 * Ppair_alloc
 * -- Returns NULL if failure
 * =============================================================================
 */
pair_t*
Ppair_alloc (void* firstPtr, void* secondPtr);


/* =============================================================================
 * TMpair_alloc
 * -- Returns NULL if failure
 * =============================================================================
 */
pair_t*
TMpair_alloc (TM_ARGDECL  void* firstPtr, void* secondPtr);


/* =============================================================================
 * pair_free
 * =============================================================================
 */
void
pair_free (pair_t* pairPtr);


/* =============================================================================
 * Ppair_free
 * =============================================================================
 */
void
Ppair_free (pair_t* pairPtr);


/* =============================================================================
 * TMpair_free
 * =============================================================================
 */
void
TMpair_free (TM_ARGDECL  pair_t* pairPtr);


/* =============================================================================
 * pair_swap
 * -- Exchange 'firstPtr' and 'secondPtr'
 * =============================================================================
 */
void
pair_swap (pair_t* pairPtr);


#define PPAIR_ALLOC(f,s)    Ppair_alloc(f, s)
#define PPAIR_FREE(p)       Ppair_free(p)

#define TMPAIR_ALLOC(f,s)   TMpair_alloc(TM_ARG  f, s)
#define TMPAIR_FREE(p)      TMpair_free(TM_ARG  p)


#ifdef __cplusplus
}
#endif


#endif /* PAIR_H */


/* =============================================================================
 *
 * End of pair.h
 *
 * =============================================================================
 */
