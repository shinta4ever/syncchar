/* =============================================================================
 *
 * memory.c
 * -- Very simple pseudo thread-local memory allocator
 *
 * =============================================================================
 *
 * Copyright (C) Stanford University, 2006.  All Rights Reserved.
 * Author: Chi Cao Minh
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
 * For the license of ssca2, please see ssca2/COPYRIGHT
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


#include <assert.h>
#include <stdlib.h>
#include "memory.h"
#include "types.h"

#ifdef ORDERTM
#include "tm.h"
#include <sys/mman.h>
#endif

/* We want to use enum bool_t */
#ifdef FALSE
#  undef FALSE
#endif
#ifdef TRUE
#  undef TRUE
#endif


#define PADDING_SIZE 8
typedef struct block {
    long padding1[PADDING_SIZE];
    size_t size;
    size_t capacity;
    char* contents;
    struct block* nextPtr;
    long padding2[PADDING_SIZE];
} block_t;

typedef struct pool {
    block_t* blocksPtr;
    size_t nextCapacity;
    size_t initBlockCapacity;
    long blockGrowthFactor;
    long poolColor;
} pool_t;

struct memory {
    pool_t*** pools;
    long numThread;
    long numColors;
};

memory_t* global_memoryPtr = 0;
char * mmap_base_addr = 0;

#if 0
// #ifdef TX_OVERFLOW
void * color_malloc(long size, long color) {
  struct mmap_arg_struct margs;
  margs.addr = 0; 
  margs.len = size;
  margs.prot = PROT_READ | PROT_WRITE;
  margs.flags = MAP_ANON | MAP_SHARED;
  margs.fd = -1; 
  margs.offset = 0;
  return (void*) old_mmap_color(&margs, color);
};
void color_free(void * p, long color) {
  // TODO! to unmap everything correctly,
  // we need to keep a record of everything
  // we allocate and it's size, which is a 
  // hassle and won't contribute to measurements
  // since no stats on start-up/tear down are reported
}
#else 
void * color_malloc(long size, long color) {
  return malloc(size);
}
void color_free(void * p, long color) {
  free(p);
}
#endif

/* =============================================================================
 * allocBlock
 * -- Returns NULL on failure
 * =============================================================================
 */
static block_t*
allocBlock (size_t capacity, long color)
{
    block_t* blockPtr;

    assert(capacity > 0);

#ifdef ORDERTM
    if(_xgettxid()) {
        _xretry(NEED_EXCLUSIVE);
    }
#endif

    blockPtr = (block_t*)color_malloc(sizeof(block_t), color);
    if (blockPtr == NULL) {
        return NULL;
    }

    blockPtr->size = 0;
    blockPtr->capacity = capacity;
    blockPtr->contents = (char*)color_malloc((capacity / sizeof(char) + 1), color);
    if (blockPtr->contents == NULL) {
        return NULL;
    }
    blockPtr->nextPtr = NULL;

    return blockPtr;
}


/* =============================================================================
 * freeBlock
 * =============================================================================
 */
static void
freeBlock (block_t* blockPtr, long color)
{
  color_free(blockPtr->contents, color);
  color_free(blockPtr, color);
}


/* =============================================================================
 * allocPool
 * -- Returns NULL on failure
 * =============================================================================
 */
static pool_t*
allocPool(size_t initBlockCapacity, long blockGrowthFactor, long color)
{
    pool_t* poolPtr;

    poolPtr = (pool_t*)color_malloc(sizeof(pool_t), color);
    if (poolPtr == NULL) {
        return NULL;
    }

    poolPtr->initBlockCapacity =
        (initBlockCapacity > 0) ? initBlockCapacity : DEFAULT_INIT_BLOCK_CAPACITY;
    poolPtr->blockGrowthFactor =
        (blockGrowthFactor > 0) ? blockGrowthFactor : DEFAULT_BLOCK_GROWTH_FACTOR;

    poolPtr->blocksPtr = allocBlock(poolPtr->initBlockCapacity, color);
    if (poolPtr->blocksPtr == NULL) {
        return NULL;
    }

    poolPtr->nextCapacity = poolPtr->initBlockCapacity *
                            poolPtr->blockGrowthFactor;

    poolPtr->poolColor = color;
    return poolPtr;
}


