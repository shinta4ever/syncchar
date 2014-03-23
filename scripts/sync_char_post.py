#!/usr/bin/python
#########################################################
## Process a sync_char.log file to produce a report
##  on synchronization activity in the kernel.  Needs access
##  the kernel image (vmlinux).
import sys, re, os, pprint, getopt, math, array
from sync_common import *
objdump = 'objdump --disassemble --line-numbers '
nm = 'nm'
## XXX These must agree with values in sync_char.cc and sync_char_post.py
## Flags.  Properties of each locking routine
F_LOCK      = 0x1    # It locks
F_UNLOCK    = 0x2    # It unlocks
F_TRYLOCK   = 0x4    # It trys to get the lock
F_INLINED   = 0x8    # Inlined function
F_EAX       = 0x10   # Address in 0(%EAX)
F_TIME_RET  = 0x20   # Once this instruction executes, note time until
                     # ret (allowing for additional call/ret pairs
                     # before ret)
F_NOADDR    = 0x40   # No address for this lock, it is acq/rel at this pc

# Type of lock
L_SEMA      = 1
L_RSEMA     = 2
L_WSEMA     = 3
L_SPIN      = 4
L_RSPIN     = 5
L_WSPIN     = 6
L_MUTEX     = 7
L_COMPL     = 8
L_RCU       = 9
L_FUTEX     = 10

# Window size to use for (hotos-era) data independence calculations
HOTOS_DI_WINDOW_SIZE = 128

# Global switch for hotos data independence vs. asplos data independence
USE_HOTOS_DI = 0
# The size of the window
DI_WINDOW_SIZE = 512
# The amount to move the window between each round of sampling
DI_WINDOW_INCREMENT = 512
# The number of samples to take
DI_SAMPLE_SIZE = 1
# The target number of cpus
NUM_CPUS = {8 : 0,
            16 : 1,
            32 : 2}
NUM_CPUS_ARRAY = [8,16,32]
MAX_CPUS = 32
# Sampling rate, 0-100%
DI_SAMPLE_RATE = 100

# Seed the pseudo-random number generator
# Should probably re-run with different numbers for better results
import random

# Locks we want more detailed info about
important_locks = ['gKrwLock', 'gKrbtreeLock', 'gKprobLock', 'zone->lru_lock', \
                   'zone->lock', \
                   'journal_t->j_state_lock', 'journal_t->j_list_lock',\
                   'gKprobLoc', 'gKrbtreeL', 'globalLock'] # Handle truncated locks


# Don't use these, use the lock/unlock ids in the log file
sema_set = frozenset([L_SEMA])
rwsema_set = frozenset([L_RSEMA, L_WSEMA])
spin_lock_set = frozenset([L_SPIN])
rwspin_lock_set = frozenset([L_RSPIN, L_WSPIN])
mutex_set = frozenset([L_MUTEX])
completion_set = frozenset([L_COMPL])
rcu_set = frozenset([L_RCU])
futex_set = frozenset([L_FUTEX])


# Definition of a workset/manipulation functions
class WorkSet :
    '''The address set of a critical section'''

    READ_MASK  = 0x55555555
    WRITE_MASK = 0xAAAAAAAA
    SET_CHUNK_SIZE = 4

    def __init__ (self, lk_addr, pids, lk_generation,
                 ws_index, cpu, address_set, io) :
        self.lock_addr = lk_addr
        self.pids = set(pids)
        self.lock_generation = lk_generation
        self.workset_index = ws_index
        self.cpu = cpu
        self.address_set = address_set
        self.io = io
        self.mysize = self._size()
        self.mywsize = self._wsize()
        self.mysize_words = self._size_words()

    def population (self, x) :
        pop = 0
        while x :
            pop += 1
            x &= x - 1
        return pop

    # Use cached info, since address sets don't change at this stage
    # of the game
    def size(self) :
        return self.mysize

    def wsize(self) :
        return self.mywsize

    def size_words(self) :
        return self.mysize_words

    def _wsize(self) :
        size = 0
        for addr in self.address_set.keys() :
            myrange = self.address_set[addr]
            for i in xrange(len(myrange)) :
                writes = (myrange[i] & self.WRITE_MASK) >> 1
                size += self.population(writes)
        return size

    # Hack to get down to word granularity
    def _size_words (self) :
        size = 0
        for addr in self.address_set.keys() :
            myrange = self.address_set[addr]
            for i in xrange(len(myrange)) :
                reads = myrange[i] & self.READ_MASK
                writes = (myrange[i] & self.WRITE_MASK) >> 1
                union = reads | writes
                pop = self.population(union)
                if(pop) :
                    intersection = []
                    for j in xrange(4 * self.SET_CHUNK_SIZE) :
                        la = int(addr, 16) + (4* self.SET_CHUNK_SIZE * i) \
                             + ((4 * self.SET_CHUNK_SIZE) - 1 - j)

                        if(union & 1) :
                            # Round non-word aligned addrs
                            my_la = la
                            if (la % 4) != 0 :
                                my_la = la & 0xFFFFFFFC
                            tmp = '0x%x' % my_la 

                            if not tmp in intersection :
                                intersection.append(tmp)
                            else :
                                # Adjust the size of pop
                                pop -= 1

                        union >>= 2
                size += pop    
        return size

    def _size (self) :
        size = 0
        for addr in self.address_set.keys() :
            myrange = self.address_set[addr]
            for i in xrange(len(myrange)) :
                reads = myrange[i] & self.READ_MASK
                writes = (myrange[i] & self.WRITE_MASK) >> 1
                size += self.population(reads | writes)
        return size


    def compare (self, other, intersection, io) :
        dependences = 0
        # Just set dependences to 1 if either does IO
        if io and (self.io == 1 or other.io == 1):
            dependences = 1
            intersection.append('io')

        # Ignore address keys that aren't in both WS's
        my_addrs = set(self.address_set.keys())
        other_addrs = set(other.address_set.keys())
        shared_addrs = my_addrs & other_addrs

        # Iterate on the overlapping keys
        for addr in shared_addrs :
            range1 = self.address_set[addr]
            range2 = other.address_set[addr]

            for i in xrange(len(range1)) :
                reads1 = range1[i] & self.READ_MASK
                writes1 = (range1[i] & self.WRITE_MASK) >> 1

                reads2 = range2[i] & self.READ_MASK
                writes2 = (range2[i] & self.WRITE_MASK) >> 1

                depends = (reads1 & writes2) \
                          | (reads2 & writes1) \
                          | (writes1 & writes2)
                pop = self.population(depends)
                dependences += pop
                if(pop) :
                    for j in xrange(4 * self.SET_CHUNK_SIZE) :
                        la = int(addr, 16) + (4* self.SET_CHUNK_SIZE * i) \
                             + ((4 * self.SET_CHUNK_SIZE) - 1 - j)

                        if(depends & 1) :
                            # Round non-word aligned addrs
                            my_la = la
                            if (la % 4) != 0 :
                                my_la = la & 0xFFFFFFFC
                            tmp = '0x%x' % my_la 

                            if not tmp in intersection :
                                intersection.append(tmp)
                            else :
                                # Adjust the size of dependences
                                dependences -= 1

                        depends >>= 2

        assert(len(intersection) == dependences)
        return dependences

    def printWorkset (self) :
        readString = ''
        writeString = ''
        for addr in self.address_set.keys() :
            rnge = self.address_set[addr]
            for i in xrange(len(rnge)) :
                reads = rnge[i] & self.READ_MASK
                writes = (rnge[i] & self.WRITE_MASK) >> 1

                for j in xrange(4 * self.SET_CHUNK_SIZE) :
                    la = int(addr, 16) + (4* self.SET_CHUNK_SIZE * i) + ((4 * self.SET_CHUNK_SIZE) - 1 - j)

                    # Hack in word-alignment for jason's stuff
                    if (la % 4) == 0 :
                        if reads & 1 :
                            if len(readString) == 0 :
                                readString = '0x%x' % la
                            else :
                                readString += ',0x%x' % la

                        if writes & 1 :
                            if len(writeString) == 0 :
                                writeString = '0x%x' % la
                            else :
                                writeString += ',0x%x' % la

                    reads >>= 2
                    writes >>= 2
        return (readString, writeString)

    def printArffEntry (self) :
        (readString, writeString) = self.printWorkset();

        print '"%s",%s,%s,%s,%s,%d, R[%s] W[%s]' % (self.lock_addr, self.lock_generation,
                                                    self.workset_index, self.cpu,
                                                    self.pids.pop(), self.io,
                                                    readString, writeString)

