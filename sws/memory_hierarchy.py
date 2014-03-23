###################################################
# XMESI SMP hierarchy
#
# create a cache hierarchy using cache parameters
# Hierarchy consists of split instruction/data 
# L1 cache connected to private L2. This script
# will create i,d,l2 caches, connect them using
# an i/d splitter, and will connect the L2 snoopers
# attribute for MESI coherence.
# Uses tx-cache for both L1 and L2 caches
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
# l1cache_lines (256)
# l1cache_line_size (64)
# l1cache_assoc (4)
# l1cache_policy ('random')
# l1cache_penalty (0)
# l2cache_lines (65536)
# l2cache_line_size (64)
# l2cache_assoc (8)
# l2cache_policy ('random')
# l2cache_penalty (24)
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
@staller.frontbus_bandwidth_limit_bpc = simenv.frontbus_bandwidth_limit_bpc
@staller.frontbus_busy_penalty_factor = simenv.frontbus_busy_penalty_factor
@staller.frontbus_check_interval = simenv.frontbus_check_interval
@staller.frontbus_stat_interval = simenv.frontbus_stat_interval
@staller.frontbus_stat_size = simenv.frontbus_stat_size
@staller.verbose = simenv.frontbus_bw_data
@staller.stress_test = simenv.memhier_stress_test
@staller.stress_range = simenv.mainmem_stress_range
@i=0
@allComponents = [staller]
@cacheEntries = [ ]
@snoopingcaches = [ ]
@l1caches = [ ]
@l2caches = [ ]
@dcaches = [ ]
@icaches = [ ]
@storebuffers = [ ]
@stress_test = simenv.memhier_stress_test
@cache_stress_range = simenv.cache_stress_range
@check_inclusion = stress_test
@repair_inclusion = stress_test
@check_invariants = stress_test
@repair_invariants = stress_test
@ringlog = stress_test
@failstop = stress_test
@logmax = 10000
@verbosity = stress_test
@virtual_index = 0
@virtual_tag = 0
@sb_buffer_tx_writes = int(simenv.sb_buffer_tx_writes)
@sb_passthru = int(simenv.sb_passthru)
@sb_slots = int(simenv.sb_slots)
@sb_unbounded_tx_mode = int(simenv.sb_unbounded_tx_mode)
@l2present = int(simenv.multi_level_c)
@l2shared = int(simenv.use_txcmp)
@l2buffertx = not simenv.htm_model == 'flextm'
@l1buffertx = not simenv.logtm_mode and not simenv.htm_model == 'rock'
@l2needtx = not simenv.logtm_mode
@l1needtx = not simenv.logtm_mode
@protocol = simenv.cc_protocol
@includers = [ ]
@if(l2present and l2shared):
  #create the shared l2 cache
  l2cacheName = simenv.prefix + 'l2cache0'
  l2c = pre_conf_object( l2cacheName, 'tx' )
  l2c.config_line_number = l2cache_lines
  l2c.config_line_number = l2cache_lines
  l2c.config_line_size = l2cache_line_size
  l2c.config_size = l2cache_lines * l2cache_line_size
  l2c.config_bank_size = l2cache_lines * l2cache_line_size
  l2c.config_virtual_index = virtual_index
  l2c.config_virtual_tag = virtual_tag
  l2c.config_assoc = l2cache_assoc
  l2c.penalty_read = l2cache_penalty
  l2c.penalty_write = l2cache_penalty
  l2c.need_tx = l2needtx
  l2c.buffer_tx_writes = l2buffertx
  l2c.protocol = protocol
  l2c.config_cpuid = i
  l2c.cname = 'l2c0'
  l2c.timing_model = staller
  l2c.verbose = verbosity
  l2c.repair_inclusion = repair_inclusion
  l2c.repair_invariants = repair_invariants
  l2c.stress_test = stress_test
  l2c.stress_range = cache_stress_range
  includers.append( l2c )
  allComponents += [l2c]

