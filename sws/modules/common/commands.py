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

def state2string(state) :

    string  = ''

    # Running
    if(state == 0): return 'R'

    # Unknown
    if (state == 256): return'?'

    # Interruptible
    if(state & 0x1):
        string += 'I'

    # Uninterruptible
    if(state & 0x2):
        string += 'U'

    # Stopped
    if(state & 0x4):
        string += 'S'

    # Traced
    if(state & 0x8):
        string += 'T'

    # Zombie
    if(state & 0x10):
        string += 'Z'

    # Dead
    if(state & 0x20):
        string += 'D'

    # Noninteractive
    if(state & 0x40):
        string += 'N'

    # Dead
    if(state & 0x100):
        string += 'X'

    return string

# dump lock state
def process_info_cmd(obj):
    try:
        os = SIM_get_attribute(obj, "os_visibility")
        # Build a reverse mapping of current processes
        running = {}
        for cpu in os['current_process'] :
            running[os['current_process'][cpu]] = cpu
        
        #print running
        #print os['sprocs']
        print 'Process info:'
        for pid in os['sprocs'] :
            state = state2string(os['sprocs'][pid]['state'])
            if running.has_key(pid): 
                print '\t%d (%s) (%d) : %s - running on CPU %d' \
                    % (pid, state, \
                           os['sprocs'][pid]['syscall'], os['sprocs'][pid]['cmd'], running[pid])

            else :
                print '\t%d (%s) (%d) : %s' % \
                    (pid, state, \
                         os['sprocs'][pid]['syscall'], os['sprocs'][pid]['cmd'])
        
    except Exception, msg:
        print "Error printing process info:", msg

new_command("process_info", process_info_cmd,
            args = [],
            alias = "",
            type  = "common commands",
            short = "process info",
	    namespace = "common",
            doc = """
Shows information about the os processes (requires os visibility in kernel).
""")

#
# ------------------------ info -----------------------
#

def get_info(obj):
    # USER-TODO: Return something useful here
    return []

sim_commands.new_info_command('common', get_info)

#
# ------------------------ status -----------------------
#

def get_status(obj):
    # USER-TODO: Return something useful here
    return [("Internals",
             [("Attribute 'value'", SIM_get_attribute(obj, "disk_delay"))])]

sim_commands.new_status_command('common', get_status)