# HotOS era data (in)dependence
def calculate_hotos_data_dependence (worksets, window_size):
    total_count = 0
    dependent_count = 0
    dependent_bytes = 0
    pct_dependent_bytes = 0.0
    dependent_count_thresh = 0.0
    #print 'worksets len = %d' % len(worksets)
    for i in xrange(len(worksets)) :
        workset_size = float(worksets[i].size())
        for j in xrange(window_size) :
            offset = i - j - 1
            if offset < 0: break
            # Filter locks from the same pid
            if worksets[i].pids & worksets[offset].pids :
                continue
            total_count += 1
            depends = worksets[i].compare(worksets[offset], [], True)
            if depends > 0 :
                dependent_count += 1
                dependent_bytes += depends
                pct_depend_bytes = 0.0
                if workset_size > 0 :
                    pct_depend_bytes = float(depends) / float(workset_size)
                    pct_dependent_bytes += pct_depend_bytes
                    dependent_count_thresh += 1

    # dependent acq's / total count of acquires (%)
    data_dependence = 0.0
    # mean dependent bytes per dependent acqs                
    dependent_bytes_per_acq = 0.0
    # mean percent of bytes conflicting per dependent acq
    dependent_bytes_per_acq_pct = 0.0

    #print 'total count = %f' % total_count
    #print 'dependent count = %f' % dependent_count
    #print 'dep bytes = %f' % dependent_bytes
    #print 'dep bytes pct = %f' % pct_dependent_bytes
    #print 'dep bytes pct thresh = %f' % dependent_count_thresh

    if total_count != 0 :
        data_dependence = float(dependent_count) / float(total_count)
    if dependent_count != 0 :
        dependent_bytes_per_acq = float(dependent_bytes) / float(dependent_count)
    if dependent_count_thresh != 0 :
        dependent_bytes_per_acq_pct = pct_dependent_bytes / float(dependent_count_thresh)

    return [ data_dependence, dependent_bytes_per_acq, dependent_bytes_per_acq_pct]

# Data independence as probability of conflict
# and Nouveau ASPLOS througput number
def calculate_new_data_independence (worksets_entry, histogram) :
    worksets = worksets_entry['worksets']
    max_threads = worksets_entry['max_threads']

    #window_start = worksets_entry['window_start']

    #window_end = min(window_start + DI_WINDOW_SIZE, len(worksets))

    # Sort by pid
    by_pid = {}
    #for ws in worksets[window_start: window_end] :
    for ws in worksets :
        # If there is more than 1, pick arbitrarily for now
        tmp_pid = ws.pids.pop()
        ws.pids.add(tmp_pid)
        if not by_pid.has_key(tmp_pid) :
            by_pid[tmp_pid] = []
        by_pid[tmp_pid].append(ws)
        # Make sure we track the max number of pids to use this lock
        max_threads.add(tmp_pid)

    # Repeat for a number of cpus
    for cpu_count in NUM_CPUS_ARRAY :
        sample_size = min(len(by_pid), cpu_count)
        
        # choose DI_SAMPLE_SIZE of sets of cpu_count
        for i in xrange(DI_SAMPLE_SIZE) :
            
            sample_pids = random.sample(by_pid.keys(), sample_size)
            sample_worksets = []
            for pid in sample_pids :
                tmp_ws = random.sample(by_pid[pid], 1)
                sample_worksets.append(tmp_ws.pop())
                
            # Debug printing:
            #print 'Round of comparisons, size = %d' % len(sample_worksets)
            #for j in xrange(len(sample_worksets)) :
            #    print 'Workset %d:' % j
            #    sample_worksets[j].printWorkset()

            # For each set:
            conflicting = 0
            nonconflicting = 0
            conflicting_ws = []
            for j in xrange(len(sample_worksets)) :
                # Compare all of the worksets
                for k in xrange(len(sample_worksets)) :
                    if j == k: continue
                    if sample_worksets[j].compare(sample_worksets[k], [], True) :
                        conflicting += 1
                        conflicting_ws.append(sample_worksets[j])
                        break
                else :
                    nonconflicting += 1

            # Calculate the "density" of the conflicts
            conflict_density = 0.0
            for x in conflicting_ws :
                local_conflicts = 0.0
                for y in conflicting_ws :
                    if x == y : continue
                    intersection = []
                    dep = x.compare(y, intersection, True)
                    if dep > 0 : 
                        local_conflicts += 1.0

                        assert(len(intersection) > 0), intersection

                        if histogram != None :
                            for entry in intersection :
                                if histogram.has_key(entry) : histogram[entry] += 1
                                else : histogram[entry] = 1
                            if histogram.has_key('total') : histogram['total'] += 1
                            else : histogram['total'] = 1
                        
                local_conflicts /= float(conflicting - 1)
                conflict_density += local_conflicts

            # Put conflicting/nonconflicting on the mean
            worksets_entry['avg_conflicts'][NUM_CPUS[cpu_count]] += conflicting
            worksets_entry['avg_completions'][NUM_CPUS[cpu_count]] += nonconflicting
            worksets_entry['samples'][NUM_CPUS[cpu_count]] += 1
            worksets_entry['sample_threads'][NUM_CPUS[cpu_count]] += float(sample_size)
            worksets_entry['conflict_density'][NUM_CPUS[cpu_count]] += conflict_density


    # Clean up entries we are done with
    #for i in xrange(window_start, min(window_start + DI_WINDOW_INCREMENT, window_end)) :
    #    try:
    #        worksets[i] = 0
    #    except :
    #        print '%d, %d' % i, len(worksets)

    # advance window by DI_WINDOW_INCREMENT
    #worksets_entry['window_start'] += DI_WINDOW_INCREMENT

        
def strip_underscore(key) :
    addr_re = re.compile(r'^(?P<addr>0x[a-fA-F0-9]+)')
    m = addr_re.match(key)
    if m:
        return m.group('addr')
    else :
        print 'Cant match addr %s' % key

def compute_average(total, count) :
    if count == 0:
      assert total == 0
      return 0.0
    return total/float(count)

# We don't really need this at the moment and I don't feel like fixing it
# def print_workset_av(mmap, keys, sort_key, ksym,  num_print, title, print_desc, cycles, nm_sym):
#    if len(keys) == 0:
#       return

#    keys.sort(lambda a,b : -cmp(
#       mmap[a][sort_key][1], mmap[b][sort_key][1] ))
#    num_print = min(len(keys), num_print)
#    top_keys = keys[:num_print]

#    print title
#    print 'Top %d of %d %s' % (num_print, len(keys), print_desc)
#    for key in top_keys:
#       if ksym.has_key(int(strip_underscore(key), 16)) :
#          key_str = ksym[int(strip_underscore(key),16)]['real'] + "::" + ksym[int(strip_underscore(key),16)]['cfl']
#       elif nm_sym.has_key(int(strip_underscore(key), 16)) :
#          key_str = nm_sym[int(strip_underscore(key),16)] + ' (' + key + ')'
#       elif mmap[key].has_key('lock_name') :
#          key_str = mmap[key]['lock_name'] + ' (' + key + ')'
#       else :
#          key_str = key + ' ()'