# get the current system from the $system variable,
# then enumerate its cpus
@for p in system.cpu_list:
  if(not l2shared):
    includers = [ ]
  l2cacheName = simenv.prefix + 'l2cache' + str(i)
  dcacheName = simenv.prefix + 'dcache' + str(i)
  icacheName = simenv.prefix + 'icache' + str(i)
  dsplitterName = simenv.prefix + 'd_ts' + str(i)
  isplitterName = simenv.prefix + 'i_ts' + str(i)
  idsplitterName = simenv.prefix + 'ids' + str(i)
  sbname = simenv.prefix + 'sb' + str(i)
  if(l2present and not l2shared):
    #################################
    # init L2 cache, connect to cpu
    #################################
    l2c = pre_conf_object( l2cacheName, 'tx' )
    l2c.config_line_number = l2cache_lines
    l2c.config_line_number = l2cache_lines
    l2c.config_line_size = l2cache_line_size
    l2c.config_size = l2cache_lines * l2cache_line_size
    l2c.config_bank_size = l2cache_lines * l2cache_line_size
    l2c.config_virtual_index = virtual_index
    l2c.config_virtual_tag = virtual_tag
    l2c.config_assoc = l2cache_assoc
    l2c.penalty_read = l2cache_penalty
    l2c.penalty_write = l2cache_penalty
    l2c.need_tx = l2needtx
    l2c.buffer_tx_writes = l2buffertx
    l2c.protocol = protocol
    l2c.config_cpuid = i
    l2c.cname = 'l2c' + str(i)
    l2c.timing_model = staller
    l2c.repair_inclusion = repair_inclusion
    l2c.repair_invariants = repair_invariants
    l2c.stress_test = stress_test
    l2c.check_invariants = check_invariants
    l2c.stress_range = cache_stress_range
    includers.append( l2c )
  #################################
  # init dcache, connect to cpu
  #################################
  dc = pre_conf_object( dcacheName, 'tx' )
  dc.config_line_number = l1cache_lines
  dc.config_line_size = l1cache_line_size
  dc.config_size = l1cache_lines * l1cache_line_size
  dc.config_bank_size = l1cache_lines * l1cache_line_size
  dc.config_assoc = l1cache_assoc
  dc.config_virtual_index = virtual_index
  dc.config_virtual_tag = virtual_tag
  dc.need_tx = l1needtx
  dc.buffer_tx_writes = l1buffertx
  dc.protocol = protocol
  dc.config_cpuid = i
  dc.cname = 'dc' + str(i)
  dc.penalty_write = l1cache_penalty
  dc.penalty_read = l1cache_penalty
  dc.verbose = verbosity
  if(l2present):
    dc.timing_model = l2c
  else:
    dc.timing_model = staller
  dc.includers = includers
  dc.check_inclusion = check_inclusion 
  dc.repair_inclusion = repair_inclusion
  dc.check_invariants = check_invariants
  dc.repair_invariants = repair_invariants
  dc.stress_test = stress_test
  dc.stress_range = cache_stress_range
  #################################
  # init icache, connect to cpu
  #################################
  ic = pre_conf_object( icacheName, 'tx' )
  ic.config_line_number = l1cache_lines
  ic.config_line_size = l1cache_line_size
  ic.config_size = l1cache_lines * l1cache_line_size
  ic.config_bank_size = l1cache_lines * l1cache_line_size
  ic.config_assoc = l1cache_assoc
  ic.config_virtual_index = virtual_index
  ic.config_virtual_tag = virtual_tag
  ic.need_tx = 0
  ic.buffer_tx_writes = 0
  ic.protocol = 'xmesi'
  ic.config_cpuid = i
  ic.cname = 'ic' + str(i)
  ic.penalty_write = l1cache_penalty
  ic.penalty_read = l1cache_penalty
  ic.timing_model = l2c
  ic.includers = includers
  ic.check_inclusion = check_inclusion
  ic.repair_inclusion = repair_inclusion
  ic.check_invariants = check_invariants
  ic.repair_invariants = repair_invariants
  ic.stress_test = stress_test
  ic.stress_range = cache_stress_range
  if(l2present and not l2shared):
    l2c.hlc = [dc,ic]
  ###########################################
  # d-cache, icache X splitter, store buffer
  ###########################################
  dts = pre_conf_object( dsplitterName, 'trans-splitter' )
  its = pre_conf_object( isplitterName, 'trans-splitter' )
  idsplitter = pre_conf_object( idsplitterName, 'id-splitter' )
  sb = pre_conf_object( sbname, 'tx-store-buffer' )
  sb.read_latency = 0
  sb.write_latency = 0
  sb.buffer_tx_writes = sb_buffer_tx_writes
  sb.passthru = sb_passthru
  sb.blocksize = l1cache_line_size
  sb.unbounded_tx_mode = sb_unbounded_tx_mode
  sb.slots = sb_slots
  sb.timing_model = dc
  dc.hlc = [sb]
  dts.cache = sb
  dts.timing_model = sb
  dts.next_cache_line_size = l1cache_line_size
  its.cache = ic
  its.timing_model = ic
  its.next_cache_line_size = l1cache_line_size
  idsplitter.ibranch = its
  idsplitter.dbranch = dts
  ####################################
  # keep collecting components to add
  ####################################
  allComponents += [dts,its,idsplitter,l2c, dc, ic, sb] 
  cacheEntries.append( idsplitter )
  if(l2present and not l2shared):
    l2caches.append(l2c)
  dcaches.append(dc)
  icaches.append(ic)
  l1caches.append(dc)
  l1caches.append(ic)
  print 'created cache hierarchy for CPU' + str(i) + ' ' + 'protocol=xmesi'
  i = i + 1
###################################################
# set up snoopers. In an SMP, l2 caches snoop,
# in a CMP or one level hierarchy, l1s snoop.
###################################################
@if(l2present and l2shared):
  l2c.hlc = l1caches

@if(l2shared or not l2present):
  snoopingcaches = l1caches
else:
  snoopingcaches = l2caches

@for acache in l1caches :
  sibSet = []
  for acache2 in l1caches :
    if acache2 != acache :
      sibSet.append( acache2 )
  acache.siblings = sibSet

@for acache in l2caches :
  sibSet = []
  for acache2 in l2caches :
    if acache2 != acache :
      sibSet.append( acache2 )
  acache.siblings = sibSet

@for acache in snoopingcaches :
  snoopSet = []
  for acache2 in snoopingcaches :
    if acache2 != acache :
      snoopSet.append( acache2 )
      acache2.verbose = verbosity
      acache2.ringlog = ringlog
      acache2.failstop = failstop
      acache2.logmax = logmax
  acache.snoopers = snoopSet
@print 'adding components'
@SIM_add_configuration( allComponents, None )
###################################################
# The current chain on the cache timing model
# (between the cpu and phys_mem0) is:
#
#   common->syncchar->osatxm
#
# synccount doesn't need timing operate because it
# should always be run with common.  syncchar keeps
# its timing operate because it uses it to collect
# address sets.  Otherwise it could be removed.
# -------------------------------------------------
# If the syncchar module is present, connect
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

