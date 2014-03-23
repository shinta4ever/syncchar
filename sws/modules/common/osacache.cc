// MetaTM Project
// File Name: osacache.cc
//
// Description: 
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2006, 2007. All Rights Reserved.
// See LICENSE file for license terms.

#include "memaccess.h"
#include "osaassert.h"
#include "simulator.h"
#include "osacache.h"
#include "osacommon.h"

int gnCacheAttrs = -1;
const char ** gvszCacheStats = NULL;

// g-cache and tx-cache support a different
// set of statistics. Output of stats for
// caches is table-driven, so we just need
// different attribute tables for each. 
// -------------------------------------
// G-CACHE statistics attributes:
// -------------------------------------
int gnGCacheAttrs = 17;
const char * gvszGCacheStats[] = {
   "stat_copy_back",
   "stat_data_read",
   "stat_data_read_miss",
   "stat_data_write",
   "stat_data_write_miss",
   "stat_dev_data_read",
   "stat_dev_data_write",
   "stat_inst_fetch",
   "stat_inst_fetch_miss",
   "stat_lost_stall_cycles",
   "stat_mesi_exclusive_to_shared",
   "stat_mesi_invalidate",
   "stat_mesi_modified_to_shared",
   "stat_transaction",
   "stat_uc_data_read",
   "stat_uc_data_write",
   "stat_uc_inst_fetch",
   NULL
};
// -------------------------------------
// TX-CACHE statistics attributes:
// -------------------------------------
int gnTxCacheAttrs = 66;
const char * gvszTxCacheStats[] = {
  "stat_transactions",
  "stat_dev_data_read",
  "stat_dev_data_write",
  "stat_uc_inst_fetch",
  "stat_uc_data_read",

  "stat_uc_data_write",
  "stat_lost_stall_cycles",
  "stat_data_read_miss",
  "stat_inst_fetch_miss",
  "stat_tmesi_invalidate",

  "stat_tmesi_modified_to_shared",
  "stat_tmesi_shared_to_modified",
  "stat_tmesi_exclusive_to_shared",
  "stat_tmesi_tm_to_ts",
  "stat_tmesi_tm_to_tmi",

  "stat_tmesi_ts_to_tmi",
  "stat_tmesi_m_to_tm",
  "stat_tmesi_m_to_tmi",
  "stat_tmesi_m_to_ts",
  "stat_tmesi_exclusive_to_modified",

  "stat_tmesi_e_to_tmi",
  "stat_tmesi_e_to_te",
  "stat_tmesi_e_to_ts",
  "stat_tmesi_s_to_tmi",
  "stat_tmesi_s_to_ts",

  "stat_tmesi_tm_to_m",
  "stat_tmesi_tm_to_s",
  "stat_tmesi_tmi_to_m",
  "stat_tmesi_tmi_to_e",
  "stat_tmesi_tmi_to_s",

  "stat_tmesi_te_to_m",
  "stat_tmesi_te_to_e",
  "stat_tmesi_te_to_s",
  "stat_tmesi_te_to_tmi",
  "stat_tmesi_te_to_ts",

  "stat_tmesi_ts_to_m",
  "stat_tmesi_ts_to_e",
  "stat_tmesi_ts_to_s",
  "stat_conflicting_reads",
  "stat_conflicting_write",

  "stat_detected_conflicts",
  "stat_local_undetected_conflicts",
  "stat_predicted_conflicts",
  "stat_unpredicted_conflicts",
  "stat_asymmetric_conflicts",

  "stat_data_write",
  "stat_data_write_miss",
  "stat_write_back",
  "stat_overflow_count",
  "stat_ov_conflicts",

  "stat_data_read",
  "stat_inst_fetch",
  "stat_copy_back",
  "stat_txR_miss",  
  "stat_txR_hit",

  "stat_txW_miss",  
  "stat_txW_hit",
  "stat_tx_upgrade",
  "stat_tx_upgrade_noblock",
  "stat_tx_commits",

  "stat_tx_aborts",
  "stat_tx_references",
  "stat_tx_reads",
  "stat_tx_writes",
  "stat_stack_asymmetric_conflicts",

  "stat_tx_early_releases",

  NULL
};

const char * gvszTxsbStats[] = {

  "stat_tx_memops",
  "stat_tx_loads",
  "stat_tx_stores",
  "stat_tx_commits",
  "stat_tx_aborts",

  "stat_tx_overflows",
  "stat_tx_conflicts",
  "stat_tx_asymmetric_conflicts",
  "stat_tx_coherence_inv",
  "stat_memops",

  "stat_unstallable",
  "stat_loads",
  "stat_stores",
  "stat_coherence_inv",
  "stat_loads_from_q",

  "stat_tx_loads_from_q",
  "stat_repeat_stores",
  "stat_tx_repeat_stores",
  "stat_total_latency",
  "stat_cumulative_len",

  "stat_cumulative_deferred",
  "stat_enqueued_memops",
  "stat_tx_enqueued_memops",
  "stat_qfull_memops",
  "stat_tx_qfull_memops",

  "stat_max_qlen",
  "stat_min_qlen",
  "stat_qsamples",
  NULL

};

/**
 * initialize_cache_stats()
 * Tx-cache and g-cache report a different set of statistics. 
 * We drive the reporting with a global table of stats attributes, 
 * so we select the proper table based on whether we're using
 * a cache with transaction support. 
 */