#       cnt_conflict = float(mmap[key][sort_key][0])
#       cnt_total = float(mmap[key][sort_key][9])
#       cnt_percent = float(mmap[key][sort_key][18])

#       print "  %.2f (sd:%.2f, avg_total:%.2f, avg_percent:%.2f): %s" % \
#         (compute_average(mmap[key][sort_key][1], cnt_conflict),
#          math.sqrt(math.fabs(
#             compute_average(mmap[key][sort_key][2], cnt_conflict) -
#             compute_average(mmap[key][sort_key][1]**2, cnt_conflict) )),
#          compute_average(mmap[key][sort_key][10], cnt_total),
#          compute_average(mmap[key][sort_key][19], cnt_percent),
#          key_str)

def print_map_av(mmap, keys, sort_key, ksym, num_print, title, print_desc, cycles, nm_sym, cpu_count, freq_mhz) :
    # 0 is count, 1 is sum (total cycles), divided is mean
    keys.sort(lambda a,b : -cmp( mmap[a][sort_key][1], mmap[b][sort_key][1]))
    num_print = min(len(keys), num_print)
    top_keys = keys[:num_print]
    key_sum = sum([mmap[key][sort_key][1] for key in keys])
    top_key_sum = sum([mmap[key][sort_key][1] for key in top_keys])
    top_acq_pid_sum = sum([mmap[key]['acq_pid'] for key in top_keys])
    # Calculate the average data independence and conflict density for
    # the entire kernel
    total_independent16 = 0.0
    total_independent32 = 0.0
    total_density16 = 0.0
    total_density32 = 0.0
    weight = 0.0
    for key in keys :
        weight = float(mmap[key][sort_key][1]) / float(key_sum)       
        total_independent16 += mmap[key]['average_completions'][NUM_CPUS[16]] * weight
        total_independent32 += mmap[key]['average_completions'][NUM_CPUS[32]] * weight
        total_density16 += mmap[key]['conflict_density'][NUM_CPUS[16]] * weight
        total_density32 += mmap[key]['conflict_density'][NUM_CPUS[32]] * weight
    if top_key_sum == 0 : return
    print title
    print 'Top %d of %d %s\n\t%d pids top is %.1f%% of total %.1f%% of cyc (sync %.1f%%)' %\
          (num_print, len(keys), print_desc,
           top_acq_pid_sum,
           100.0 * top_key_sum / key_sum,
           100.0 * top_key_sum / cycles,
           100.0 * key_sum / cycles )
    print '%s cycles (%.2f seconds) total' %\
          (commify(str(key_sum)), key_sum / (freq_mhz * 1000000 * cpu_count) )
    for key in top_keys :
        if ksym.has_key(int(strip_underscore(key), 16)) :
            key_str = ksym[int(strip_underscore(key),16)]['real'] + "::" + ksym[int(strip_underscore(key),16)]['cfl']
        elif nm_sym.has_key(int(strip_underscore(key), 16)) :
            key_str = nm_sym[int(strip_underscore(key),16)] + ' (' + key + ')'
        elif mmap[key].has_key('lock_name') :
            key_str = mmap[key]['lock_name'] + ' (' + key + ')'
        else :
            key_str = key + ' ()'

        if(mmap[key].has_key('rsize')):
            key_str += '(%d/%d/%d)' % (mmap[key]['rsize'],
                                       mmap[key]['wsize'],
                                       mmap[key]['size'])


        # % of this kind of sync, then % of total cyc
        # then avg(sd) for total/short/long instances
        # then percent conflicting datasets, average
        # conflict size for conflicting sets, average
        # conflict percent for conflicting sets
        cnt_total = float(mmap[key][sort_key][0])
        cnt_short = float(mmap[key][sort_key][3])
        cnt_long = float(mmap[key][sort_key][6])

        # Weighted total of data dependent things we are calculating
        #total_dependent += (mmap[key][sort_key][1] * mmap[key]['hotos_data_dependence'])

        print '%4.2f%%(%4.2f%%) %s(%s)/%s(%s)/%s(%s) %.2f%% %.2f %.2f%%: %s' %(
            ( 100.0 * mmap[key][sort_key][1] ) / key_sum,
            ( 100.0 * mmap[key][sort_key][1] ) / cycles,

            commify(str(
                int(compute_average(mmap[key][sort_key][1], cnt_total)) )),
            commify(str(
               int(math.sqrt(math.fabs(
               compute_average(mmap[key][sort_key][2], cnt_total) - 
               compute_average(mmap[key][sort_key][1]**2, cnt_total) ))) )),

            commify(str(
               int(compute_average(mmap[key][sort_key][4], cnt_short)) )),
            commify(str(
               int(math.sqrt(math.fabs(
               compute_average(mmap[key][sort_key][5], cnt_short) - 
               compute_average(mmap[key][sort_key][4]**2, cnt_short) ))) )),

            commify(str(
               int(compute_average(mmap[key][sort_key][7], cnt_long)) )),
            commify(str(
               int(math.sqrt(math.fabs(
               compute_average(mmap[key][sort_key][8], cnt_long) - 
               compute_average(mmap[key][sort_key][7]**2, cnt_long) ))) )),

            100*mmap[key]['hotos_data_dependence'],
            mmap[key]['hotos_dependent_bytes'],
            100*mmap[key]['hotos_dependent_bytes_pct'],

            key_str)

        if len(mmap[key]['average_conflicts']) :
            po_str = '  Projected optimism of %d max (ok/conf): ' % mmap[key]['max_threads']
            for i in xrange(len(mmap[key]['average_conflicts'])) :
                po_str += '(%d: %.2f/%.2f, %.2f, %.2f), ' % (NUM_CPUS_ARRAY[i], \
                                                             mmap[key]['average_completions'][i], \
                                                             mmap[key]['average_conflicts'][i], \
                                                             mmap[key]['average_threads'][i], \
                                                             mmap[key]['conflict_density'][i])
            print po_str

    print 'Average data independence: 16: %4.2f, 32: %4.2f' % ( total_independent16, total_independent32)
    print 'Average conflict density: 16: %4.2f, 32: %4.2f' % ( total_density16, total_density32)

    
