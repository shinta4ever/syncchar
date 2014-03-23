# SyncChar Project
# File Name: commands.py
#
# Description: 
#
# Operating Systems & Architecture Group
# University of Texas at Austin - Department of Computer Sciences
# Copyright 2007. All Rights Reserved.
# See LICENSE file for license terms.


from cli import *
import sim_commands

def print_lock(lock, addr):
    print 'Lock %x:' % addr
    print '\t      state: %s' % lock['state']
    print '\t      lkval: %x' % lock['lkval']
    print '\t      queue: %d' % lock['queue']
    print '\t  lock type: %s' % lock['lock_id']
    print '\t spid owner: %s' % lock['spid_owner']
    print '\t  lock name: %s' % lock['name']
    print '\tworking set:'
    # Broken
    #for key in lock['worksets'].keys().sort():
    #    if lock['worksets'][key] == 1:
    #        print '\t\t %x r' % key
    #    elif lock['worksets'][key] == 2:
    #        print '\t\t %x w' % key
    #    else :
    #        print '\t\t %x rw' % key            
        
    print ''

# dump lock state
def dump_lock_cmd(obj, addr):
    try:
        lockmap = SIM_get_attribute(obj, "lockmap")
        print_lock(lockmap[addr], addr)
    except Exception, msg:
        print "Error dumping lock:", msg

new_command("dump_lock", dump_lock_cmd,
            args = [arg(int_t, "addr")],
            alias = "",
            type  = "sync_char commands",
            short = "dump lock",
	    namespace = "sync_char",
            doc = """
Increments the value by adding 1 to it.
""")

#
# ------------------------ info -----------------------
#

def get_info(obj):
    # USER-TODO: Return something useful here
    return []

sim_commands.new_info_command('sync_char', get_info)

#
# ------------------------ status -----------------------
#

def get_status(obj):
    # USER-TODO: Return something useful here
    return [("Internals",
             [("Attribute 'value'", SIM_get_attribute(obj, "disk_delay"))])]

sim_commands.new_status_command('sync_char', get_status)