void initialize_cache_stats(bool busetx) {
   gnCacheAttrs = busetx ? gnTxCacheAttrs : gnGCacheAttrs;
   gvszCacheStats = busetx ? gvszTxCacheStats : gvszGCacheStats;
}

/**
 * reset_one_cache_stats()
 * Reset (by brute force!) all the statistics
 * for a given cache pointed to by the conf_object_t *
 * pCache.
 */
void reset_one_cache_stats( system_component_object_t * pCache ) 
{
   integer_attribute_t av;
   int nCacheAttrs = 0;
   while( gvszCacheStats[nCacheAttrs] != NULL ) {
      //      av.u.integer = 0;
      //      av.kind = Sim_Val_Integer;
      av = INT_ATTRIFY(0);
      osa_sim_set_integer_attribute( pCache, gvszCacheStats[nCacheAttrs], &av );
      nCacheAttrs++;
   }
}

/**
 * reset_one_storebuffer_stats()
 * Reset all the statistics
 * for a given store buffer 
 */
void reset_one_storebuffer_stats(system_component_object_t * psb) {
   integer_attribute_t av;
   int nattrs = 0;
   while(gvszTxsbStats[nattrs]) {
      av = INT_ATTRIFY(0);
      osa_sim_set_integer_attribute(psb, gvszTxsbStats[nattrs], &av);
      nattrs++;
   }
}

/**
 * reset_cache_statistics()
 * Called when OSA_CLEARSTATS magic breakpoint
 * occurs--iterates over caches defined on the
 * timing model chain, and resets statistics for
 * devices on that chain. 
 */
void reset_cache_statistics( int nCaches, const char *prefix ) 
{
   int i;
   char szICacheName[OSATXM_BUFSIZE];
   char szDCacheName[OSATXM_BUFSIZE];
   char szL2CacheName[OSATXM_BUFSIZE];
   char szStorebufferName[OSATXM_BUFSIZE];
   system_component_object_t * picache = NULL;
   system_component_object_t * pdcache = NULL;
   system_component_object_t * pl2cache = NULL;
   system_component_object_t * pstorebuffer = NULL;
   for (i=0; i<nCaches; i++) {
      sprintf( szICacheName, "%sicache%d", prefix, i );
      sprintf( szDCacheName, "%sdcache%d", prefix, i );
      sprintf( szL2CacheName, "%sl2cache%d", prefix, i );
      sprintf( szStorebufferName, "%ssb%d", prefix, i );
      picache = osa_get_object_by_name( szICacheName );
      pdcache = osa_get_object_by_name( szDCacheName );
      pl2cache = osa_get_object_by_name( szL2CacheName );
      pstorebuffer = osa_get_object_by_name( szStorebufferName );
      if(picache != NULL)
         reset_one_cache_stats( picache );
      if(pdcache != NULL)
         reset_one_cache_stats( pdcache );
      if(pl2cache != NULL)
         reset_one_cache_stats( pl2cache );
      if(pstorebuffer != NULL) 
         reset_one_cache_stats( pstorebuffer );

      // Clear the general exception that may have arisen if the
      // caches don't exist
      //OSA_TODO: SimExc_General??
      if(osa_sim_get_error() == SimExc_General){
         osa_sim_clear_error();
      }
   }
   system_component_object_t * staller = NULL;
   if(NULL != (staller = osa_get_object_by_name("staller"))) {
      string_attribute_t av = STRING_ATTRIFY("");
      osa_sim_set_string_attribute(staller, "statistics", &av );
   }
}

/** 
 * dump_cache_stat_attrs()
 * Iterate over the list of cache stats attrs
 * available for caches, and print to the output
 * stream the cache name, attribute name, and attribute
 * value for the cache specified by the conf_object_t * pCache.
 */
void dump_cache_stat_attrs( ostream * pStream,
                            system_component_object_t * pCache,
                            char * lpszCacheName ) {
   // integer_attribute_t av;
   // integer_attribute_t *pAttrValue = &av;
   int nCacheAttrs = 0;
   while( gvszCacheStats[nCacheAttrs] != NULL ) {
      //av = osa_sim_get_integer_attribute( pCache, gvszCacheStats[nCacheAttrs] );
      int arg = osa_sim_get_integer_attribute( pCache, gvszCacheStats[nCacheAttrs] );
      *pStream << lpszCacheName << ": " << gvszCacheStats[nCacheAttrs];
      *pStream << ":\t" << arg << endl;
      nCacheAttrs++;

      //      SIM_free_attribute(av);

   }
}

/** 
 * tabular_cache_stat_attrs()
 * Iterate over the list of cache stats attrs
 * available for caches, and print to the output
 * stream the cache name, attribute name, and attribute
 * value for the cache specified by the conf_object_t * pCache.
 * Output is in tabular form, with the delimiter 
 * specified. 
 */
