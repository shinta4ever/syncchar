# MetaTM Project
# File Name: cachehierarchy.py
#
# Description: 
#
# Operating Systems & Architecture Group
# University of Texas at Austin - Department of Computer Sciences
# Copyright 2006, 2007. All Rights Reserved.
# See LICENSE file for license terms.

###################################################
# create a one level cache hierarchy 
# Heirarchy consists of split instruction/data
# L1 cache connected to a bus. 
# -------------------------------------------------
# If OSATXM module is connected, 
# its caches attribute will be connected
# -------------------------------------------------
# If synccount module is connected, it's 
# caches attribute will be connected.
# -------------------------------------------------
# This script expects the following symbols
# to be specified at global scope: (suggested vals)
# -------------------------------------------------
# l1cache_lines (512)
# l1cache_line_size (64)
# l1cache_assoc (8)
# l1cache_policy ('random')
# l1cache_penalty (0)
# main_mem_penalty (350)
###################################################
#instruction-fetch-mode instruction-fetch-trace

local $prefix = (get-component-prefix)
@system = getattr(conf, simenv.system)
@phys_mem0 = system.object_list['phys_mem']

@staller = pre_conf_object(simenv.prefix + 'staller', 'trans-staller-plus')
@staller.stall_time = main_mem_penalty
@staller.cache_perturb = int(simenv.cache_perturb_ns * .001 * simenv.freq_mhz)
@staller.seed = simenv.cache_seed
@i=0
@allComponents = [staller]
@cacheEntries = [ ]
@snoopingcaches = [ ]
@write_back_policy = 1
@write_allocate = 1
@log_level = 0

# get the current system from the $system variable,
# then enumerate its cpus
@for p in system.cpu_list:
  dcacheName = simenv.prefix + 'dcache' + str(i)
  icacheName = simenv.prefix + 'icache' + str(i)
  dsplitterName = simenv.prefix + 'd_ts' + str(i)
  isplitterName = simenv.prefix + 'i_ts' + str(i)
  idsplitterName = simenv.prefix + 'ids' + str(i)
  #################################
  # init dcache, connect to cpu
  #################################
  dc = pre_conf_object( dcacheName, 'g-cache' )
  dc.cpus = p
  dc.config_line_number = l1cache_lines
  dc.config_line_size = l1cache_line_size
  dc.config_assoc = l1cache_assoc
  dc.config_virtual_index = 0
  dc.config_virtual_tag = 0
  dc.config_replacement_policy = l1cache_policy
  dc.config_write_back = write_back_policy
  dc.config_write_allocate = write_allocate
  dc.penalty_read = l1cache_penalty
  dc.penalty_write = l1cache_penalty
  dc.penalty_read_next = 0
  dc.penalty_write_next = 0
  dc.timing_model = staller
  dc.log_level = log_level
  #################################
  # init icache, connect to cpu
  #################################
  ic = pre_conf_object( icacheName, 'g-cache' )
  ic.cpus = p
  ic.config_line_number = l1cache_lines
  ic.config_line_size = l1cache_line_size
  ic.config_assoc = l1cache_assoc
  ic.config_virtual_index = 0
  ic.config_virtual_tag = 0
  ic.config_replacement_policy = l1cache_policy
  ic.penalty_read = l1cache_penalty
  ic.penalty_write = l1cache_penalty
  ic.penalty_read_next = 0
  ic.penalty_write_next = 0
  ic.timing_model = staller
  #################################
  # d-cache, icache X splitter
  #################################
  dts = pre_conf_object( dsplitterName, 'trans-splitter' )
  its = pre_conf_object( isplitterName, 'trans-splitter' )
  idsplitter = pre_conf_object( idsplitterName, 'id-splitter' )
  dts.cache = dc
  dts.timing_model = dc
  dts.next_cache_line_size = l1cache_line_size
  its.cache = ic
  its.timing_model = ic
  its.next_cache_line_size = l1cache_line_size
  idsplitter.ibranch = its
  idsplitter.dbranch = dts
  ####################################
  # keep collecting components to add
  ####################################
  allComponents += [dts,its,idsplitter,dc,ic] 
  cacheEntries.append( idsplitter )
  snoopingcaches.append(dc)
  snoopingcaches.append(ic)
  print 'created single level cache hierarchy for CPU' + str(i)
  i = i + 1