/* =============================================================================
 * freeBlocks
 * =============================================================================
 */
static void
freeBlocks(block_t* blockPtr, long color)
{
    if (blockPtr != NULL) {
      freeBlocks(blockPtr->nextPtr, color);
      freeBlock(blockPtr, color);
    }
}


/* =============================================================================
 * freePool
 * =============================================================================
 */
static void
freePool (pool_t* poolPtr)
{
  freeBlocks(poolPtr->blocksPtr, poolPtr->poolColor);
  color_free(poolPtr, poolPtr->poolColor);
}


/* =============================================================================
 * memory_init
 * -- Returns FALSE on failure
 * =============================================================================
 */
bool_t
memory_init (long numThread, 
	     size_t initBlockCapacity, 
	     long blockGrowthFactor) {
  long numColors = 1;
  return memory_init_colors(numThread, 
			    initBlockCapacity, 
			    blockGrowthFactor, 
			    numColors);
}

/* =============================================================================
 * memory_init_colors
 * -- Returns FALSE on failure
 * =============================================================================
 */
bool_t
memory_init_colors(long numThread, 
		   size_t initBlockCapacity, 
		   long blockGrowthFactor, 
		   long numColors)
{
    long i, j;

    assert(numThread > 0);

    global_memoryPtr = (memory_t*)malloc(sizeof(memory_t));
    if (global_memoryPtr == NULL) {
        return FALSE;
    }
    
    global_memoryPtr->pools = (pool_t***)malloc(numThread * sizeof(pool_t**));
    if (global_memoryPtr->pools == NULL) {
        return FALSE;
    }

    for (i = 0; i < numThread; i++) {
      global_memoryPtr->pools[i] = (pool_t**)malloc(numColors*sizeof(pool_t*));
      if (global_memoryPtr->pools[i] == NULL) {
	return FALSE;
      }
    }

    for(i = 0; i < numThread; i++) {
      for(j = 0; j < numColors; j++) {
        global_memoryPtr->pools[i][j] = allocPool(initBlockCapacity, blockGrowthFactor, j);
        if(global_memoryPtr->pools[i][j] == NULL) {
	  return FALSE;
        }
      }
    }

    global_memoryPtr->numThread = numThread;
    global_memoryPtr->numColors = numColors;

    return TRUE;
}


/* =============================================================================
 * memory_destroy
 * =============================================================================
 */
void
memory_destroy (void)
{
    long i, j;
    long numThread = global_memoryPtr->numThread;
    long numColors = global_memoryPtr->numColors;

    for (i = 0; i < numThread; i++) {
      for (j = 0; j < numColors; j++) {
        freePool(global_memoryPtr->pools[i][j]);
      }
    }

    for (i = 0; i < numThread; i++) {
        free(global_memoryPtr->pools[i]);
    }

    free(global_memoryPtr->pools);
    free(global_memoryPtr);
}


/* =============================================================================
 * addBlockToPool
 * -- Returns NULL on failure, else pointer to new block
 * =============================================================================
 */
static block_t*
addBlockToPool (pool_t* poolPtr, long numByte)
{
    block_t* blockPtr;
    size_t capacity = poolPtr->nextCapacity;
    long blockGrowthFactor = poolPtr->blockGrowthFactor;

    if ((size_t)numByte > capacity) {
        capacity = numByte * blockGrowthFactor;
    }

    blockPtr = allocBlock(capacity, poolPtr->poolColor);
    if (blockPtr == NULL) {
        return NULL;
    }

    blockPtr->nextPtr = poolPtr->blocksPtr;
    poolPtr->blocksPtr = blockPtr;
    poolPtr->nextCapacity = capacity * blockGrowthFactor;

    return blockPtr;
}


/* =============================================================================
 * getMemoryFromBlock
 * -- Reserves memory
 * =============================================================================
 */
static void*
getMemoryFromBlock (block_t* blockPtr, size_t numByte)
{
    size_t size = blockPtr->size;
    size_t capacity = blockPtr->capacity;

    assert((size + numByte) <= capacity);
    blockPtr->size += numByte;

    return (void*)&blockPtr->contents[size];
}


/* =============================================================================
 * getMemoryFromPool
 * -- Reserves memory
 * =============================================================================
 */
