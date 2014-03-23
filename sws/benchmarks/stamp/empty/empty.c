/* =============================================================================
 *
 * empty benchmark to measure fanin/out time for a parallel phase
 *
 * =============================================================================
 */


#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "timer.h"
#include "getUserParameters.h"
#include "globals.h"
#include "thread.h"
#include "tm.h"

void
work (void* argPtr)
{
    TM_THREAD_ENTER();
    volatile int boo;

    while(boo < 1000)
      boo++;

    TM_THREAD_EXIT();
}


MAIN(argc, argv)
{
    char exitmsg[1024];

    GOTO_REAL();

    load_syncchar_map("sync_char.map.empty");

    sprintf(exitmsg, "END BENCHMARK %s-parallel-phase\n", argv[0]);

    /* -------------------------------------------------------------------------
     * Preamble
     * -------------------------------------------------------------------------
     */

    /*
     * User Interface: Configurable parameters, and global program control
     */

    printf("\nEmpty benchmark:");
    printf("\nRunning...\n\n");

    getUserParameters(argc, (char** const) argv);

    SIM_GET_NUM_CPU(THREADS);
    TM_STARTUP(THREADS);
    P_MEMORY_STARTUP(THREADS);
    thread_startup(THREADS);

    puts("");
    printf("Number of processors:       %ld\n", THREADS);
    puts("");


    /* -------------------------------------------------------------------------
     * Kernel 1 - Graph Construction
     *
     * From the input edges, construct the graph 'G'
     * -------------------------------------------------------------------------
     */


    OSA_PRINT("entering parallel phase\n",0);
    START_INSTRUMENTATION();
    GOTO_SIM();
    thread_start(work, (void*)NULL);
    GOTO_REAL();
    OSA_PRINT("exiting parallel phase\n",0);
    OSA_PRINT(exitmsg,0);
    STOP_INSTRUMENTATION();
    return 0;
}


/* =============================================================================
 *
 * End of ssca2.c
 *
 * =============================================================================
 */
