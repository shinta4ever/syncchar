# Common functions for sync_char/count.  Code reuse good.
#
# Maintainer: Don Porter <porterde@cs.utexas.edu>
#
###########################################################

import sys, os, re, fsm as fsm_mod, cPickle as pickle
# Need this to compare the modification times
from stat import *

# Use objdump to get symbol information, because it is much better than addr2line
## This class parses the output of objdump using a state machine.
class GET_KSYM:
    '''Scan output of objdump using a  state machine to look for important kernel features'''
    def _init_fsm(self) :
        '''Initialize the finite state machine that scans objdump --disassemble --line-number output.'''
        self.fsm = fsm_mod.FSM()
        self.fsm.start('scan')
        # Keep track of most recent func def and filename:lineno pair for c files
        self.fsm.add('scan',     self.re_func_def, 'scan',  self.set_real_func)
        self.fsm.add('scan',     self.re_cfl,      'scan',  self.set_cfl)
        self.fsm.add('scan',     self.re_func,     'scan',  self.set_inlined_func)
        self.fsm.add('scan',     self.re_addr,     'scan',  self.record_ksym)
        self.fsm.add('scan',     None,             'scan',  None)

    def __init__(self, _ksym, istr) :
        '''Initialize SCAN_OBJ object.'''
        self.real_func  = ''
        # Use this because sometimes the first instruction of a
        # function is from an inlined function.  In this case we want the
        # 'inlined' name to be the real name.  That way the inlined
        # name of 'sync_addr' is always correct
        self.real_addr  = 0
        self.inlined_func  = ''        
        self.cfl        = ''
        # last filename/lineno from a dotc file.  Where call got inlined.
        # self.last_cfl = ''
        self.ksym = _ksym
        self.last_ksym_pc = 0xc0000000
        self.re_func = re.compile(r'^(?P<func_name>[A-Za-z0-9_$#@./]+)\(\):')
        self.re_cfl  = re.compile(r'^(?P<cfile_line>[A-Za-z0-9_$#@./-]+?.c:[0-9]+)')
        self.re_func_def = re.compile(r'^(?P<addr>[A-Fa-f0-9]+) <(?P<func_name>[A-Za-z0-9_$#@./]+)>:')
        self.re_addr = re.compile(r'^(?P<addr>[A-Fa-f0-9]+):\s+')
        self._init_fsm()
        for line in istr :
            try:
                self.fsm.execute(line)
            except fsm_mod.FSMError:
                print 'Invalid input: %s' % line,

    def set_real_func(self, state, input, m) :
        '''Call this when we see source function name in the input'''
        # print 'Set_line_info: %s' % m.group('func_name')
        self.real_func = m.group('func_name')
        self.real_addr = m.group('addr')

    def set_inlined_func(self, state, input, m) :
        '''Call this when we see source function name in the input'''
        # print 'Set_line_info: %s' % m.group('func_name')
        self.inlined_func = m.group('func_name')

    def set_cfl(self, state, input, m) :
        self.cfl = m.group('cfile_line')
        
    def record_ksym(self, state, input, m) :
        # XXX Slight hack here to record ksym information for the PC
        # and PC-1, if that didn't have an instruction.  This allows
        # me to use PC-1 if I have two PCs that are listed as calling
        # different sync functions 
        new_pc = int(m.group('addr'), 16)
        for pc in range(max(new_pc-1, self.last_ksym_pc), new_pc + 1) :
            self.ksym[pc] = {'real':self.real_func, 'cfl':self.cfl,
                             'inlined':self.inlined_func}
        self.last_sym_pc = new_pc + 1


# Get symbol info for lock addresses
class GET_NM:
    def _init_fsm(self) :
        self.fsm = fsm_mod.FSM()
        self.fsm.start('scan')
        self.fsm.add('scan',  self.re_symbol_def, 'scan', self.set_symbol)
        self.fsm.add('scan',     None,             'scan',  None)


    def __init__(self, _nm, istr, _invert) :
        self.re_symbol_def = re.compile(r'^(?P<addr>[A-Fa-f0-9]+) \w (?P<symbol_name>[A-Za-z0-9_$#@./]+)')
        self.nm = _nm
        self.invert = _invert
        self._init_fsm()
        for line in istr :
            try:
                self.fsm.execute(line)
            except fsm_mod.FSMError:
                print 'Invalid input: %s' % line,


    def set_symbol(self, state, input, m) :
        if self.invert :
            self.nm[m.group('symbol_name')] = int(m.group('addr'), 16)
        else :
            self.nm[int(m.group('addr'), 16)] = m.group('symbol_name')

def commify(val) :
    if len(val) <= 3 : return val
    else : return "".join(commify(val[:-3]) + ',' + val[-3:])

def print_set (s) :
    f0 = f1 = f2 = ''
    try :
        while True :
            f0 = s.pop()
            f1 = s.pop()
            f2 = s.pop()
            print '    %22s %22s %22s' % (f0, f1, f2)
            f0 = f1 = f2 = ''
    except KeyError :
        if len(f0) or len(f1) or len(f2) :
            print '    %22s %22s %22s' % (f0, f1, f2)

def print_hash (h) :
    sh = h.items()
    sh.sort(lambda a,b: -cmp(h[a[0]], h[b[0]]))
    while True :
        try :
            print '    ',
            first_one = True
            print '%18s(%5d)' % sh.pop(0),
            first_one = False
            print '%18s(%5d)' % sh.pop(0),
            print '%18s(%5d)' % sh.pop(0)
        except IndexError :
            if not first_one :
                print
            break

def init_syms(ksym, nm_sym, objdump, nm, opt_vmlinux, opt_pickle, opt_pickle_only) :
    if opt_pickle :
        # If pickled ksym is out of date, recreate it
        try:
            pickle_time = os.stat(opt_pickle)[ST_MTIME]
        except os.error:
            pickle_time = 0
        kernel_time = os.stat(opt_vmlinux)[ST_MTIME]
        if kernel_time > pickle_time :
            GET_KSYM(ksym, os.popen('%s %s' % (objdump, opt_vmlinux)))
            GET_NM(nm_sym, os.popen('%s %s' % (nm, opt_vmlinux)), False)
            pickle_file = open(opt_pickle, 'wb')
            pickle.dump(ksym, pickle_file, -1)
            pickle.dump(nm_sym, pickle_file, -1)
            pickle_file.close()
            if opt_pickle_only:
                sys.exit(0)
        else :
            if opt_pickle_only:
                sys.exit(0)
                        
            # Read in the pickled data
            pickle_file = open(opt_pickle, 'rb')
            ksym = pickle.load(pickle_file)
            nm_sym = pickle.load(pickle_file)
            pickle_file.close()
    else:
        GET_KSYM(ksym, os.popen('%s %s' % (objdump, opt_vmlinux)))
        GET_NM(nm_sym, os.popen('%s %s' % (nm, opt_vmlinux)), False)

    return [ksym, nm_sym]

def init_syscalls(syscall_name_map, opt_unistd):
    syscall_nr_re = re.compile(r'#define __NR_(\w+)\s+(\d*)')
    syscall_number = 0
    for line in open(opt_unistd):
        m = syscall_nr_re.match(line)
        if m:
            # some syscalls are defined as (prev_syscall+x), and
            # there are gaps.  this seems to work out
            if len(m.group(2)) > 0:
                syscall_number = int(m.group(2))
            else:
                syscall_number += 1
            syscall_name_map[syscall_number] = m.group(1)
