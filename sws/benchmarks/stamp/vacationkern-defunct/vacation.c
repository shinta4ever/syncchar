/* =============================================================================
 *
 * vacation.c
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

//#include <assert.h>
//#include <stdlib.h>
//#include <stdio.h>
//#include <getopt.h>
#include <linux/syscalls.h>

#ifdef CONFIG_TX_VACATION
#include "client.h"
#include "customer.h"
#include "list.h"
#include "manager.h"
#include "map.h"
#include "operation.h"
//#include "random.h"
#include "reservation.h"
#include "tm.h"
#include "types.h"
#include "utility.h"
#include "vacation.h"

struct task_struct ** vacation_threads;

enum param_types {
    PARAM_CLIENTS      = (unsigned char)'c',
    PARAM_NUMBER       = (unsigned char)'n',
    PARAM_QUERIES      = (unsigned char)'q',
    PARAM_RELATIONS    = (unsigned char)'r',
    PARAM_TRANSACTIONS = (unsigned char)'t',
    PARAM_USER         = (unsigned char)'u',
};

#define PARAM_DEFAULT_CLIENTS      (2)
#define PARAM_DEFAULT_NUMBER       (2)  /* used to be 10 */
#define PARAM_DEFAULT_QUERIES      (10) /* used to be 90 */
#define PARAM_DEFAULT_RELATIONS    (1 << 16)
#define PARAM_DEFAULT_TRANSACTIONS (1 << 16) /* used to be 26 !! */
#define PARAM_DEFAULT_USER         (80)

long global_params[256]; /* 256 = ascii limit */




/* =============================================================================
 * displayUsage
 * =============================================================================
 */
/*
static void
displayUsage (const char* appName)
{
    printf("Usage: %s [options]\n", appName);
    puts("\nOptions:                                             (defaults)\n");
    printf("    c <UINT>   Number of [c]lients                   (%i)\n",
           PARAM_DEFAULT_CLIENTS);
    printf("    n <UINT>   [n]umber of user queries/transaction  (%i)\n",
           PARAM_DEFAULT_NUMBER);
    printf("    q <UINT>   Percentage of relations [q]ueried     (%i)\n",
           PARAM_DEFAULT_QUERIES);
    printf("    r <UINT>   Number of possible [r]elations        (%i)\n",
           PARAM_DEFAULT_RELATIONS);
    printf("    t <UINT>   Number of [t]ransactions              (%i)\n",
           PARAM_DEFAULT_TRANSACTIONS);
    printf("    u <UINT>   Percentage of [u]ser transactions     (%i)\n",
           PARAM_DEFAULT_USER);
    exit(1);
}
*/

/* =============================================================================
 * setDefaultParams
 * =============================================================================
 */
static void
setDefaultParams (void)
{
    global_params[PARAM_CLIENTS]      = PARAM_DEFAULT_CLIENTS;
    global_params[PARAM_NUMBER]       = PARAM_DEFAULT_NUMBER;
    global_params[PARAM_QUERIES]      = PARAM_DEFAULT_QUERIES;
    global_params[PARAM_RELATIONS]    = PARAM_DEFAULT_RELATIONS;
    global_params[PARAM_TRANSACTIONS] = PARAM_DEFAULT_TRANSACTIONS;
    global_params[PARAM_USER]         = PARAM_DEFAULT_USER;
}


/* =============================================================================
 * parseArgs
 * =============================================================================
 */
/*
static void
parseArgs (long argc, char* const argv[])
{
    long i;
    long opt;

    opterr = 0;

    setDefaultParams();

    while ((opt = getopt(argc, argv, "c:n:q:r:t:u:")) != -1) {
        switch (opt) {
            case 'c':
            case 'n':
            case 'q':
            case 'r':
            case 't':
            case 'u':
                global_params[(unsigned char)opt] = atoi(optarg);
                break;
            case '?':
            default:
                opterr++;
                break;
        }
    }

    for (i = optind; i < argc; i++) {
        fprintf(stderr, "Non-option argument: %s\n", argv[i]);
        opterr++;
    }

    if (opterr) {
        displayUsage(argv[0]);
    }
}
*/

/* =============================================================================
 * initializeManager
 * =============================================================================
 */