void tabular_cache_stat_attrs( ostream * pStream,
                               system_component_object_t * pCache,
                               char * lpszCacheName,
                               const char * lpszDelimiter,
                               bool bRowNames ) {
   //   integer_attribute_t av;
   //integer_attribute_t *pAttrValue = &av;
   int nCacheAttrs = 0;
   if( bRowNames )
      *pStream << lpszCacheName << lpszDelimiter;
   while( gvszCacheStats[nCacheAttrs] != NULL ) {
      //av = osa_sim_get_integer_attribute( pCache, gvszCacheStats[nCacheAttrs] );
      int arg = osa_sim_get_integer_attribute( pCache, gvszCacheStats[nCacheAttrs] );
      //*pStream << INTEGER_ARGUMENT << lpszDelimiter; 
      *pStream << arg << lpszDelimiter; 
      nCacheAttrs++;
      // osa_sim_free_attribute(av);
   }
   *pStream << endl;
}

/**
 * dump_cache_table_headers() 
 * Dump a delimiter-separated list of column
 * names. If the bRowNames flag is set, this
 * means the number of columns needs to be shifted
 * one to accomodate the extra column
 */
void dump_cache_table_headers( ostream * pStream,
                               const char * lpszDelimiter,
                               bool bRowNames,
                               bool bColNames ) {
   if( !bColNames )
      return;
   int nCacheAttrs = 0;
   if( bRowNames ) 
      *pStream << "" << lpszDelimiter;
   while( gvszCacheStats[nCacheAttrs] != NULL ) { 
      *pStream << gvszCacheStats[nCacheAttrs] << lpszDelimiter;
      nCacheAttrs++;
   }
   *pStream << endl;
}

/** 
 * dump_sb_stat_attrs()
 * Iterate over the list of storebuffer stats attrs
 * available for caches, and print to the output
 * stream the cache name, attribute name, and attribute
 * value for the cache specified by the conf_object_t * ptxsb.
 */
void dump_sb_stat_attrs(ostream * pStream,
                        system_component_object_t * ptxsb,
                        char * lpsz) {
   int nCacheAttrs = 0;
   while(gvszTxsbStats[nCacheAttrs] != NULL ) {
      int arg = osa_sim_get_integer_attribute( ptxsb, gvszTxsbStats[nCacheAttrs] );
      *pStream << lpsz << ": " << gvszTxsbStats[nCacheAttrs];
      *pStream << ":\t" << arg << endl;
      nCacheAttrs++;
   }
}

/** 
 * tabular_sb_stat_attrs()
 * Iterate over the list of storebuffer stats attrs
 * available for caches, and print to the output
 * stream the store buffer name, attribute name, and attribute.
 * Output is in tabular form, with the delimiter 
 * specified. 
 */
void tabular_sb_stat_attrs(ostream * pStream,
                           system_component_object_t * ptxsb,
                           char * lpsz,
                           const char * lpszDelimiter,
                           bool bRowNames) {
   int nCacheAttrs = 0;
   if( bRowNames )
      *pStream << lpsz << lpszDelimiter;
   while( gvszTxsbStats[nCacheAttrs] != NULL ) {
      int arg = osa_sim_get_integer_attribute(ptxsb, gvszTxsbStats[nCacheAttrs] );
      *pStream << arg << lpszDelimiter; 
      nCacheAttrs++;
   }
   *pStream << endl;
}

/**
 * dump_sb_table_headers() 
 * Dump a delimiter-separated list of column
 * names. If the bRowNames flag is set, this
 * means the number of columns needs to be shifted
 * one to accomodate the extra column
 */
void dump_sb_table_headers(ostream * pStream,
                           const char * lpszDelimiter,
                           bool bRowNames,
                           bool bColNames ) {
   if( !bColNames )
      return;
   int nCacheAttrs = 0;
   if( bRowNames ) 
      *pStream << "" << lpszDelimiter;
   while( gvszTxsbStats[nCacheAttrs] != NULL ) { 
      *pStream << gvszTxsbStats[nCacheAttrs] << lpszDelimiter;
      nCacheAttrs++;
   }
   *pStream << endl;
}


/**
 * dump_cache_statistics()
 * Called when OSA_DUMPSTATS magic breakpoint
 * occurs--iterates over caches defined on the
 * timing model chain, and dumps statistics for
 * devices on that chain to the ostream supplied. 
 */
