/* =============================================================================
 *
 * contab.c
 * -- Representation of contingency table
 *
 * =============================================================================
 *
 * Copyright (C) Stanford University, 2006.  All Rights Reserved.
 * Author: Chi Cao Minh
 *
 * =============================================================================
 *
 * Reference:
 *
 * A. Moore and M.-S. Lee. Cached sufficient statistics for efficient machine
 * learning with large datasets. Journal of Artificial Intelligence Research 8
 * (1998), pp 67-91.
 *
 * =============================================================================
 *
 * For the license of bayes/sort.h and bayes/sort.c, please see the header
 * of the files.
 * 
 * ------------------------------------------------------------------------
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


#include <stdlib.h>
#include "adtree.h"
#include "contab.h"
#include "query.h"
#include "utility.h"
#include "vector.h"

struct contab_node {
    long index;
    long count;
    struct contab_node* zeroNodePtr;
    struct contab_node* oneNodePtr;
};


/* =============================================================================
 * allocNode
 * =============================================================================
 */
static contab_node_t*
allocNode (long index)
{
    contab_node_t* nodePtr;

    nodePtr = (contab_node_t*)malloc(sizeof(contab_node_t));
    if (nodePtr) {
        nodePtr->index   = index;
        nodePtr->count   = -1L;
        nodePtr->zeroNodePtr = NULL;
        nodePtr->oneNodePtr  = NULL;
    }

    return nodePtr;
}


/* =============================================================================
 * freeNode
 * =============================================================================
 */
static void
freeNode (contab_node_t* nodePtr)
{
    free(nodePtr);
}


/* =============================================================================
 * contab_alloc
 * =============================================================================
 */
contab_t*
contab_alloc ()
{
    contab_t* contabPtr;

    contabPtr = (contab_t*)malloc(sizeof(contab_t));
    if (contabPtr) {
        contabPtr->numVar = -1L;
        contabPtr->rootNodePtr = NULL;
    }

    return contabPtr;
}


/* =============================================================================
 * freeNodes
 * =============================================================================
 */
static void
freeNodes (contab_node_t* nodePtr)
{
    if (nodePtr) {
        freeNodes(nodePtr->zeroNodePtr);
        freeNodes(nodePtr->oneNodePtr);
        freeNode(nodePtr);
    }
}


/* =============================================================================
 * contab_free
 * =============================================================================
 */
void
contab_free (contab_t* contabPtr)
{
    freeNodes(contabPtr->rootNodePtr);
    free(contabPtr);
}


/* =============================================================================
 * makeNodes
 * =============================================================================
 */
static contab_node_t*
makeNodes (adtree_t* adtreePtr, long i, vector_t* wildcardQueryVectorPtr)
{
    long numQuery = vector_getSize(wildcardQueryVectorPtr);

    if (i >= numQuery) {
        contab_node_t* leafNodePtr = allocNode(-1);
        assert(leafNodePtr);
        long count = adtree_getCount(adtreePtr, wildcardQueryVectorPtr);
        leafNodePtr->count = count;
        return leafNodePtr;
    }

    query_t* queryPtr = (query_t*)vector_at(wildcardQueryVectorPtr, i);
    assert(queryPtr);
    contab_node_t* nodePtr = allocNode(queryPtr->index);
    assert(nodePtr);

    queryPtr->value = 0;
    nodePtr->zeroNodePtr = makeNodes(adtreePtr, (i + 1), wildcardQueryVectorPtr);

    queryPtr->value = 1;
    nodePtr->oneNodePtr = makeNodes(adtreePtr, (i + 1), wildcardQueryVectorPtr);

    queryPtr->value = QUERY_VALUE_WILDCARD;

    return nodePtr;
}


/* =============================================================================
 * contab_populate
 * -- query VectorPtr must be sorted by id
 * =============================================================================
 */
void
contab_populate (contab_t* contabPtr,
                 adtree_t* adtreePtr,
                 vector_t* queryVectorPtr)
{
    long numVar = adtreePtr->numVar;
    long numQuery = vector_getSize(queryVectorPtr);
    assert(numVar == numQuery);

    vector_t* wildcardQueryVectorPtr = vector_alloc(1);
    assert(wildcardQueryVectorPtr);
    long q;
    for (q = 0; q < numQuery; q++) {
        query_t* queryPtr = (query_t*)vector_at(queryVectorPtr, q);
        if (queryPtr->value == QUERY_VALUE_WILDCARD) {
            bool_t status = vector_pushBack(wildcardQueryVectorPtr,
                                            (void*)queryPtr);
            assert(status);
        }
    }

    contabPtr->rootNodePtr = makeNodes(adtreePtr, 0, wildcardQueryVectorPtr);

    contabPtr->count = adtree_getCount(adtreePtr, wildcardQueryVectorPtr);

    vector_free(wildcardQueryVectorPtr);

    long n = -1;
    contab_node_t* nodePtr;
    for (nodePtr = contabPtr->rootNodePtr;
         nodePtr != NULL;
         nodePtr = nodePtr->zeroNodePtr)
    {
        n++;
    }
    contabPtr->numVar = MAX(0, n);

}


/* =============================================================================
 * contab_getCount
 * -- query VectorPtr must be sorted by id
 * =============================================================================
 */
long
contab_getCount (contab_t* contabPtr, vector_t* queryVectorPtr)
{
    long numQuery = vector_getSize(queryVectorPtr);
    assert(numQuery == contabPtr->numVar);
    contab_node_t* nodePtr = contabPtr->rootNodePtr;
    assert(nodePtr);
    long q;
    for (q = 0; q < numQuery; q++) {
        query_t* queryPtr = (query_t*)vector_at(queryVectorPtr, q);
        nodePtr = ((queryPtr->value == 0) ?
                   nodePtr->zeroNodePtr : nodePtr->oneNodePtr);
    }
    assert(nodePtr);

    return nodePtr->count;
}