# Sort the map in various ways and print summary
def print_map_scalar(mmap, keys, sort_key, ksym, num_print, title, print_desc) :
    keys.sort(lambda a,b : -cmp(mmap[a][sort_key],
                                mmap[b][sort_key]))
    num_print = min(len(keys), num_print)
    top_keys = keys[:num_print]
    key_sum = sum([mmap[key][sort_key] for key in keys])
    top_key_sum = sum([mmap[key][sort_key] for key in top_keys])
    if top_key_sum == 0 : return
    # Calculate the average data independence and conflict density for
    # the entire kernel
    total_independent16 = 0.0
    total_independent32 = 0.0
    total_density16 = 0.0
    total_density32 = 0.0
    weight = 0.0
    for key in keys :
        weight = float(mmap[key][sort_key]) / float(key_sum)
        total_independent16 +=  mmap[key]['average_completions'][NUM_CPUS[16]] * weight
        total_independent32 += mmap[key]['average_completions'][NUM_CPUS[32]] * weight
        total_density16 += mmap[key]['conflict_density'][NUM_CPUS[16]] * weight
        total_density32 += mmap[key]['conflict_density'][NUM_CPUS[32]] * weight
    print title
    print 'Top %d of %d %s %.1f%% of total count' %\
          (num_print, len(keys), print_desc,
           100.0 * top_key_sum / key_sum)
    for key in top_keys :
        if ksym.has_key(int(strip_underscore(key), 16)) :
            key_str = ksym[int(strip_underscore(key),16)]['real'] + "::" + ksym[int(strip_underscore(key),16)]['cfl']
        elif nm_sym.has_key(int(strip_underscore(key), 16)) :
            key_str = nm_sym[int(strip_underscore(key),16)] + ' (' + key + ')'
        elif mmap[key].has_key('lock_name') :
            key_str = mmap[key]['lock_name'] + ' (' + key + ')'
        else :
            key_str = key + ' ()'

        if(mmap[key].has_key('rsize')):
            key_str += '(%d/%d/%d)' % (mmap[key]['rsize'],
                                       mmap[key]['wsize'],
                                       mmap[key]['size'])

        if(mmap[key].has_key('nest_depth')):
            if(mmap[key]['nest_depth'][0] != '0'):
                nest = (float(mmap[key]['nest_depth'][1]) / float(mmap[key]['nest_depth'][0]))
                key_str += '(%.2f)' % nest
            else :
                key_str += '(%.2f)' % 0

        # Weighted total of data dependent things we are calculating
        #total_dependent += (mmap[key][sort_key] * mmap[key]['hotos_data_dependence'])

        print '%8s(%4.1f%%) %.2f%% %.2f %.2f%%: %s' %(
            commify(str(mmap[key][sort_key])),
            100.0 * mmap[key][sort_key] / key_sum,


            100*mmap[key]['hotos_data_dependence'],
            mmap[key]['hotos_dependent_bytes'],
            100*mmap[key]['hotos_dependent_bytes_pct'],

            key_str)

        if len(mmap[key]['average_conflicts']) :
            po_str = '  Projected optimism of %d max (ok/conf): ' % mmap[key]['max_threads']
            for i in xrange(len(mmap[key]['average_conflicts'])) :
                po_str += '(%d: %.2f/%.2f, %.2f, %.2f), ' % (NUM_CPUS_ARRAY[i], \
                                                             mmap[key]['average_completions'][i], \
                                                             mmap[key]['average_conflicts'][i], \
                                                             mmap[key]['average_threads'][i], \
                                                             mmap[key]['conflict_density'][i])
            print po_str

    print 'Average data independence: 16: %4.2f, 32: %4.2f' % ( total_independent16, total_independent32)
    print 'Average conflict density: 16: %4.2f, 32: %4.2f' % ( total_density16, total_density32)


def print_map_qcount(mmap, keys, sort_key, ksym, num_print, title, print_desc) :
    keys.sort(lambda a,b : -cmp(mmap[a][sort_key],
                                mmap[b][sort_key]))
    num_print = min(len(keys), num_print)
    top_keys = keys[:num_print]
    key_sum = sum([mmap[key][sort_key] for key in keys])
    top_key_sum = sum([mmap[key][sort_key] for key in top_keys])
    if top_key_sum == 0 : return
    print title
    print 'Top %d of %d %s %.1f%% of total count' %\
          (num_print, len(keys), print_desc,
           100.0 * top_key_sum / key_sum)
    print 'lock | total acq | contended acq (%%) | % data dependent | mean dependent address set bytes| mean overlapping bytes'
    for key in top_keys :
        if ksym.has_key(int(strip_underscore(key), 16)) :
            key_str = ksym[int(strip_underscore(key),16)]['real'] + "::" + ksym[int(strip_underscore(key),16)]['cfl']
        elif nm_sym.has_key(int(strip_underscore(key), 16)) :
            key_str = nm_sym[int(strip_underscore(key),16)] + ' (' + key + ')'
        elif mmap[key].has_key('lock_name') :
            key_str = mmap[key]['lock_name'] + ' (' + key + ')'
        else :
            key_str = key + ' ()'

        pct_contended = 0
        if(mmap[key]['count'] != 0) :
            pct_contended = 100.0 * mmap[key]['q_count'] / mmap[key]['count']

        mean_bytes = 0
        mean_overlapping_bytes = 0
        pct_dependent = 0

        # DEP 3/30/07 - Fix up q_count to equal dep + indep.  It is
        # often off by 1, most likely because of clearing stats.
        # Also, for radix, one of the runqueue locks if crazy off.  I
        # don't want to waste a lot of time on that, though, since
        # runqueue locks are weird anyway.
        #
        # The primary purpose for this is to avoid introducing a bias
        # one way or the other

        mmap[key]['q_count'] = mmap[key]['q_count_dependent'] + \
                               mmap[key]['q_count_independent'] 

        if(mmap[key]['q_count'] != 0) :
            pct_dependent = 100.0 * mmap[key]['q_count_dependent'] / mmap[key]['q_count']

        if(mmap[key]['q_count_dependent'] != 0) :
            mean_bytes = mmap[key]['q_count_dependent_total_bytes'] / mmap[key]['q_count_dependent']
            mean_overlapping_bytes = mmap[key]['q_count_dependent_conflicting_bytes'] / mmap[key]['q_count_dependent']

        print '%s %8s(%4.1f%%) | %s |  %s (%.2f%%) | %.2f%% | %.0f | %.0f' %(
            key_str,
            commify(str(mmap[key][sort_key])),
            100.0 * mmap[key][sort_key] / key_sum,

            commify(str(mmap[key]['count'])),
            commify(str(mmap[key]['q_count'])),
            pct_contended,
            pct_dependent,
            mean_bytes,
            mean_overlapping_bytes)

def print_conflict_histogram (histogram) :

    splits = 5; # number of cols
    categories = [0]*(splits+1)

    if not histogram.has_key('total') :
        print 'No conflicts'
        return

    total = histogram['total']

    sanity_check_total = 0

    for addr in histogram.keys() :
        #print '%s - %d' % (addr, histogram[addr])
        if addr == 'total' : continue
        sanity_check_total += histogram[addr]
        if addr == 'io' : continue
        idx = int( (float(histogram[addr]) / float(total)) * splits)
        if(idx > 3): print 'Hot address:%s' % addr
        categories[idx] += 1

    assert (sanity_check_total >= total)

    print 'Total conflicts: %d' % histogram['total']


    for i in xrange(splits+1) :
        print '%d : %d' % ( ((100 / splits) * i), categories[i])


    if histogram.has_key('io') :
        '%d IO conflicts' % histogram['io']
        
    

def print_important_locks(mmap, cycles, idle_cycles, cpu_count, freq_mhz) :
    print 'Begin Important Locks'
    for key in mmap.keys() :
        if mmap[key]['lock_name'] in important_locks :

            # Compute the projected speedup/execution time at current cpu count
            lock_cycles = mmap[key]['acq_total'][1] + mmap[key]['hold_total'][1]
            cycle_frac = float(lock_cycles / cycles)

            data_independence = mmap[key]['average_completions'][NUM_CPUS[cpu_count]]
            conflicts = mmap[key]['average_conflicts'][NUM_CPUS[cpu_count]]
            conflict_density = mmap[key]['conflict_density'][NUM_CPUS[cpu_count]]
            threads = mmap[key]['average_threads'][NUM_CPUS[cpu_count]]
            
            if conflict_density == 0 :
                conflict_density = 1

            speedup = threads / conflict_density

            other_cycles = cycles - lock_cycles
            
            # Adjust for gross load imbalance in locking
            other_cycles = other_cycles - idle_cycles 
            # There is some imprecision in idle accounting that can get this value slightly below zero
            if other_cycles < 0 :
                other_cycles = 0

            projected_seconds = 0.00
            
            if speedup != 0 :
                projected_lock_cycles = lock_cycles/speedup

                projected_cycles = projected_lock_cycles + other_cycles
                
                projected_seconds = float( projected_cycles / (freq_mhz * 1000000 * cpu_count) )

            
            # Print the cycles (total/acq/held), data independence and conflict density
            print '%s(%s) Cycles t[%d] a[%d] h[%d], DI/C/CD/T %d[%.2f/%.2f/%.2f/%.2f] Speedup: %.2f, Projected %.2f s, asym rate %.2f' % ( \
                mmap[key]['lock_name'], key, \
                lock_cycles, \
                mmap[key]['acq_total'][1], mmap[key]['hold_total'][1], \
                cpu_count, \
                data_independence, \
                conflicts, \
                conflict_density, \
                mmap[key]['average_threads'][NUM_CPUS[cpu_count]], \
                speedup, projected_seconds, mmap[key]['asymmetry_rate'])

            print 'Average workset size (words): %.2f' % mmap[key]['avg_workset_size_words']

            print_conflict_histogram(mmap[key]['histogram'])

    print 'End Important Locks'