void dump_cache_statistics( int nCaches,
                            ostream * pStream,
                            int bTabular,
                            const char * lpszDelim, 
                            bool bRowNames,
                            bool bColNames,
                            const char *prefix) {
   int i;
   char szsb[OSATXM_BUFSIZE];
   char szICacheName[OSATXM_BUFSIZE];
   char szDCacheName[OSATXM_BUFSIZE];
   char szL2CacheName[OSATXM_BUFSIZE];
   system_component_object_t * picache = NULL;
   system_component_object_t * pdcache = NULL;
   system_component_object_t * pl2cache = NULL;
   system_component_object_t * pstorebuffer = NULL;
   system_component_object_t * pstaller = NULL;
   for (i=0; i<nCaches; i++) {
      sprintf( szICacheName, "%sicache%d", prefix, i );
      sprintf( szDCacheName, "%sdcache%d", prefix, i );
      sprintf( szL2CacheName, "%sl2cache%d", prefix, i );
      picache = osa_get_object_by_name( szICacheName );
      pdcache = osa_get_object_by_name( szDCacheName );
      pl2cache = osa_get_object_by_name( szL2CacheName );
      *pStream << "Cache Statistics for CPU " << i << ": (";
      if(picache) *pStream << szICacheName << ","; 
      if(pdcache) *pStream << szDCacheName;
      if(pl2cache) *pStream << ", " << szL2CacheName;
      *pStream << ")" << endl; 
      *pStream << "--------------------------------------------------" << endl;
      if( bTabular ) {
         dump_cache_table_headers( pStream, lpszDelim, true, true ); 
         if(picache != NULL)
            tabular_cache_stat_attrs( pStream, picache, szICacheName, lpszDelim, true );
         if(pdcache != NULL){
            tabular_cache_stat_attrs( pStream, pdcache, szDCacheName, lpszDelim, true );
         }
         if(pl2cache != NULL)
            tabular_cache_stat_attrs( pStream, pl2cache, szL2CacheName, lpszDelim, true );
      } else {
         if(picache != NULL)
            dump_cache_stat_attrs( pStream, picache, szICacheName );
         if(pdcache != NULL)
            dump_cache_stat_attrs( pStream, pdcache, szDCacheName );
         if(pl2cache != NULL)
            dump_cache_stat_attrs( pStream, pl2cache, szL2CacheName );
      } 

      // Clear the general exception that may have arisen if the
      // caches don't exist
      if (osa_sim_get_error() != NO_ERROR){
         osa_sim_clear_error();
      }
   }
   *pStream << "--------------------------------------------------" << endl;
   for (i=0; i<nCaches; i++) {
      sprintf( szsb, "%ssb%d", prefix, i );
      pstorebuffer = osa_get_object_by_name( szsb );
      if(!pstorebuffer) 
         continue;

      *pStream << "Store Buffer Statistics for CPU " << i << ": (";
      if(pstorebuffer) *pStream << szsb;
      *pStream << ")" << endl; 
      *pStream << "--------------------------------------------------" << endl;
      if( bTabular ) {
         dump_sb_table_headers( pStream, lpszDelim, true, true ); 
         tabular_sb_stat_attrs( pStream, pstorebuffer, szsb, lpszDelim, true );
      } else {
         dump_sb_stat_attrs( pStream, pstorebuffer, szsb );
      } 

      // Clear the general exception that may have arisen if the
      // caches don't exist
      if (osa_sim_get_error() != NO_ERROR){
         osa_sim_clear_error();
      }
   }
   *pStream << "--------------------------------------------------" << endl;
   pstaller = osa_get_object_by_name("staller");
   if(pstaller)  {
      attr_value_t statsattr = SIM_get_attribute(pstaller, "statistics");
      string ststats = statsattr.u.string;
      *pStream << "Staller statistics" << endl;
      *pStream << ststats;
      *pStream << "Staller time series" << endl;
      attr_value_t epochlist;
      attr_value_t avlist = SIM_get_attribute(pstaller, "epoch_time_series");
      for(int i=0; i<avlist.u.list.size; i++) { 
         epochlist = avlist.u.list.vector[i];
         int epochsize = epochlist.u.list.size;
         *pStream << "[";
         for(int j=0; j<epochsize; j++) {
            *pStream << epochlist.u.list.vector[j].u.floating;
            if(j!=epochsize-1) *pStream << ",";
         }
         *pStream << "]" << endl;
      }
      *pStream << "--------------------------------------------------" << endl;
   }
}

/*
 * If the timing_model attribute is set, we need to
 * also pass the memory transaction on to it, and
 * add any cycle cost estimates it returns to the
 * current penalty. We cannot, however, return 0 and
 * expect consistent results. If the rest of the timing
 * chain fails to come up with anything reasonable,
 * return a 1 cycle penalty to ensure forward progress.
 */
osa_cycles_t 
collect_cache_timing( TIMING_SIGNATURE )
{
   int penalty = 0;
   osamod_t * pOsaTxm = (osamod_t *) pConfObject;

   /*
    * Here we distribute the memory transaction to 
    * any attribute/interface that is registered as
    * interested in the the timing_model interface
    * half of memory transactions. For osatxm, there
    * are two such attributes: 
    * 1) timing_model
    * 2) caches. 
    * ---------------------------
    * Collect mem-transaction penalty in cycles
    * for both (Both should probably not be set or 
    * we will over-charge for the memory operation!).
    */
   if( pOsaTxm->ppCaches != NULL 
       && !((pOsaTxm->type == SYNCCHAR ||
             pOsaTxm->type == SYNCCOUNT ||
             pOsaTxm->type == COMMON
             )// Don't "double-count" cycles if modules are chained
            && pOsaTxm->timing_model != NULL)) {
	    
      int nProcessor = pOsaTxm->minfo->getCpuNum(OSA_get_sim_cpu());
      int nCaches = pOsaTxm->nCaches;
	    
      if( nProcessor >= nCaches ) {
         cout << "collect_cache_timing: SEVERE ERROR: nCaches = " << nCaches << "\n";
      }
      if( pOsaTxm->tx_trace && false ) {
         cout << "mem tx src cpu:" << nProcessor << "-> cache timing chain\n";
      }
	    
      timing_model_interface_t * pTIfc = pOsaTxm->ppCachesIfc[nProcessor];
      conf_object_t * pCache = pOsaTxm->ppCaches[nProcessor];
      OSA_assert( pTIfc != NULL , pOsaTxm);
      OSA_assert( pCache != NULL , pOsaTxm);

      penalty += pTIfc->operate( TIMING_ARGUMENTS_1 );
   }

   if( pOsaTxm->timing_model != NULL ) {
      if( pOsaTxm->tx_trace ) {
         // Before uncommenting this, know that it build breaks on 64-bit..
         // fprintf( stdout, "pOsaTxm = %08x, g_osatxm = %08x\n", 
         //           (unsigned int) pOsaTxm, 
         //          (unsigned int) g_osatxm );
         // fprintf( stdout, "pOsaTxm->timing_ifc = %08x, pOsaTxm->timing_model = %08x\n", 
         //          (unsigned int) pOsaTxm->timing_ifc,
         //          (unsigned int) pOsaTxm->timing_model );
      }

      penalty += pOsaTxm->timing_ifc->operate( TIMING_ARGUMENTS_2 );
   }

   return penalty;
}
    