static manager_t*
initializeManager (void)
{
    manager_t* managerPtr;
    long i;
    long numRelation;
//    random_t* randomPtr;
    long* ids;
    long t;
    long numTable;
    bool_t (*manager_add[])(manager_t*, long, long, long) = {
        &manager_addCar_seq,
        &manager_addFlight_seq,
        &manager_addRoom_seq
    };
    numTable = 3; // sizeof(manager_add) / sizeof(manager_add[0]);

//    printf("Initializing manager... ");
//    fflush(stdout);

//    randomPtr = random_alloc();
//    assert(randomPtr != NULL);
  printk( KERN_ERR "Mgr. about to manager_alloc");

    managerPtr = manager_alloc();
//    assert(managerPtr != NULL);

  printk( KERN_ERR "Mgr. about to plain_alloc relations");
    numRelation = (long)global_params[PARAM_RELATIONS];
    ids = (long*)vmalloc(numRelation * sizeof(long));
    for (i = 0; i < numRelation; i++) {
        ids[i] = i + 1;
    }

  printk( KERN_ERR "Mgr. about to populate tables..");
    for (t = 0; t < numTable; t++) {

        /* Shuffle ids */
        for (i = 0; i < numRelation; i++) {
	    long x, y, tmp;
            x = get_osa_random()  % numRelation;
//            long y = random_generate(randomPtr) % numRelation;
            y = get_osa_random() % numRelation;
            tmp = ids[x];
            ids[x] = ids[y];
            ids[y] = tmp;
        }

        /* Populate table */
        for (i = 0; i < numRelation; i++) {
            bool_t status;
	    long id, num, price;
            id = ids[i];
            num = ((get_osa_random() % 5) + 1) * 100;
            price = ((get_osa_random() % 5) * 10) + 50;
            status = manager_add[t](managerPtr, id, num, price);
//            assert(status);
        }

    } /* for t */

//    puts("done.");
//    fflush(stdout);

//    random_free(randomPtr);
  printk( KERN_ERR "Mgr. about to plain_free..");
    vfree(ids);

    return managerPtr;
}


/* =============================================================================
 * initializeClients
 * =============================================================================
 */
static client_t**
initializeClients (manager_t* managerPtr)
{
//    random_t* randomPtr;
    client_t** clients;
    long i, numClient, numTransaction, numTransactionPerClient, numQueryPerTransaction, numRelation, percentQuery,
	queryRange, percentUser;
    numClient = (long)global_params[PARAM_CLIENTS];
    numTransaction = (long)global_params[PARAM_TRANSACTIONS];
    numQueryPerTransaction = (long)global_params[PARAM_NUMBER];
    numRelation = (long)global_params[PARAM_RELATIONS];
    percentQuery = (long)global_params[PARAM_QUERIES];
    percentUser = (long)global_params[PARAM_USER];

//    printf("Initializing clients... ");
//    fflush(stdout);

    //  randomPtr = random_alloc();
//    assert(randomPtr != NULL);

    clients = (client_t**)plain_malloc(numClient * sizeof(client_t*));
    // HER: FUck this floating point crap
    numTransactionPerClient = numTransaction / numClient;
    queryRange = (percentQuery * numRelation ) / 100;
//    assert(clients != NULL);
//    numTransactionPerClient = (long)((double)numTransaction / (double)numClient + 0.5);
//    queryRange = (long)((double)percentQuery / 100.0 * (double)numRelation + 0.5);

    for (i = 0; i < numClient; i++) {
        clients[i] = client_alloc(i,
                                  managerPtr,
                                  numTransactionPerClient,
                                  numQueryPerTransaction,
                                  queryRange,
                                  percentUser);
//        assert(clients[i]  != NULL);
    }

/*
    puts("done.");
    printf("    Transactions        = %li\n", numTransaction);
    printf("    Clients             = %li\n", numClient);
    printf("    Transactions/client = %li\n", numTransactionPerClient);
    printf("    Queries/transaction = %li\n", numQueryPerTransaction);
    printf("    Relations           = %li\n", numRelation);
    printf("    Query percent       = %li\n", percentQuery);
    printf("    Query range         = %li\n", queryRange);
    printf("    Percent user        = %li\n", percentUser);
    fflush(stdout);
*/

//    random_free(randomPtr);

    return clients;
}


/* =============================================================================
 * checkTables
 * -- some simple checks (not comprehensive)
 * -- dependent on tasks generated for clients in initializeClients()
 * =============================================================================
 */