static void*
getMemoryFromPool (pool_t* poolPtr, size_t numByte)
{
    block_t* blockPtr = poolPtr->blocksPtr;

    if ((blockPtr->size + numByte) > blockPtr->capacity) {
#ifdef SIMULATOR
        assert(0);
#endif
        blockPtr = addBlockToPool(poolPtr, numByte);
        if (blockPtr == NULL) {
            return NULL;
        }
    }

    return getMemoryFromBlock(blockPtr, numByte);
}

/* =============================================================================
 * memory_prealloc
 * -- Makes sure there is enough memory in the pool for the upcoming tx to complete
 * =============================================================================
 */
#include <stdio.h>
void
memory_prealloc (long threadId, size_t numByte, long color)
{
  
    pool_t* poolPtr = global_memoryPtr->pools[threadId][color];
    block_t* blockPtr = poolPtr->blocksPtr;

    if ((blockPtr->size + numByte) > blockPtr->capacity) {
#ifdef SIMULATOR
        assert(0);
#endif
        blockPtr = addBlockToPool(poolPtr, numByte);
	//printf("Preallocated some memory\n");
    }
}

/* =============================================================================
 * memory_get
 * -- Reserves memory
 * =============================================================================
 */
void*
memory_get (long threadId, size_t numByte)
{
  return memory_get_color(threadId, numByte, 0);
}


/* =============================================================================
 * memory_get_color
 * -- Reserves memory
 * =============================================================================
 */
void*
memory_get_color(long threadId, size_t numByte, long color)
{
    pool_t* poolPtr;
    void* dataPtr;
    size_t addr;
    size_t misalignment;

    poolPtr = global_memoryPtr->pools[threadId][color];
    dataPtr = getMemoryFromPool(poolPtr, (numByte + 7)); /* +7 for alignment */

    /* Fix alignment for 64 bit */
    addr = (size_t)dataPtr;
    misalignment = addr % 8;
    if (misalignment) {
        addr += (8 - misalignment);
        dataPtr = (void*)addr;
    }
    return dataPtr;
}


/* =============================================================================
 * TEST_MEMORY
 * =============================================================================
 */
#ifdef TEST_MEMORY


#include <stdio.h>

#define NUM_ALLOC (10)

char* mem0Array[NUM_ALLOC];


static void
printMemory (memory_t* memoryPtr)
{
    long i;

    for (i = 0; i < memoryPtr->numThread; i++) {
        pool_t* poolPtr = memoryPtr->pools[i];
        block_t* blockPtr;
        long j = 0;
        for (blockPtr = poolPtr->blocksPtr;
             blockPtr != NULL;
             blockPtr = blockPtr->nextPtr)
        {
            j++;
        }
        for (blockPtr = poolPtr->blocksPtr;
             blockPtr != NULL;
             blockPtr = blockPtr->nextPtr)
        {
            printf("pool %li, block %li, size %u, capacity %u\n",
                   i, --j, (unsigned)blockPtr->size, (unsigned)blockPtr->capacity);
        }
    }
}


int
main ()
{
    memory_t* memoryPtr;
    long i;
    long size;

    puts("Starting tests...");

    assert(memory_init(1, 32, 2));
    memoryPtr = global_memoryPtr;

    size = 1;
    for (i = 0; i < NUM_ALLOC; i++) {
        long j;
        printf("Allocating %li bytes...\n", size);
        mem0Array[i] = (char*)memory_get(0, size);
        assert(mem0Array[i] != NULL);
        for (j = 0; j < size; j++) {
            mem0Array[i][j] = 'a' + (j % 26);
        }
        printMemory(memoryPtr);
        if (i % 2) {
            size = size * (i + 1);
        }
    }

    size = 1;
    for (i = 0; i < NUM_ALLOC; i++) {
        long j;
        printf("Checking allocation of %li bytes...\n", size);
        for (j = 0; j < size; j++) {
            assert(mem0Array[i][j] == 'a' + (j % 26));
        }
        if (i % 2) {
            size = size * (i + 1);
        }
    }

    memory_destroy();

    puts("All tests passed.");

    return 0;
}

#endif /* TEST_MEMORY */


/* =============================================================================
 *
 * End of memory.c
 *
 * =============================================================================
 */