/**
 * set_timing_model()
 * set the timing model attribute 
 * which allows timing model chaining
 */
osa_attr_set_t
set_timing_model( SIMULATOR_SET_GENERIC_ATTRIBUTE_SIGNATURE )
{
   osamod_t *pOsaTxm = (osamod_t *) obj;
#ifdef _USE_SIMICS  //OSA_TODO What about QEMU?
   if( pAttrValue->kind == Sim_Val_Nil) {
      pOsaTxm->timing_model = NULL;
      pOsaTxm->timing_ifc = NULL;
   } else
#endif
      {
      pOsaTxm->timing_ifc = (timing_model_interface_t *)
         osa_sim_get_interface( GENERIC_ARGUMENT,
                            TIMING_MODEL_INTERFACE);
      if( osa_sim_clear_error() ) {
         //OSA_TODO
#ifdef _USE_SIMICS
         SIM_log_error( &pOsaTxm->log, 1,
                        "osatxm::set_timing_model: "
                        "object '%s' does not provide the timing model "
                        "interface.", pAttrValue->u.object->name );
#endif
         return INTERFACE_NOT_FOUND_ERR;
      }
      pOsaTxm->timing_model = GENERIC_ARGUMENT;
   }
   return ATTR_OK;
}
    
/**
 * get_timing_model()
 * get the timing model attribute 
 * which allows timing model chaining
 */
generic_attribute_t
get_timing_model( SIMULATOR_GET_ATTRIBUTE_SIGNATURE )
{
   osamod_t *pOsaTxm = (osamod_t *)obj;
   return GENERIC_ATTRIFY(pOsaTxm->timing_model);
}
    
/**
 * set_caches()
 * set the list of caches,
 * which is a simics list attribute 
 * containing per-cpu caches interested
 * in knowing about memory transactions 
 * the bus. 
 */
osa_attr_set_t  //OSA_TODO: Lots of work needed here
set_caches( SIMULATOR_SET_LIST_ATTRIBUTE_SIGNATURE )
{
   int i;
   generic_attribute_t *pCache = NULL;
   osamod_t * pOsaTxm = (osamod_t*) obj;
   int nCaches = LIST_ARGUMENT_SIZE;
	
   MM_FREE( pOsaTxm->ppCaches );
   MM_FREE( pOsaTxm->ppCachesIfc );
   pOsaTxm->nCaches = nCaches;
   pOsaTxm->ppCaches = MM_MALLOC( nCaches, system_component_object_t *);
   pOsaTxm->ppCachesIfc = MM_MALLOC( nCaches, timing_model_interface_t * ); 
	
   for( i=0; i<nCaches; i++ ) {
      pCache = &LIST_ARGUMENT(i);
      pOsaTxm->ppCachesIfc[i] = (timing_model_interface_t *)
         osa_sim_get_interface( GENERIC_ATTR(*pCache),
                            TIMING_MODEL_INTERFACE );
      if( osa_sim_clear_error()) {
#ifdef _USE_SIMICS
         SIM_log_error( &pOsaTxm->log, 1,
                        "set_caches: "
                        "object %d does not provide the timing model "
                        "listen interface.", i);
#else
         std::cout << "\nset caches: object "
                   << i <<" does not provide the timing model listen interface.";
#endif
         pOsaTxm->nCaches = 0;
         MM_FREE(pOsaTxm->ppCaches);
         MM_FREE(pOsaTxm->ppCachesIfc);
         pOsaTxm->ppCaches = NULL;
         pOsaTxm->ppCachesIfc = NULL;
         return INTERFACE_NOT_FOUND_ERR;
      }
      pOsaTxm->ppCaches[i] = pCache->u.object;
   }
	
   return ATTR_OK;
}
    
/**
 * get_caches()
 * get the list of caches,
 * which is a simics list attribute 
 * containing per-cpu cache objects interested
 * in knowing about memory transactions 
 * the bus. 
 */
list_attribute_t
get_caches( SIMULATOR_GET_ATTRIBUTE_SIGNATURE )
{
   int i;
   list_attribute_t avReturn;
   osamod_t * pOsaTxm = (osamod_t *) obj;
#ifdef _USE_SIMICS	
   avReturn.kind = Sim_Val_Nil;
#endif
   if( pOsaTxm->ppCaches ) {
      avReturn = osa_sim_allocate_list(pOsaTxm->nCaches);
      /*
      avReturn.kind = Sim_Val_List;
      avReturn.u.list.vector = MM_MALLOC( pOsaTxm->nCaches, attr_value_t);
      avReturn.u.list.size = pOsaTxm->nCaches;
      */
      for (i=0; i<pOsaTxm->nCaches; i++) {
         /*avReturn.u.list.vector[i].kind = Sim_Val_Object;
         avReturn.u.list.vector[i].u.object = pOsaTxm->ppCaches[i];
         */
         LIST_ATTR(avReturn, i) = GENERIC_ATTRIFY(pOsaTxm->ppCaches[i]);
      }
   }
   return avReturn;
}