/* #############################################################################
 * TEST_CONTAB
 * #############################################################################
 */
#ifdef TEST_CONTAB

#include <stdio.h>


static void
printData (data_t* dataPtr)
{
    long numVar = dataPtr->numVar;
    long numRecord = dataPtr->numRecord;

    long r;
    for (r = 0; r < numRecord; r++) {
        printf("%4li: ", r);
        char* record = data_getRecord(dataPtr, r);
        assert(record);
        long v;
        for (v = 0; v < numVar; v++) {
            printf("%li", (long)record[v]);
        }
        puts("");
    }
}


static void
printQuery (vector_t* queryVectorPtr)
{
    printf("[");
    long q;
    long numQuery = vector_getSize(queryVectorPtr);
    for (q = 0; q < numQuery; q++) {
        query_t* queryPtr = (query_t*)vector_at(queryVectorPtr, q);
        printf("%li:%li ", queryPtr->index, queryPtr->value);
    }
    printf("]");
}


static long
countData (data_t* dataPtr, vector_t* queryVectorPtr)
{
    long count = 0;
    long numQuery = vector_getSize(queryVectorPtr);

    long r;
    long numRecord = dataPtr->numRecord;
    for (r = 0; r < numRecord; r++) {
        char* record = data_getRecord(dataPtr, r);
        bool_t isMatch = TRUE;
        long q;
        for (q = 0; q < numQuery; q++) {
            query_t* queryPtr = (query_t*)vector_at(queryVectorPtr, q);
            long queryValue = queryPtr->value;
            if ((queryValue != QUERY_VALUE_WILDCARD) &&
                ((char)queryValue) != record[queryPtr->index])
            {
                isMatch = FALSE;
                break;
            }
        }
        if (isMatch) {
            count++;
        }
    }

    return count;
}


static long
testContab (contab_node_t* nodePtr,
            long depth,
            long numContabVar,
            contab_t* contabPtr,
            adtree_t* adtreePtr,
            data_t* dataPtr,
            vector_t* queryVectorPtr)
{
    if (depth >= numContabVar) {
        printQuery(queryVectorPtr);
        long actual = contab_getCount(contabPtr, queryVectorPtr);
        long expected = countData(dataPtr, queryVectorPtr);
        printf(" actual=%li expected=%li\n", actual, expected);
        assert(actual == expected);
        return actual;
    }

    long count = 0L;

    query_t query;
    query.index = nodePtr->index;
    bool_t status = vector_pushBack(queryVectorPtr, (void*)&query);
    assert(status);

    query.value = 0;
    count += testContab(nodePtr->zeroNodePtr,
                        (depth + 1),
                        numContabVar,
                        contabPtr,
                        adtreePtr,
                        dataPtr,
                        queryVectorPtr);

    query.value = 1;
    count += testContab(nodePtr->oneNodePtr,
                        (depth + 1),
                        numContabVar,
                        contabPtr,
                        adtreePtr,
                        dataPtr,
                        queryVectorPtr);

    vector_popBack(queryVectorPtr);

    return count;
}


static void
makeQueries (adtree_t* adtreePtr,
             data_t* dataPtr,
             long i,
             vector_t* queryVectorPtr,
             long numQuery)
{
    if (i >= numQuery) {
        contab_t* contabPtr = contab_alloc();
        assert(contabPtr);
        contab_populate(contabPtr, adtreePtr, queryVectorPtr);
        vector_t* queryContabVectorPtr = vector_alloc(contabPtr->numVar);
        assert(queryContabVectorPtr);
        long count = testContab(contabPtr->rootNodePtr,
                                0,
                                contabPtr->numVar,
                                contabPtr,
                                adtreePtr,
                                dataPtr,
                                queryContabVectorPtr);
        printf("[contab] count=%li\n", contabPtr->count);
        assert(count == contabPtr->count);
        vector_free(queryContabVectorPtr);
        contab_free(contabPtr);
        return;
    }

    query_t query;
    query.index = i;
    bool_t status = vector_pushBack(queryVectorPtr, (void*)&query);
    assert(status);

    query.value = QUERY_VALUE_WILDCARD;
    makeQueries(adtreePtr, dataPtr, (i + 1), queryVectorPtr, numQuery);

    query.value = 0;
    makeQueries(adtreePtr, dataPtr, (i + 1), queryVectorPtr, numQuery);

    query.value = 1;
    makeQueries(adtreePtr, dataPtr, (i + 1), queryVectorPtr, numQuery);

    vector_popBack(queryVectorPtr);
 }


static void
test (long numVar, long numRecord)
{
    random_t* randomPtr = random_alloc();
    data_t* dataPtr = data_alloc(numVar, numRecord, randomPtr);
    assert(dataPtr);
    data_generate(dataPtr, 0, 10, 10);
    printData(dataPtr);
    adtree_t* adtreePtr = adtree_alloc();
    assert(adtreePtr);
    adtree_make(adtreePtr, dataPtr);

    long numQuery = numVar;
    vector_t* queryVectorPtr = vector_alloc(numQuery);
    assert(queryVectorPtr);
    makeQueries(adtreePtr, dataPtr, 0, queryVectorPtr, numQuery);

    adtree_free(adtreePtr);
    data_free(dataPtr);
    random_free(randomPtr);
}


int
main ()
{
    puts("Starting...");

    puts("Test 1:");
    test(4, 20);

    puts("Test 1:");
    test(8, 256);

    puts("All tests passed.");

    return 0;
}


#endif /* TEST_CONTAB */


/* =============================================================================
 *
 * End of contab.c
 *
 * =============================================================================
 */
