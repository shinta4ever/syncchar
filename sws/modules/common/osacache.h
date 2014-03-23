// MetaTM Project
// File Name: osacache.h
//
// Description: 
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006, 2007. All Rights Reserved.
// See LICENSE file for license terms.

#ifndef OSACACHE_H
#define OSACACHE_H

#define MINPENALTY   0
#define OSATXM_BUFSIZE 32
#define OSATXM_STATS_TABULAR    1
#define OSATXM_STATS_DELIMITER  ","
#define OSATXM_STATS_SHOWROWNAMES true
#define OSATXM_STATS_SHOWCOLNAMES true

using namespace::std;

extern int gnCacheAttrs;
extern const char ** gvszCacheStats;

void initialize_cache_stats(bool busetx);
void reset_one_cache_stats( system_component_object_t * pCache );
void reset_cache_statistics( int nCaches, const char *prefix );
void dump_cache_stat_attrs( ostream * pStream, 
                            system_component_object_t * pCache,
                            char * lpszCacheName );
void tabular_cache_stat_attrs( ostream * pStream,
                               system_component_object_t * pCache,
                               char * lpszCacheName,
                               const char * lpszDelimiter,
                               bool bRowNames );
void dump_cache_table_headers( ostream * pStream,
                               const char * lpszDelimiter,
                               bool bRowNames,
                               bool bColNames );
void dump_cache_statistics( int nCaches,
                            ostream * pStream,
                            int bTabular,
                            const char * lpszDelim, 
                            bool bRowNames,
                            bool bColNames,
                            const char *prefix);

osa_cycles_t collect_cache_timing( TIMING_SIGNATURE );

osa_cycles_t memhier_exercise(system_component_object_t * obj,
                              osa_logical_address_t laddr, 
                              osa_physical_address_t paddr,
                              unsigned int bytes, 
                              int procNum, 
                              bool isread);
osa_cycles_t memhier_load(system_component_object_t * obj,
                          osa_logical_address_t laddr, 
                          osa_physical_address_t paddr,
                          unsigned int bytes, 
                          int procNum);
osa_cycles_t memhier_store(system_component_object_t * obj,
                           osa_logical_address_t laddr, 
                           osa_physical_address_t paddr,
                           unsigned int bytes, 
                           int procNum);
osa_cycles_t memhier_load_block(system_component_object_t * obj,
                                osa_logical_address_t laddr, 
                                osa_physical_address_t paddr,
                                unsigned int granularity,
                                unsigned int bytes, 
                                int procNum);
osa_cycles_t memhier_store_block(system_component_object_t * obj,
                                 osa_logical_address_t laddr, 
                                 osa_physical_address_t paddr,
                                 unsigned int granularity,
                                 unsigned int bytes, 
                                 int procNum);

system_component_object_t * 
find_l1_dcache(system_component_object_t *pConfObject, int procNum);

osa_attr_set_t
set_timing_model( SIMULATOR_SET_GENERIC_ATTRIBUTE_SIGNATURE );

generic_attribute_t
get_timing_model( SIMULATOR_GET_ATTRIBUTE_SIGNATURE );

osa_attr_set_t
set_caches( SIMULATOR_SET_LIST_ATTRIBUTE_SIGNATURE );

list_attribute_t
get_caches( SIMULATOR_GET_ATTRIBUTE_SIGNATURE );


/*
 * None of these functions actually exist....

set_error_t set_cache_perturb(void *arg, system_component_object_t *obj,
                              attr_value_t *pAttrValue, attr_value_t *pAttrIdx);


attr_value_t get_cache_perturb( void *arg, system_component_object_t *obj,
                                attr_value_t *pAttrIdx);



set_error_t set_fresh_boot(void *arg, system_component_object_t *obj,
                           attr_value_t *pAttrValue, attr_value_t *pAttrIdx);

attr_value_t get_fresh_boot( void *arg, system_component_object_t *obj,
                             attr_value_t *pAttrIdx);
*/

int get_l1_dcache_size(system_component_object_t *pConfObject, int procNum);
unsigned int get_txcache_size(system_component_object_t *pConfObject, int procNum);
unsigned int get_txcache_block_size(system_component_object_t *pConfObject, int procNum);

void get_cache_perturb(system_component_object_t *pConfObject, 
                       int procNum,
                       int *pcache_perturb,
                       int *pcache_seed);
osa_cycles_t commitTxCache(system_component_object_t *obj, int currentTxID);
osa_cycles_t abortTxCache(conf_object_t *obj, int currentTxID, int procNum);
osa_cycles_t abortTxCache(conf_object_t *obj, int currentTxID, int procNum);
void earlyRelease(system_component_object_t *obj, 
                  osa_physical_address_t paddr,
                  osa_logical_address_t laddr,
                  int currentTxID);
void enable_cache_perturb(system_component_object_t *pConfObject, 
                          int procNum);

#endif

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