void
checkTables (manager_t* managerPtr)
{
    long i, numRelation, numTable, t;
    long percentQuery, queryRange, maxCustomerId;
    MAP_T *customerTablePtr;
    MAP_T * tables[3];
    bool_t (*manager_add[])(manager_t*, long, long, long) = {
        &manager_addCar_seq,
        &manager_addFlight_seq,
        &manager_addRoom_seq
    };

    tables[0] = managerPtr->carTablePtr;
    tables[1] = managerPtr->flightTablePtr;
    tables[2] = managerPtr->roomTablePtr;

    numRelation = (long)global_params[PARAM_RELATIONS];
    customerTablePtr = managerPtr->customerTablePtr;
    numTable = sizeof(tables) / sizeof(tables[0]);

//    printf("Checking tables... ");
//    fflush(stdout);

    /* Check for unique customer IDs */
    // HER: Fuck this floating point crap
    percentQuery = global_params[PARAM_QUERIES];
    queryRange = (percentQuery * numRelation) / 100;
//    percentQuery = (long)global_params[PARAM_QUERIES];
//    queryRange = (long)((double)percentQuery / 100.0 * (double)numRelation + 0.5);
    maxCustomerId = queryRange + 1;
    for (i = 1; i <= maxCustomerId; i++) {
        if (MAP_FIND(customerTablePtr, i)) {
            if (MAP_REMOVE(customerTablePtr, i)) {
//                assert(!MAP_FIND(customerTablePtr, i));
            }
        }
    }

    /* Check reservation tables for consistency and unique ids */
    for (t = 0; t < numTable; t++) {
        MAP_T* tablePtr = tables[t];
        for (i = 1; i <= numRelation; i++) {
            if (MAP_FIND(tablePtr, i)) {
//                assert(manager_add[t](managerPtr, i, 0, 0)); /* validate entry */
                if (MAP_REMOVE(tablePtr, i)) {
//                    assert(!MAP_REMOVE(tablePtr, i));
                }
            }
        }
    }

//    puts("done.");
//    fflush(stdout);
}


/* =============================================================================
 * freeClients
 * =============================================================================
 */
static void
freeClients (client_t** clients)
{
    long i;
    long numClient = (long)global_params[PARAM_CLIENTS];

    for (i = 0; i < numClient; i++) {
        client_t* clientPtr = clients[i];
        client_free(clientPtr);
    }
}



/* =============================================================================
 * main
 * =============================================================================
 */


long vacationMain(unsigned long numThreads, unsigned long numTransactions)
{
    manager_t* managerPtr;
    client_t** clients;
    int i;
    int gNumThreads;
  printk( KERN_ERR "About to initialize semaphores");

    sema_init(&vacation_sem_bm_done, 1);
    sema_init(&vacation_sem_all_awake, 1);

  printk( KERN_ERR "About to setDefaultParams");

    setDefaultParams();
//    parseArgs(argc, (char** const)argv);

    global_params[PARAM_CLIENTS] = numThreads;
    global_params[PARAM_TRANSACTIONS] = numTransactions;

    gNumThreads = gtcount = numThreads;


//  printk( KERN_ERR "About to initializeManager");
    managerPtr = initializeManager();
//  printk( KERN_ERR "About to initializeClients");
    clients = initializeClients(managerPtr);
//  printk( KERN_ERR "About to allocate threads");


    vacation_threads = kmalloc(gNumThreads * sizeof(struct task_struct*),GFP_KERNEL);
    if(!vacation_threads)
    {
	printk(KERN_ERR "vacation cannot allocate thread array-buffer!\n");
	return 1;
    }
    memset(vacation_threads, 0, gNumThreads * sizeof(struct task_struct*));

//  printk( KERN_ERR "About to  launch threads");
    for (i = 0 ; i < gNumThreads ; i++)
    {
	vacation_threads[i] = kthread_run(client_run, 
					  (void*) clients[i],
					  "vacationthreadproc");
	printk(KERN_ERR "created thread #%d\n",i);
    }

    printk(KERN_ERR "main thread awaiting gtcount == 0\n");  
    while(gtcount) {
	msleep(50);
    }
    
    for (i = 0 ; i < gNumThreads ; i++)
    {
	kthread_stop(vacation_threads[i]);
    }

    printk(KERN_ERR "Vacation complete!\n");
    kfree(vacation_threads);

    checkTables(managerPtr);

    freeClients(clients);
    /*
     * TODO: The contents of the manager's table need to be deallocated.
     */
    // The above todo is in the original code
    manager_free(managerPtr);

    return 0;
}


asmlinkage long
sys_vacation_bm(unsigned long numThreads, unsigned long numTransactions)
{
  return vacationMain(numThreads, numTransactions);
}

#else /* CONFIG_TX_VACATION */
asmlinkage long
sys_vacation_bm(unsigned long numThreads, unsigned long numTransactions) 
{
  printk( KERN_ERR "Build does not support vacation \
                    benchmark...\n" );
  return 0;
}
#endif


/* =============================================================================
 *
 * End of vacation.c
 *
 * =============================================================================
 */