def print_map(mmap, keys, sort_key, ksym, num_print, title, print_desc, cycles, nm_sym, cpu_count, freq_mhz, print_fn = None) :
    if print_fn is not None:
        print_fn(mmap, keys, sort_key, ksym, num_print, title, print_desc,
            cycles, nm_sym)
        return
    try :
        if isinstance(mmap[keys[0]][sort_key], list) :
            print_map_av(mmap, keys, sort_key, ksym, num_print,
                         title, print_desc, cycles, nm_sym, cpu_count, freq_mhz)
        else :
            print_map_scalar(mmap, keys, sort_key, ksym, num_print, title,
                             print_desc)
    except IndexError : pass # No data to print

def print_all(title, lockmap, callmap, lockid_set, ksym, num_print, cycles, nm_sym, cpu_count, freq_mhz) :
    lock_keys = [key for key in lockmap.keys() \
                 if lockmap[key]['lock_id'] <= lockid_set]
    call_keys = [key for key in callmap.keys() \
                 if callmap[key]['lock_id'] <= lockid_set]
    print 'Dynamic count of %s: %s' % (title, commify(str(len(lock_keys))))
    print_map(lockmap, lock_keys, 'count', ksym, opt_sync_funcs,
              title, 'most called locks', cycles, nm_sym, cpu_count, freq_mhz)
    print_map(callmap, call_keys, 'count', ksym, opt_sync_funcs,
              title, 'most called lock routines', cycles, nm_sym, cpu_count, freq_mhz)
    print_map(lockmap, lock_keys, 'acq_total', ksym, opt_sync_funcs,
              title, 'longest to acquire locks', cycles, nm_sym, cpu_count, freq_mhz)
    print_map(callmap, call_keys, 'acq_total', ksym, opt_sync_funcs,
              title, 'call sites for longest to acquire', cycles, nm_sym, cpu_count, freq_mhz)
    print_map(lockmap, lock_keys, 'hold_total', ksym, opt_sync_funcs,
              title, 'longest held locks', cycles, nm_sym, cpu_count, freq_mhz)
    print_map(callmap, call_keys, 'hold_total', ksym, opt_sync_funcs,
              title, 'call sites for longest held', cycles, nm_sym, cpu_count, freq_mhz)
    # Broken - don't need it at the moment
    #print_map(lockmap, lock_keys, 'hotos_dependent_bytes', ksym, opt_sync_funcs,
    #          title, 'locks with largest conflicting working sets (in bytes)',
    #          cycles, nm_sym,
    #          print_workset_av)

    #Rokken contention data
    print_map(lockmap, lock_keys, 'q_count', ksym, opt_contended,
              title, 'most contended locks', cycles, nm_sym, cpu_count, freq_mhz)
    print_map(callmap, call_keys, 'q_count', ksym, opt_contended,
              title, 'most contended lock routines', cycles, nm_sym, cpu_count, freq_mhz)

    print_map_qcount(lockmap, lock_keys, 'q_count', ksym, opt_contended,
              title, 'most contended locks (2)')

    print_map(lockmap, lock_keys, 'q_count_independent', ksym, opt_contended,
              title, 'most data independent contended locks', cycles, nm_sym, cpu_count, freq_mhz)
    print_map(lockmap, lock_keys, 'q_count_dependent', ksym, opt_contended,
              title, 'most data dependent contended locks', cycles, nm_sym, cpu_count, freq_mhz)
    
    
    # Probably less useful
    print_map(lockmap, lock_keys, 'useless_release', ksym, opt_sync_funcs,
              title, 'most uselessly released locks', cycles, nm_sym, cpu_count, freq_mhz)
    print_map(callmap, call_keys, 'useless_release', ksym, opt_sync_funcs,
              title, 'most call sites for useless releases', cycles, nm_sym, cpu_count, freq_mhz)
    
def process_exp(lockmap, callmap, ksym, cycles, idle_cycles, nm_sym, cpu_count, freq_mhz) :
    #############################################
    ## Print summary info about inlined/not inlined
    #######################################################
    # Actually print the different outputs

    if opt_sync_categories > 0 :
        # Print a quick summary of data we really care about for
        # paper-writing purposes
        print_important_locks(lockmap, cycles, idle_cycles, cpu_count, freq_mhz)
        
        # Categories are semaphores, spinlocks, mutex, rcu, futex
        print_all('Spin locks', lockmap, callmap, spin_lock_set, ksym,
                  opt_sync_categories, cycles, nm_sym, cpu_count, freq_mhz)
        print_all('RW spin locks', lockmap, callmap, rwspin_lock_set, ksym,
                  opt_sync_categories, cycles, nm_sym, cpu_count, freq_mhz)
        print_all('Semaphores', lockmap, callmap, sema_set, ksym,
                  opt_sync_categories, cycles, nm_sym, cpu_count, freq_mhz)
        print_all('RW semaphores', lockmap, callmap, rwsema_set, ksym,
                  opt_sync_categories, cycles, nm_sym, cpu_count, freq_mhz)
        print_all('RCU', lockmap, callmap, rcu_set, ksym,
                  opt_sync_categories, cycles, nm_sym, cpu_count, freq_mhz)
        print_all('Mutex', lockmap, callmap, mutex_set, ksym,
                  opt_sync_categories, cycles, nm_sym, cpu_count, freq_mhz)
        print_all('Completions', lockmap, callmap, completion_set, ksym,
                  opt_sync_categories, cycles, nm_sym, cpu_count, freq_mhz)
        print_all('Futex', lockmap, callmap, futex_set, ksym,
                  opt_sync_categories, cycles, nm_sym, cpu_count, freq_mhz)

def add_av(i, m, lockmap, lock_addr, field) :
    lockmap[lock_addr][field][0] += long(m.group(i + 0))
    lockmap[lock_addr][field][1] += long(m.group(i + 1))
    lockmap[lock_addr][field][2] += long(m.group(i + 2))
    lockmap[lock_addr][field][3] += long(m.group(i + 3))
    lockmap[lock_addr][field][4] += long(m.group(i + 4))
    lockmap[lock_addr][field][5] += long(m.group(i + 5))
    lockmap[lock_addr][field][6] += long(m.group(i + 6))
    lockmap[lock_addr][field][7] += long(m.group(i + 7))
    lockmap[lock_addr][field][8] += long(m.group(i + 8))

def usage() :
    print sys.argv[0] + \
''': [-l location of sync_count.log file (sync_count.log)]
                   [-x location of vmlinux file (vmlinux)]
                   [-c X show top X contended sync function users (15)]
                   [-t X show top X sync LOCK functions for each type (10)]
                   [-s X show top X sync functions (12)]
                   [-u X show top X sync function users (0)]
                   [-v verbose output (false)]'''
###################################################################
## Main program starts here
## Get options
try :
    opts, args = getopt.getopt(sys.argv[1:],'c:l:u:s:ht:vx:p:qamr:z:w:n:')
except getopt.GetoptError :
    usage()
    sys.exit(2)
