# MetaTM Project
# File Name: init-logs.py
#
# Description: Sets up the logfile names correctly.
#
# Operating Systems & Architecture Group
# University of Texas at Austin - Department of Computer Sciences
# Copyright 2006, 2007. All Rights Reserved.
# See LICENSE file for license terms.

@try:
  common = getattr(conf, simenv.common)
except:
  print 'kidding me ?'
else:
  common.log_init = common_log_tag
  common.log_init

@try:
  syncchar = getattr(conf, simenv.syncchar)
except:
  print 'syncchar module not loaded'
else:
  syncchar.log_init = syncchar_log_tag
  syncchar.log_init

@try:
  synccount = getattr(conf, simenv.synccount)
except:
  print 'synccount module not loaded'
else:
  synccount.log_init = synccount_log_tag
  synccount.log_init

@try:
  osatxm = getattr(conf, simenv.osatxm)
except:
  print 'osatxm module not loaded'
else:
  osatxm.log_init = osatxm_log_tag
  osatxm.log_init

@try:
  txcache = getattr(conf, 'dcache0')
except:
  print 'no cache: could not setup tx-cache log'
else:
  try:
    needtx = getattr(txcache, 'need_tx')
  except:
    print 'no tx support in cache objects'
  else:
    if needtx == 1:
      print 'setting up txcache log'
      txcache.log_init = txcache_log_tag
      txcache.log_init
    else:
      print 'needtx not set'
    
@common = getattr(conf, simenv.common)
@common.profile_log_prefix = profile_log