/*
 * find_staller
 * chase through the cache hierarchy until we find a staller
 * object, or conclude that one is not present. Don't assume
 * a particular structure to the hierarchy, other than that
 * there must be a splitter at the cpu (for i/d).
 */
system_component_object_t * 
find_staller(system_component_object_t *pConfObject, int procNum) {
   osamod_t * osamod = (osamod_t *) pConfObject;
   system_component_object_t *pstorebuffer = NULL;   
   system_component_object_t *pcache = NULL;
   system_component_object_t *pstaller = NULL;
   system_component_object_t *psplitter = NULL;
   system_component_object_t *pbranch = NULL;
   if(NULL == osamod->ppCaches) return NULL;
   if(NULL == (psplitter = osamod->ppCaches[procNum]))
      goto clear_exception;
   if(NULL == (pbranch = osa_sim_get_generic_attribute(psplitter, "dbranch")))
      goto clear_exception;
   if(NULL == (pstorebuffer = osa_sim_get_generic_attribute(pbranch, "cache")))
      goto clear_exception;
   if(NULL == (pcache = osa_sim_get_generic_attribute(pstorebuffer, "timing_model")))
      goto clear_exception;
   do {
      /* follow timing_model connections until we find a staller */
      if(NULL == (pstaller = osa_sim_get_generic_attribute(pcache, "timing_model")))
         goto clear_exception;
      pcache = pstaller;
   } while(strncmp(pstaller->name, "staller", strlen("staller")));
   return pstaller;
clear_exception:
   if (osa_sim_get_error ()!= NO_ERROR)
      osa_sim_clear_error();   
   return NULL;
}

/*
 * find_l1_dcache
 * chase through the cache hierarchy until we find a staller
 * object, or conclude that one is not present. Don't assume
 * a particular structure to the hierarchy, other than that
 * there must be a splitter at the cpu (for i/d).
 */
system_component_object_t * 
find_l1_dcache(system_component_object_t *pConfObject, int procNum) {
   osamod_t * osamod = (osamod_t *) pConfObject;
   system_component_object_t *pstorebuffer = NULL;   
   system_component_object_t *pcache = NULL;
   system_component_object_t *psplitter = NULL;
   system_component_object_t *pbranch = NULL;
   if(NULL == osamod->ppCaches) return NULL;
   if(NULL == (psplitter = osamod->ppCaches[procNum]))
      goto clear_exception;
   if(NULL == (pbranch = osa_sim_get_generic_attribute(psplitter, "dbranch")))
      goto clear_exception;
   if(NULL == (pstorebuffer = osa_sim_get_generic_attribute(pbranch, "cache")))
      goto clear_exception;
   if(NULL == (pcache = osa_sim_get_generic_attribute(pstorebuffer, "timing_model")))
      goto clear_exception;
   return pcache;
clear_exception:
   if (osa_sim_get_error ()!= NO_ERROR)
      osa_sim_clear_error();   
   return NULL;
}

/*
 * hierarchy_entry
 * get a pointer to the object at the top
 * of the cache hierarchy.
 */
system_component_object_t * 
hierarchy_entry(system_component_object_t *pConfObject, int procNum) {
   osamod_t * osamod = (osamod_t *) pConfObject;
   system_component_object_t *psplitter = NULL;
   if(NULL == osamod->ppCaches) return NULL;
   if(NULL == (psplitter = osamod->ppCaches[procNum]))
      goto clear_exception;
   return psplitter;
clear_exception:
   if (osa_sim_get_error ()!= NO_ERROR)
      osa_sim_clear_error();   
   return NULL;
}

/*
 * memhier_exercise 
 * perform a load or store in the memory hierarchy--
 * this only exercises the traffic and computes latency. No data are
 * actually read or written to or from physmem. NB: caller must
 * provide logical to physical mapping!
 */
osa_cycles_t 
memhier_exercise(system_component_object_t * obj,
                 osa_logical_address_t laddr, 
                 osa_physical_address_t paddr,
                 unsigned int bytes, 
                 int procNum, 
                 bool isread) {
   osa_cycles_t latency = 0;
   timing_model_interface_t * ifc = NULL;
   system_component_object_t * top = hierarchy_entry(obj, procNum);
   
   if(top) {
      
      generic_transaction_t mt;
      memset(&mt, 0, sizeof mt);
      mt.logical_address = laddr;
      mt.physical_address = paddr;
      mt.size = bytes;
      mt.may_stall = 1;
      mt.ini_type = Sim_Initiator_CPU;
      mt.ini_ptr = (conf_object_t *) obj;
      mt.exception = Sim_PE_No_Exception;
      SIM_set_mem_op_type(&mt, isread ? Sim_Trans_Load : Sim_Trans_Store);
      
      ifc = (timing_model_interface_t *) SIM_get_interface(top,
                                                           TIMING_MODEL_INTERFACE);
      
      if(ifc) {
         latency += ifc->operate(top, NULL, NULL, &mt);
      }
      
      if(osa_sim_get_error ()!= NO_ERROR)
         osa_sim_clear_error();   
   }
   return latency;
}