opt_log = 'sync_char.log'
opt_sync_users = 0
opt_sync_funcs = 15
opt_contended = 20
opt_verbose = False
opt_sync_categories = 10
opt_vmlinux = 'vmlinux'
opt_pickle = False
opt_pickle_only = False
opt_threshold = 0
opt_arff_output = False
opt_user_app = False
opt_seed = 1
for o, a in opts :
    if o == '-v' :
        opt_verbose = True
    if o == '-h' :
        usage()
        sys.exit(2)
    if o == '-s' :
        opt_sync_funcs = int(a)
    if o == '-t' :
        opt_sync_categories = int(a)
    if o == '-c' :
        opt_contended = int(a)
    if o == '-u' :
        opt_sync_users = int(a)
    if o == '-l' :
        opt_log = a
    if o == '-x' :
        opt_vmlinux = a
    if o == '-p' :
        opt_pickle = a
    if o == '-q' :
        opt_pickle_only = True
    if o == '-a' :
        opt_arff_output = True
    if o == '-m' :
        opt_user_app = True
    if o == '-r' :
        opt_seed = int(a)
    if o == '-z' :
        DI_SAMPLE_SIZE = int(a)
    if o == '-w' :
        DI_WINDOW_SIZE = int(a)
    if o == '-n' :
        DI_SAMPLE_RATE = int(a)

random.seed(opt_seed);

if not os.access(opt_vmlinux, os.R_OK) :
    raise AssertionError, 'Cannot read vmlinux kernel image file %s' % opt_vmlinux
if not opt_pickle_only:
    if not os.access(opt_log, os.R_OK) :
        raise AssertionError, 'Cannot read %s' % opt_log

# Assume a default pickle file of the kernel.pkl
if not opt_pickle :
    opt_pickle = '%s.pkl' % opt_vmlinux

# Globals
lockmap = {} # Lock address string to record about lock
callmap = {} # PC value (RA from call) or inlined PC to info about caller
ksym = {} # dict from pc value to function:filename:lineno
nm_sym = {} # dict from addr to symbol name
worksets = {} # dict of lockaddr->generation->index->WorkSet
histograms = {} # dict of lockaddr->conflict count

asym_detector1 = WorkSet(0, [], 0, 0, 0, {}, 0)
asym_detector2 = WorkSet(0, [], 0, 0, 0, {}, 0)

# Read-ahead storage for contended workset data dependence info
q_count = {} # dict of lockaddr->generation->info

[ksym, nm_sym] = init_syms(ksym, nm_sym, objdump, nm, opt_vmlinux, opt_pickle, opt_pickle_only)

cycles = 0
cycles_re = re.compile(r'^Cycles \d+: (?P<cyc>\d+)')

idle_cycles = 0
idle_re = re.compile(r'^KSTAT_SNAPSHOT.*cycle\s+\d+\s+\d+\s+\d+\s+\d+\s+\d+\s+\d+\s+(?P<idle>\d+)')

# Workset dumps
workset_re = re.compile(r'''
  ^WS_CLOSE\s+                       # Start with the  WS_CLOSE tag
  (?P<lock_addr>(0x)?[a-fA-f0-9]+)\s+   # The lock address
  (?P<lock_generation>\d+)\s+        # The generation number
  (?P<index>\d+)\s+                  # The index of the workset for the lock
  \((?P<pids>[0-9\s]+)\)\s+          # The pids that held the lock are in parens
                                     # Separated by spaces
                                     # there can be 2 for runqueue lock
  (?P<cpu>\d+)\s+                    # the cpu that held this lock
  C\[\s+(?P<contending_ws>[0-9\s]*)\s*\]\s*                                     
                                     # The tuples identifying the contended worksets
  (?P<IO>I?O?)                      # Does IO occur in this lock instance?
''', re.VERBOSE)


address_set_re = re.compile(r'^\s*\((?P<addr>0x[a-fA-F0-9]+)\,\s+(?P<chunks>[\sa-fA-F0-9]+)\s*')

# Detect when we should reset stats
reset_stats_re = re.compile(r'''^RESET_STATS''')

# Old versions of the regexes
#inst_line_re = re.compile(r'^(?P<lock_addr>0x[a-fA-F0-9_]+)\((?P<lock_name>.*)\)\s+(?P<lock_id>\d+)\s+(?P<acq_pid>\d+)\s+(?P<rsize>\d+)\s+(?P<wsize>\d+)\s+(?P<size>\d+)\s+(?P<workset>([\d\.]+\s+){27})')
#caller_re = re.compile(r'^\s*(?P<caller_ra>0?x?[a-fA-F0-9]+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)')

inst_line_re = re.compile(r'''
   ^(?P<lock_addr>0x[a-fA-F0-9_]+)   # At the start of the line is the addr in hex
   \((?P<lock_name>.*)\)\s+          # Followed by the name of the lock in parens
   (?P<lock_generation>\d+)\s+       # Lock generation number
   (?P<workset_count>\d+)\s+         # Number of worksets this lock created
   (?P<lock_id>\d+)\s+               # Type of lock we are dealing with
   (?P<acq_pid>\d+)\s+               # Not sure what this is - number of pids to acq?
   (?P<rsize>\d+)\s+                 # Aggregate size of workets, split by r/w/total
   (?P<wsize>\d+)\s+
   (?P<size>\d+)\s+
   (?P<nest_depth>([\d\.]+\s+){9})   # Average variable for lock nesting depth
                                     # splits at 1
''', re.VERBOSE)

caller_re = re.compile(r'''
                                      # Caller entry re
   ^\s*                               # Begin with an arbitrary amount of space
   (?P<caller_ra>0?x?[a-fA-F0-9]+)\s+ # Followed by the return addr in hex

   (\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+   # And 22 ints with various and sundry
   (\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+   # meanings
   (\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+
   (\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+
   (\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+
   (\d+)\s+(\d+)                      

''', re.VERBOSE)


# Regex to get the lock addr from the lockaddr_version combo
lock_addr_re = re.compile(r'''
   ^(?P<lock_addr>0?x?[a-fA-F0-9]+)  # Ignore subsequent _...  
                                     # Should greedily suck in as many chars as possible
''', re.VERBOSE)

# Regex to get the cpu/freq info
cpu_info_re = re.compile(r'''
   ^(?P<cpu_count>\d+)\s+            # The number of cpus
   (?P<freq_mhz>\d+)MHz\s+processors # and the clock frequency

''', re.VERBOSE)

# End of benchmarks re
eob_re = re.compile(r'''
   ^SYNCCHAR:\s+End\s+of\s+Stats    # Special string for end-of-benchmarks
''', re.VERBOSE)

# Related globals
cpu_count = 0
freq_mhz = 0


# Dump the arff header if we are giving arff output
if opt_arff_output:

    print '@RELATION syncchar'
    print
    print '@ATTRIBUTE lock_addr STRING'
    print '@ATTRIBUTE lock_generation NUMERIC'
    print '@ATTRIBUTE workset_index NUMERIC'
    print '@ATTRIBUTE cpu NUMERIC'
    print '@ATTRIBUTE pid NUMERIC'
    print '@ATTRIBUTE io NUMERIC'
    # We need a way to list the worksets themselves and the critical
    # sections we waited on

    print
    print '@DATA'

    

f = open(opt_log)