###################################################
# L1 caches are coherent via # MESI protocol--
###################################################
@for l1cache in snoopingcaches :
  snoopSet = []
  for l1cache2 in snoopingcaches :
    if l1cache2 != l1cache :
      snoopSet.append( l1cache2 )
  l1cache.snoopers = snoopSet
@SIM_add_configuration( allComponents, None )
###################################################
# The current chain on the cache timing model
# (between the cpu and phys_mem0) is:
#
#   common->syncchar->synccount->osatxm
#
# synccount doesn't need timing operate because it should always be
# run with common.  We include it so that it can get cache stats in
# its log properly.  syncchar keeps its timing operate because it uses
# it to collect address sets.  Otherwise it could be removed.
# -------------------------------------------------
# If the common module is present, connect
# the cache heirarchy to it's caches attribute
# and connect it's timing model to phys_mem0.
###################################################
@commonPresent = 0
@try:
  common = getattr(conf, simenv.common)
except:
  print 'common module not connected'
else: 
  common.caches = cacheEntries
  phys_mem0.timing_model = common
  print ''
  print '\t-------------------------------------------------'
  print '\t- common module connected to cache hierarchy  -'
  print '\t-------------------------------------------------'
  print ''
  commonPresent = 1


###################################################
# If the syncchar module is present, connect
# the cache heirarchy to it's caches attribute
# and connect it's timing model to phys_mem0.
###################################################
@synccharPresent = 0
@try:
  syncchar = getattr(conf, simenv.syncchar)
except:
  print 'syncchar module not connected'
else: 
  syncchar.caches = cacheEntries
  if commonPresent:
    common.timing_model = syncchar
  else :
    phys_mem0.timing_model = syncchar
  print ''
  print '\t-------------------------------------------------'
  print '\t- syncchar module connected to cache hierarchy  -'
  print '\t-------------------------------------------------'
  print ''
  synccharPresent = 1


###################################################
# If the synccount module is present, connect
# the cache heirarchy to it's caches attribute
# and connect it's timing model to phys_mem0.
###################################################
@synccountPresent = 0
@try:
  synccount = getattr(conf, simenv.synccount)
except:
  print 'synccount module not connected'
else: 
  synccount.caches = cacheEntries
  if synccharPresent:
    syncchar.timing_model = synccount
  elif commonPresent:
    common.timing_model = synccount
  else :
    phys_mem0.timing_model = synccount
  print ''
  print '\t-------------------------------------------------'
  print '\t- synccount module connected to cache hierarchy  -'
  print '\t-------------------------------------------------'
  print ''
  synccountPresent = 1

###################################################
# If the OSATXM module is present, connect
# the cache heirarchy to it's caches attribute
# and connect it's timing model to phys_mem0.
###################################################
@osatxmPresent = 0
@try:
  osatxm = getattr(conf, simenv.osatxm)
except:
  print 'OSATXM module not connected'
else:
  osatxm.caches = cacheEntries
  if synccountPresent:
    synccount.timing_model = osatxm
  elif synccharPresent:
     syncchar.timing_model = osatxm
  elif commonPresent:
     common.timing_model = osatxm
  else :
    phys_mem0.timing_model = osatxm
  print ''
  print '\t-----------------------------------------------'
  print '\t- OSATXM module connected to cache hierarchy  -'
  print '\t-----------------------------------------------'
  print ''
  osatxmPresent = 1

#################################################
@if osatxmPresent <= 0 and commonPresent <= 0 and synccharPresent <= 0 :
  print '\t----------------------------------------'
  print '\t!       ---WARNING---' 
  print '\t! neither common nor osatxm are present!'
  print '\t! phys_mem.timing_interface will be connected'
  print '\t! to the first available cache chain--cache'
  print '\t! modeling for multiprocess is unlikely to be'
  print '\t! correct.'
  print '\t----------------------------------------'
  print ''
  phys_mem0.timing_model = getattr(conf, simenv.prefix + "ids0")