/*
 * memhier_load()
 * perform a load in the memory hierarchy.
 */
osa_cycles_t 
memhier_load(system_component_object_t * obj,
             osa_logical_address_t laddr, 
             osa_physical_address_t paddr,
             unsigned int bytes, 
             int procNum) {
   return memhier_exercise(obj, 
                           laddr,
                           paddr,
                           bytes,
                           procNum,
                           true);
}

/*
 * memhier_store()
 * perform a store in the memory hierarchy.
 */
osa_cycles_t 
memhier_store(system_component_object_t * obj,
              osa_logical_address_t laddr, 
              osa_physical_address_t paddr,
              unsigned int bytes, 
              int procNum) {
   return memhier_exercise(obj, 
                           laddr,
                           paddr,
                           bytes,
                           procNum,
                           false);
}

/*
 * memhier_load_block
 * perform a load in the memory hierarchy with
 * given granularity--this only exercises the traffic 
 * and computes latency. No data are actually read from 
 * physmem. WARNING: if the memop crosses a cache line
 * boundary, this routine will miss it--achtung!
 */
osa_cycles_t 
memhier_load_block(system_component_object_t * obj,
                   osa_logical_address_t laddr, 
                   osa_physical_address_t paddr,
                   unsigned int granularity,
                   unsigned int bytes, 
                   int procNum) {
   return memhier_load(obj, 
                       laddr & ~(granularity-1),
                       paddr & ~(granularity-1),
                       granularity,
                       procNum);
}

/*
 * memhier_store_block
 * perform a store in the memory hierarchy with
 * given granularity--this only exercises the traffic 
 * and computes latency. No data are actually written to 
 * physmem. WARNING: if the memop crosses a cache line
 * boundary, this routine will miss it!
 */
osa_cycles_t 
memhier_store_block(system_component_object_t * obj,
                    osa_logical_address_t laddr, 
                    osa_physical_address_t paddr,
                    unsigned int granularity,
                    unsigned int bytes, 
                    int procNum) {
   return memhier_store(obj, 
                        laddr & ~(granularity-1),
                        paddr & ~(granularity-1),
                        granularity,
                        procNum);
}


/*
 * find_storebuffer
 * chase through the cache hierarchy until we find a storebuffer
 */
system_component_object_t * 
find_storebuffer(system_component_object_t *pConfObject, int procNum) {
   osamod_t * osamod = (osamod_t *) pConfObject;
   system_component_object_t *pstorebuffer = NULL;   
   system_component_object_t *psplitter = NULL;
   system_component_object_t *pbranch = NULL;
   if(NULL == osamod->ppCaches) return NULL;
   if(NULL == (psplitter = osamod->ppCaches[procNum]))
      goto clear_exception;
   if(NULL == (pbranch = osa_sim_get_generic_attribute(psplitter, "dbranch")))
      goto clear_exception;
   if(NULL == (pstorebuffer = osa_sim_get_generic_attribute(pbranch, "cache")))
      goto clear_exception;
   return pstorebuffer;
clear_exception:
   if (osa_sim_get_error ()!= NO_ERROR)
      osa_sim_clear_error();   
   return NULL;
}


int get_l1_dcache_size(system_component_object_t *pConfObject, int procNum) {
   int size = 0;
   osamod_t * osamod = (osamod_t *) pConfObject;
   if(!osamod->ppCaches) return size;
   conf_object_t *pdcache = find_l1_dcache(pConfObject, procNum);
   if (pdcache != NULL) {
      int line_number = osa_sim_get_integer_attribute(pdcache, "config_line_number");
      int line_size = osa_sim_get_integer_attribute(pdcache, "config_line_size");
      size = line_number * line_size;
   } else {
      if (osa_sim_get_error ()!= NO_ERROR) {
         osa_sim_clear_error();
      }
   }   
   return size;
}

unsigned int 
get_txcache_size(system_component_object_t *pConfObject, int procNum) {
   return get_l1_dcache_size(pConfObject, procNum);
}

unsigned int 
get_txcache_block_size(system_component_object_t *pConfObject, int procNum) {
   int size = 0;
   osamod_t * osamod = (osamod_t *) pConfObject;
   if(!osamod->ppCaches || !osamod->ppCaches[procNum]) return 0;
   conf_object_t * pdcache = find_l1_dcache(pConfObject, procNum);
   if (pdcache != NULL) {
      size = osa_sim_get_integer_attribute(pdcache, "config_line_size");
   } else {
      if(osa_sim_get_error ()!= NO_ERROR) {
         osa_sim_clear_error();
      }
   }   
   return size;
}

/*
 * get_cache_perturb
 * chase down the cache hierarchy until we find a staller,
 * which is assumed to be a trans-staller-plus, from which
 * we can extract the cache perturbation attributes. 
 * Technically, we could probably just use the global 
 * staller object, but this technique is more general,
 * and since we're only going to do this once at startup,
 * the performance hit of traversing the whole heirarchy
 * is not such a big deal.
 */