#for line in open(opt_log) :
while 1 :

    # Not being able to make assignments in a conditional really sucks
    line = f.readline()
    if not line: break

    m = eob_re.match(line)

    # Blank lines delimit different experiments in the old way - keep
    # for backwards compat, but be smarter about it (i.e. only cycles != 0).  also use the new
    # string
    if m or (len(line) == 1 and cycles):
        #pprint.pprint(lockmap)
        #print "CALLMAP"
        #pprint.pprint(callmap)
        #sys.exit(0)
        # These are the total cycles (across all cpus)
        if(not opt_arff_output) :
            print "Cycles ", cycles
            process_exp(lockmap, callmap, ksym, cycles, (idle_cycles * 1000000), nm_sym, cpu_count, freq_mhz)
            print # Separator line
            
        lockmap = {} # Instrumentation point to dict
        callmap = {}
        worksets = {}
        q_count = {}
        cycles = 0
        idle_cycles = 0
        histograms = {}
        continue
    
    # Do we match the end-of-benchmark/life data for a lock?
    m = inst_line_re.match(line)
    if m :
        lock_addr = m.group('lock_addr')
        lock_name = m.group('lock_name')
        lock_id   = long(m.group('lock_id'))
        acq_pid   = long(m.group('acq_pid'))
        rsize     = int(m.group('rsize'))
        wsize     = int(m.group('wsize'))
        size      = int(m.group('size'))
        lock_generation = int(m.group('lock_generation'))
        nest_depth = m.group('nest_depth').split()
        
        # Filter out fine-grained locks
        if(wsize < opt_threshold):
            continue

        # Filter out kernel locks if we are interested in a user-mode
        # app (dynamic locks still end up in the logfile and bad
        # things follow
        if(opt_user_app) :
            m1 = lock_addr_re.match(lock_addr)
            assert m1
            my_lock_addr = m1.group('lock_addr')
            if(int(my_lock_addr, 16) >= 0xc0000000) :
                continue

        # The data independence metric as of the HotOS paper, for
        # posterity
        hotos_data_dependence_values = [0.0, 0.0, 0.0]
        new_data_independence = [0,
                                 [0.0, 0.0, 0.0],
                                 [0.0, 0.0, 0.0],
                                 [0.0, 0.0, 0.0],
                                 [0.0, 0.0, 0.0]]
        avg_workset_size_words = 0
        asymmetry_rate = 0.0
        if  worksets.has_key(lock_addr) and worksets[lock_addr].has_key(lock_generation):

            if worksets[lock_addr][lock_generation]['worksets_sampled'] > 0 :
                avg_workset_size_words = float(worksets[lock_addr][lock_generation]['workset_size']) / worksets[lock_addr][lock_generation]['worksets_sampled']
                asymmetry_rate = float(worksets[lock_addr][lock_generation]['asym_detected']) / worksets[lock_addr][lock_generation]['worksets_sampled']


            if USE_HOTOS_DI :
                hotos_data_dependence_values = calculate_hotos_data_dependence(worksets[lock_addr][lock_generation]['worksets'], HOTOS_DI_WINDOW_SIZE)

            #if lock_addr == '0xc03e16e0' :
            # Krw or Kprob
            #if lock_addr == '0xc03e1b60' or lock_addr == '0xc03e1fe0' or lock_addr == '0xc03e16e0':
            if 1 :
                #print 'calculating data dependence for lock %s' % lock_addr
                calculate_new_data_independence(worksets[lock_addr][lock_generation], histograms[lock_addr][lock_generation])
                for i in xrange(len(NUM_CPUS)) :
                    if worksets[lock_addr][lock_generation]['samples'][i] > 0 :
                        worksets[lock_addr][lock_generation]['avg_conflicts'][i] /= float(worksets[lock_addr][lock_generation]['samples'][i])
                        worksets[lock_addr][lock_generation]['avg_completions'][i] /= float(worksets[lock_addr][lock_generation]['samples'][i])
                        worksets[lock_addr][lock_generation]['sample_threads'][i] /= float(worksets[lock_addr][lock_generation]['samples'][i])
                        worksets[lock_addr][lock_generation]['conflict_density'][i] /= float(worksets[lock_addr][lock_generation]['samples'][i])
                    
                new_data_independence =  [len(worksets[lock_addr][lock_generation]['max_threads']),
                                          worksets[lock_addr][lock_generation]['avg_conflicts'],
                                          worksets[lock_addr][lock_generation]['avg_completions'],
                                          worksets[lock_addr][lock_generation]['sample_threads'],
                                          worksets[lock_addr][lock_generation]['conflict_density']]

            # Go ahead and free up the worksets for this lock
            # after we are done with the data independence
            # calculations
            worksets[lock_addr].pop(lock_generation)

        assert not lockmap.has_key(lock_addr), lock_addr

        histogram = {}
        if histograms.has_key(lock_addr) and histograms[lock_addr].has_key(lock_generation):
            histogram = histograms[lock_addr][lock_generation]

        lockmap[lock_addr] = {
            'lock_id' : set([lock_id]),
            'acq_pid' : acq_pid,
            'count'   : 0,
            'q_count' : 0,
            'q_count_dependent' : 0,
            'q_count_independent' : 0,
            'q_count_dependent_total_bytes' : 0,
            'q_count_dependent_conflicting_bytes' : 0,
            'useless_release' : 0,
            'acq_total'  : [0, 0, 0, 0, 0, 0, 0, 0, 0],
            'hold_total' : [0, 0, 0, 0, 0, 0, 0, 0, 0],
            'hotos_data_dependence' : hotos_data_dependence_values[0],
            'hotos_dependent_bytes' : hotos_data_dependence_values[1],
            'hotos_dependent_bytes_pct' : hotos_data_dependence_values[2],
            # Max threads to use this lock
            'max_threads' : new_data_independence[0],
            'average_conflicts' : new_data_independence[1],
            'average_completions' : new_data_independence[2],
            # Average number of pids sampled
            'average_threads' : new_data_independence[3],
            'conflict_density' : new_data_independence[4],
            'asymmetry_rate' : asymmetry_rate,
            'lock_name' : lock_name,
            'rsize' : rsize,
            'wsize' : wsize,
            'size'  : size,
            'histogram' : histogram,
            'avg_workset_size_words' : avg_workset_size_words,
            # Nesting depth
            'nest_depth' : nest_depth,
            }

        if q_count.has_key(lock_addr) and q_count[lock_addr].has_key(lock_generation) :
            lockmap[lock_addr]['q_count_dependent'] = q_count[lock_addr][lock_generation]['q_count_dependent']
            lockmap[lock_addr]['q_count_independent'] =  q_count[lock_addr][lock_generation]['q_count_independent']
            lockmap[lock_addr]['q_count_dependent_total_bytes'] = q_count[lock_addr][lock_generation]['q_count_dependent_total_bytes']
            lockmap[lock_addr]['q_count_dependent_conflicting_bytes'] = q_count[lock_addr][lock_generation]['q_count_dependent_conflicting_bytes']
                

        # First record is lock_addr & lock_id

        for caller in line.split('[')[1:] :
            ma = caller_re.match(caller)
            if ma :
                caller_ra = ma.group(1)
                lockmap[lock_addr]['count'] += int(ma.group(3))
                lockmap[lock_addr]['q_count'] += int(ma.group(4))
                lockmap[lock_addr]['useless_release'] += int(ma.group(5))
                add_av( 6, ma, lockmap, lock_addr, 'acq_total')
                add_av(15, ma, lockmap, lock_addr, 'hold_total')
                
                if not callmap.has_key(caller_ra) :
                    callmap[caller_ra] = {
                        'acq_pid' : 1,
                        'count'   : 0,
                        'q_count' : 0,
                        # The q_count_* values are no longer tracked per-caller
                        'q_count_dependent' : 0,
                        'q_count_independent' : 0,
                        'q_count_dependent_total_bytes' : 0,
                        'q_count_dependent_conflicting_bytes' : 0,
                        'useless_release' : 0,
                        'lock_id' : set(),
                        'acq_total'  : [0, 0, 0, 0, 0, 0, 0, 0, 0],
                        'hold_total' : [0, 0, 0, 0, 0, 0, 0, 0, 0],
                        # We shouldn't use these values for anything
                        'hotos_data_dependence' : 0,
                        'hotos_dependent_bytes' : 0,
                        'hotos_dependent_bytes_pct' : 0,
                        'max_threads' : 0,
                        'average_conflicts' : [0.0]*len(NUM_CPUS),
                        'average_completions' : [0.0]*len(NUM_CPUS),
                        'average_threads' : [0.0]*len(NUM_CPUS),
                        'conflict_density' : [0.0]*len(NUM_CPUS),
                        }
                callmap[caller_ra]['flags'] = int(ma.group(2))
                callmap[caller_ra]['count'] += int(ma.group(3))
                callmap[caller_ra]['q_count'] += int(ma.group(4))
                callmap[caller_ra]['useless_release'] += int(ma.group(5))
                callmap[caller_ra]['lock_id'].add(lock_id)
                add_av( 6, ma, callmap, caller_ra, 'acq_total')
                add_av(15, ma, callmap, caller_ra, 'hold_total')
            else :
                assert False, caller
        continue

    # Do we match the workset for a given lock acq/release
    m = workset_re.match(line)
    if m :
        # Finish up the parsing
        lock_addr = m.group('lock_addr')
        
        generation = int(m.group('lock_generation'))
        index = int(m.group('index'))
        pids = m.group('pids').strip().split(' ')
        cpu = m.group('cpu')
        io = m.group('IO')
        if io == 'IO' :
            io = 1
        else :
            io = 0
        contending_ws = m.group('contending_ws').strip().split(' ')
        address_set = {}

        # Address sets are packed in a binary format
        # extract them here
        tmp_array = array.array('I')
        try :
            tmp_array.fromfile(f, 1)
        except EOFError :
            sys.exit()
        num_entries = tmp_array.pop(0)

        for i in xrange(num_entries) :
            # Warning: this width can change.  For now the
            # chunks are 8 bytes.  Should put this in a header
            # at some point
            try :
                tmp_array.fromfile(f, 3)
            except EOFError:
                sys.exit()
            addr = '0x%x' % (tmp_array.pop(0))
            chunks = []
            # Two elements in a chunk
            chunks.append(tmp_array.pop(0))
            chunks.append(tmp_array.pop(0))
            
            address_set[addr] = chunks


        # Clean up the extra newline
        line = f.readline()
        assert(len(line) == 1), line
                
        # Just short-circuit for now
        #if lock_addr != '0xc03e16e0' :
        #kprob or krw
        #if lock_addr != '0xc03e1b60' and lock_addr != '0xc03e1fe0' and lock_addr != '0xc03e16e0':
        
        # Skip kernel locks when we are in a user app
        if opt_user_app and int(lock_addr, 16) >= 0xc000000 :
            continue

        workset = WorkSet(lock_addr, pids, generation, index, cpu, address_set, io)

        # if this is the asymmetric conflict detector, just update and move on
        if(lock_addr == '0xc0000000' or lock_addr == '0') : 
            #print 'Asym detector size = %d' % workset.size()
            asym_detector2 = asym_detector1
            asym_detector1 = workset
            continue

        if not histograms.has_key(lock_addr) :
            histograms[lock_addr] = {}
        if not histograms[lock_addr].has_key(generation) :
            histograms[lock_addr][generation] = {}

        # Just print an arff-formatted line if we are doing
        # traces for Jason
        if opt_arff_output:
            workset.printArffEntry()
                    
        else :
            # Calculate data dependence on contended worksets
            # i.e. q_count stuff
            # first, make sure q_count entries exist
            if not q_count.has_key(lock_addr) :
                q_count[lock_addr] = {}
            if not q_count[lock_addr].has_key(generation) :
                q_count[lock_addr][generation] = {
                    'q_count_dependent' : 0,
                    'q_count_independent' : 0,
                    'q_count_dependent_total_bytes' : 0,
                    'q_count_dependent_conflicting_bytes' : 0,
                    }
            # Don't care about this at the moment
            #for idx in contending_ws :
            #    if len(idx) == 0 : continue
            #    i = int(idx) - worksets[lock_addr][generation]['offset']
                
            # dP: Known problem - we store the contended
            # worksets by caller_ra, which can be the same
            # address as both the contender and contendee.
            # Because we have bigger fish to fry at the moment
            # and don't really care too much about these
            # numbers I am going to punt on this
            #if i == index :
            #    print 'XXX: bad contended workset pair'
            #    continue
            #depends = workset.compare(worksets[lock_addr][generation]['worksets'][i])
                
            #if depends :
            #    q_count[lock_addr][generation]['q_count_dependent'] += 1
            #    q_count[lock_addr][generation]['q_count_dependent_total_bytes'] += workset.size()
            #    q_count[lock_addr][generation]['q_count_dependent_conflicting_bytes'] += depends
            #else :
            #    q_count[lock_addr][generation]['q_count_independent'] += 1
                

            # Make sure all the stuff we want is present
            if not worksets.has_key(lock_addr) :
                worksets[lock_addr] = {}
            if not worksets[lock_addr].has_key(generation) :
                worksets[lock_addr][generation] = { 'worksets' : [],
                                                    'offset' : index,
                                                    'max_threads' : set(),
                                                    'avg_conflicts' : [0.0]*len(NUM_CPUS),
                                                    'avg_completions' : [0.0]*len(NUM_CPUS),
                                                    'samples' : [0]*len(NUM_CPUS),
                                                    'sample_threads' : [0.0]*len(NUM_CPUS),
                                                    'conflict_density' : [0.0]*len(NUM_CPUS),
                                                    'window_start' : 0,
                                                    'current_pids' : [],
                                                    'workset_size' : 0,
                                                    'worksets_sampled' : 0,
                                                    'asym_detected' : 0,
                                                    }
            # Make sure we are inserting this at the correct index.
            # These should appear in the log in increasing numerical order
            # DEP 8/5/07 - we don't need this anymore, now that we
            # have confidence we are getting the traces correctly.
            # Besides, I want to sample differently now
            #assert len(worksets[lock_addr][generation]['worksets']) + \
            #       worksets[lock_addr][generation]['offset'] == index 
            if random.randint(0,100) < DI_SAMPLE_RATE :
                worksets[lock_addr][generation]['worksets'].append(workset)
                # See if this is a new pid
                for pid in workset.pids : # At most 2
                   if pid not in worksets[lock_addr][generation]['current_pids'] :
                        worksets[lock_addr][generation]['current_pids'].append(pid)
                        break
                # Track average workset size
                worksets[lock_addr][generation]['workset_size'] += workset.size_words()
                worksets[lock_addr][generation]['worksets_sampled'] += 1
                if(workset.compare(asym_detector1, [], False) or workset.compare(asym_detector2, [], False)):
                    #print 'Asym detector sizes = %d %d, io  %d %d' % (asym_detector1.size(), asym_detector2.size(), asym_detector1.io, asym_detector2.io)
                    #print "ASYM"
                    #assert 0
                    worksets[lock_addr][generation]['asym_detected'] += 1
                
            #Incremental calculation so that we can free up memory faster
            if len(worksets[lock_addr][generation]['worksets']) >= DI_WINDOW_SIZE \
                   or len(worksets[lock_addr][generation]['current_pids']) == MAX_CPUS :
                calculate_new_data_independence(worksets[lock_addr][generation], histograms[lock_addr][generation])
                # Clear this sample
                worksets[lock_addr][generation]['current_pids'] = []
                worksets[lock_addr][generation]['worksets'] = []
        
        continue

    # Cycle count
    m = cycles_re.match(line)
    if m :
        cycles += int(m.group('cyc'))
        continue

    # Idle cycles
    m = idle_re.match(line)
    if m :
        idle_cycles += int(m.group('idle'))
        continue

    # Check for an explicit reset stats message
    m = reset_stats_re.match(line)
    if m :
        # Dump everything we have collected thus far
        lockmap = {} 
        callmap = {}
        worksets = {}
        q_count = {}
        cycles  = 0
        continue

    # Cpu information
    m = cpu_info_re.match(line)
    if m :
        cpu_count = int(m.group('cpu_count'))
        freq_mhz  = int(m.group('freq_mhz'))
        if not opt_arff_output:
            print line, 
        continue

    if not opt_arff_output:
        print line, 