void get_cache_perturb(system_component_object_t *pConfObject, 
                       int procNum,
                       int *pcache_perturb,
                       int *pcache_seed) {
   osamod_t * osamod = (osamod_t *) pConfObject;
   system_component_object_t *pstaller = NULL;
   OSA_assert((pcache_perturb && pcache_seed), osamod);
   *pcache_perturb = *pcache_seed = 0;
   if(NULL == osamod->ppCaches) return;
   if(NULL == (pstaller = find_staller(pConfObject, procNum)))
      return;
   *pcache_perturb = osa_sim_get_integer_attribute(pstaller, "cache_perturb");
   *pcache_seed = osa_sim_get_integer_attribute(pstaller, "seed");
   return;
}

void enable_cache_perturb(system_component_object_t *pConfObject, int procNum){
   osamod_t * osamod = (osamod_t *) pConfObject;
   if(!osamod->nCaches) 
      return; // nothing to do!
   system_component_object_t *pstaller = NULL;
   boolean_attribute_t av = BOOL_ATTRIFY(1);
   if(NULL == osamod->ppCaches){
      cout << "Warning - cannot enable cache perturbation without the cache hierarchy" << endl;
      return;
   }
   if(NULL == (pstaller = find_staller(pConfObject, procNum)))
      return;
   if(ATTR_OK != osa_sim_set_boolean_attribute(pstaller, "past_bios", &av))
      goto clear_exception;
   return;
clear_exception:
   if (osa_sim_get_error ()!= NO_ERROR)
      osa_sim_clear_error();   
}
                         
osa_cycles_t commitTxCache(system_component_object_t *obj, 
                           int currentTxID) {
   osa_cycles_t latency = 0;
   abort_commit_interface_t * ifc = NULL;
   system_component_object_t * pdcache = NULL;
   system_component_object_t * pstorebuffer = NULL;
   osamod_t * osamod = (osamod_t *) obj;
   int procNum =  osamod->minfo->getCpuNum(OSA_get_sim_cpu());
   if(!osamod->use_txcache ||
      !osamod->ppCaches ||
      !osamod->ppCaches[procNum])
      goto clear_exception;
   if(NULL == (pdcache = find_l1_dcache(obj, procNum)))
      goto clear_exception;
   osamod->abort_commit_ifc = (abort_commit_interface_t *) osa_sim_get_interface(pdcache, "abort-commit-interface");
   latency += osamod->abort_commit_ifc->commitTx(pdcache, currentTxID);
   if(NULL == (pstorebuffer = find_storebuffer(obj, procNum)))
      goto clear_exception;
   ifc = (abort_commit_interface_t *) osa_sim_get_interface(pstorebuffer, "abort-commit-interface");
   latency += ifc->commitTx(pstorebuffer, currentTxID);
   return latency;
clear_exception:
   if (osa_sim_get_error ()!= NO_ERROR)
      osa_sim_clear_error();   
   return latency;
}

osa_cycles_t abortTxCache(conf_object_t * obj, int currentTxID, int procNum) {   
   osa_cycles_t latency = 0;
   osamod_t * osamod = (osamod_t *) obj;
   conf_object_t *pdcache = NULL;
   conf_object_t *pstorebuffer = NULL;
   abort_commit_interface_t * ifc = NULL;
   if(!osamod->use_txcache || !osamod->ppCaches) 
      goto clear_exception;
   if(NULL == (pdcache = find_l1_dcache(obj, procNum)))
      goto clear_exception;
   osamod->abort_commit_ifc = (abort_commit_interface_t *) osa_sim_get_interface(pdcache, "abort-commit-interface");
   latency += osamod->abort_commit_ifc->abortTx(pdcache, currentTxID);
   if(NULL == (pstorebuffer = find_storebuffer(obj, procNum)))
      goto clear_exception;
   ifc = (abort_commit_interface_t *) osa_sim_get_interface(pstorebuffer, "abort-commit-interface");
   latency += ifc->abortTx(pstorebuffer, currentTxID);
   return latency;
clear_exception:
   if (SIM_get_pending_exception ()!= NO_ERROR)
      SIM_clear_exception();   
   return latency;
}

void earlyRelease(system_component_object_t *obj, 
                  osa_physical_address_t paddr,
                  osa_logical_address_t laddr,
                  int currentTxID) {
   abort_commit_interface_t * ifc = NULL;
   system_component_object_t *pdcache = NULL;
   system_component_object_t *pstorebuffer = NULL;
   osamod_t * osamod = (osamod_t *) obj;
   int procNum =  osamod->minfo->getCpuNum(OSA_get_sim_cpu());
   if(!osamod->use_txcache ||
      !osamod->ppCaches ||
      !osamod->ppCaches[procNum])
      goto clear_exception;
   if(NULL == (pdcache = find_l1_dcache(obj, procNum)))
      goto clear_exception;
   osamod->abort_commit_ifc = (abort_commit_interface_t *) osa_sim_get_interface(pdcache, "abort-commit-interface");
   osamod->abort_commit_ifc->earlyRelease(pdcache, paddr, laddr, currentTxID);
   if(NULL == (pstorebuffer = find_storebuffer(obj, procNum)))
      goto clear_exception;
   ifc = (abort_commit_interface_t *) osa_sim_get_interface(pstorebuffer, "abort-commit-interface");
   ifc->earlyRelease(pstorebuffer, paddr, laddr, currentTxID);
   return;
clear_exception:
   if (osa_sim_get_error ()!= NO_ERROR)
      osa_sim_clear_error();   
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
