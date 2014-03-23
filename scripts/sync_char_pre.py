#!/usr/bin/python
####################################################
## Run this file before doing a simics sync_char run.
## It produces the sync_char.map file that is read by
## The simics sync_char module.
import sys, os, re, sets, shutil, getopt, sync_common, fsm as fsm_mod

# Make sure . is in the path - common case
path = os.environ.get('PATH')
path += ':.'
os.environ['PATH'] = path
objdump = 'objdump-osa --disassemble --line-numbers '
nm = 'nm'
#######################################################################
## Here is the strategy.
## 1. Find the instruction that manipulates the lock and emit
#information for how to find the address of the lock.
## 2. The runtime will need to check the contents of the lock address
#before and after the instruction.  It needs to know the semantics of
#the lock to determine if the lock aquire/release was successful
## 3. Marking the return instruction for some routines that can take a
#non-trivial amount of time is difficult to do statically because of
#multiple return instructions and most of all tail call optimization.

## XXX These must agree with values in sync_char.cc and sync_char_post.py
## Flags.  Properties of each locking routine
F_LOCK      = 0x1    # It locks
F_UNLOCK    = 0x2    # It unlocks
F_TRYLOCK   = 0x4    # It trys to get the lock
F_INLINED   = 0x8    # Inlined function
F_EAX       = 0x10   # Address in 0(%EAX)
F_TIME_RET  = 0x20   # Once this instruction executes, note time until
                     # ret (allowing for additional call/ret pairs
                     # before ret) NOT USED
F_NOADDR    = 0x40   # No address for this lock, it is acq/rel at this pc
F_LOOP_UNROLL = 0x80 # The lock instruction is in a loop that the
                     # compiler unrolls.  This is only used for
                     # sync_char_pre, not in syncchar proper.

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
L_CXA       = 11
L_CXE       = 12

# Regular expressions to find the PC and address of the lock
re_lock_i = re.compile(r'^(?P<addr>[A-Fa-f0-9]+):\s+(?P<bytes>([0-9a-z][0-9a-z][ \t])+)\s*?\tlock .*?(?P<offset>[A-Fa-f0-9x]+)?\(?\%?(?P<reg>[a-z][a-z][a-z])?\)?$')
re_xcas_i = re.compile(r'^(?P<addr>[A-Fa-f0-9]+):\s+(?P<bytes>([0-9a-z][0-9a-z][ \t])+)\s*?\txcas.*')
#re_ret_i = re.compile(r'^(?P<addr>[A-Fa-f0-9]+):\s+.*?\tret\s+$')
#re_jmp_i = re.compile(r'^(?P<addr>[A-Fa-f0-9]+):\s+.*?jmp\s+')
#re_pause_i = re.compile(r'^(?P<addr>[A-Fa-f0-9]+):\s+.*?pause')
re_addr = re.compile(r'^(?P<addr>[A-Fa-f0-9]+):\s+')     
re_raw_spin_unlock_i = re.compile(r'^\s*(?P<addr>[A-Fa-f0-9]+):\s+(?P<bytes>([0-9a-z][0-9a-z][ \t])+)\s*?\tmovb\s+\$0x1,(?P<offset>[A-Fa-f0-9x]+)?\(?\%?(?P<reg>[a-z][a-z][a-z])?\)?$')
noninlined_funcs = {
    '_spin_lock'   :
    { 'flmatch'    : '',
      'flags'      : F_LOCK|F_TIME_RET,
      'sync_id'    : L_SPIN,
      're_lkaddr_i'   : re_lock_i,
      },
    '_spin_lock_irqsave' :
    { 'flmatch'    : '', # 'kernel/spinlock.c:73',
      'flags'      : F_LOCK|F_TIME_RET,
      'sync_id'    : L_SPIN,
      're_lkaddr_i'   : re_lock_i,
      },
    '_spin_lock_irq' :
    { 'flmatch'    : '',
      'flags'      : F_LOCK|F_TIME_RET,
      'sync_id'    : L_SPIN,
      're_lkaddr_i'   : re_lock_i,
      },
    '_spin_lock_bh' :
    { 'flmatch'    : '',
      'flags'      : F_LOCK|F_TIME_RET,
      'sync_id'    : L_SPIN,
      're_lkaddr_i'   : re_lock_i,
      },
    # Tx kernels
    '_cspin_lock'   :
    { 'flmatch'    : '',
      'flags'      : F_LOCK|F_TIME_RET,
      'sync_id'    : L_SPIN,
      're_lkaddr_i'   : re_lock_i,
      },
    '_cspin_lock_irqsave' :
    { 'flmatch'    : '', # 'kernel/spinlock.c:73',
      'flags'      : F_LOCK|F_TIME_RET,
      'sync_id'    : L_SPIN,
      're_lkaddr_i'   : re_lock_i,
      },
    '_cspin_lock_irq' :
    { 'flmatch'    : '',
      'flags'      : F_LOCK|F_TIME_RET,
      'sync_id'    : L_SPIN,
      're_lkaddr_i'   : re_lock_i,
      },
    '_cspin_lock_bh' :
    { 'flmatch'    : '',
      'flags'      : F_LOCK|F_TIME_RET,
      'sync_id'    : L_SPIN,
      're_lkaddr_i'   : re_lock_i,
      },


    '_read_trylock':
    { 'flmatch'    : '', 
      'flags'      : F_TRYLOCK,
      'sync_id'    : L_RSPIN,
      're_lkaddr_i'   : re_lock_i,
      },
    '_write_trylock' :
    { 'flmatch'   : '', # kernel/spinlock.c:53
      'flags'      : F_TRYLOCK,
      'sync_id'    : L_WSPIN,
      're_lkaddr_i'   : re_lock_i,
      },
    # Look for the non-inlined form of these functions to get
    # information about their caller.  From kernel/spinlock.c
    '_read_lock_irqsave' :
    {'flmatch'    : '',
     'flags'      : F_LOCK|F_TIME_RET,
     'sync_id'    : L_RSPIN,
     're_lkaddr_i' : re_lock_i,
     },
    '_read_lock_irq' :
    {'flmatch'    : '',
     'flags'      : F_LOCK|F_TIME_RET,
     'sync_id'    : L_RSPIN,
     're_lkaddr_i' : re_lock_i,
     },
    '_read_lock_bh' :
    {'flmatch'    : '',
     'flags'      : F_LOCK|F_TIME_RET,
     'sync_id'    : L_RSPIN,
     're_lkaddr_i' : re_lock_i,
     },
    '_read_lock' :
    {'flmatch'    : '',
     'flags'      : F_LOCK|F_TIME_RET,
     'sync_id'    : L_RSPIN,
     're_lkaddr_i' : re_lock_i,
     },
    '_write_lock_irqsave' :
    {'flmatch'    : '',
     'flags'      : F_LOCK|F_TIME_RET,
     'sync_id'    : L_WSPIN,
     're_lkaddr_i' : re_lock_i,
     },
    '_write_lock_irq' :
    {'flmatch'    : '',
     'flags'      : F_LOCK|F_TIME_RET,
     'sync_id'    : L_WSPIN,
     're_lkaddr_i' : re_lock_i,
     },
    '_write_lock_bh' :
    {'flmatch'    : '',
     'flags'      : F_LOCK|F_TIME_RET,
     'sync_id'    : L_WSPIN,
     're_lkaddr_i' : re_lock_i,
     },
    '_write_lock' :
    {'flmatch'    : '',
     'flags'      : F_LOCK|F_TIME_RET,
     'sync_id'    : L_WSPIN,
     're_lkaddr_i' : re_lock_i,
     },
    #######################################################
    ## Mutexes
    'mutex_lock'   :
    {'flmatch'     : '',
     'flags'       : F_LOCK|F_TIME_RET,
     'sync_id'     : L_MUTEX,
     're_lkaddr_i'    : re_lock_i,
     },
    # Problems here, slow path is via jmp
    'mutex_lock_interruptible'   :
    {'flmatch'     : '',
     'flags'      : F_LOCK|F_TIME_RET,
     'sync_id'    : L_MUTEX,
     're_lkaddr_i'   : re_lock_i,
     },
    'mutex_trylock':
    {'flmatch'     : '',
     'flags'      : F_TRYLOCK,
     'sync_id'    : L_MUTEX,
     're_lkaddr_i'   : re_lock_i,
     },
    'mutex_unlock' :
    {'flmatch'     : '',
      'flags'      : F_UNLOCK|F_TIME_RET,
      'sync_id'    : L_MUTEX,
      're_lkaddr_i'   : re_lock_i,
     },
    #######################################################
    ## Semaphore slowpath
    '__down'   :
    {'flmatch'     : '',
     'flags'       : F_LOCK|F_TIME_RET|F_LOOP_UNROLL,
     'sync_id'     : L_SEMA,
     're_lkaddr_i' : re_lock_i,
     },
    
    # Many of these functions do some work and are tail call optimized
    # XXX Completion is not like other locks in the sense that there
    # is no single word whose state encapsulates the state of the
    # completion.  Also, the struct completion* seems to be aliasing
    # with the spin lock in the wait_queue_head_t (its the first
    # member of the wait_queue_head_t), even though the done
    # field should be at the struct completion* address. 
    # For now, just count them.
    'complete'     :
    { 'flmatch'    : '',
      'flags'      : F_UNLOCK|F_EAX|F_TIME_RET,
      'sync_id'    : L_COMPL,
      're_lkaddr_i'   : re_addr,
      },
    'complete_all' :
    { 'flmatch'    : '',
      'flags'      : F_UNLOCK|F_EAX|F_TIME_RET,
      'sync_id'    : L_COMPL,
      're_lkaddr_i'   : re_addr,
      },
    'wait_for_completion'     :
    { 'flmatch'    : '',
      'flags'      : F_LOCK|F_EAX,
      'sync_id'    : L_COMPL,
      're_lkaddr_i'   : re_addr,
      },
    'wait_for_completion_timeout'     :
    { 'flmatch'    : '', 
      'flags'      : F_LOCK|F_EAX,
      'sync_id'    : L_COMPL,
      're_lkaddr_i'   : re_addr,
      },
    'wait_for_completion_interruptible_timeout' :
    { 'flmatch'    : '',
      'flags'      : F_LOCK|F_EAX,
      'sync_id'    : L_COMPL,
      're_lkaddr_i'   : re_addr,
      },
    'wait_for_completion_interruptible' :
    { 'flmatch'     : '',
      'flags'       : F_LOCK|F_EAX,
      'sync_id'     : L_COMPL,
      're_lkaddr_i' : re_addr,
      },
#    'sys_futex' : # Maybe don't need caller, since this is syscall
#    { 'flmatch'    : '', 
#      'str'        : '0 10'
#      },

   ####################################
   # cxspinlocks

   '_cx_atomic' :
   { 'flmatch'    : '',
     'flags'      : F_LOCK|F_EAX|F_LOOP_UNROLL,
     'sync_id'    : L_CXA,
     're_lkaddr_i': re_xcas_i,
     },
   '_cx_atomic_bh' :
   { 'flmatch'    : '',
     'flags'      : F_LOCK|F_EAX|F_LOOP_UNROLL,
     'sync_id'    : L_CXA,
     're_lkaddr_i': re_xcas_i,
     },
   '_cx_atomic_irq' :
   { 'flmatch'    : '',
     'flags'      : F_LOCK|F_EAX|F_LOOP_UNROLL,
     'sync_id'    : L_CXA,
     're_lkaddr_i': re_xcas_i,
     },
   '_cx_atomic_irqsave' :
   { 'flmatch'    : '',
     'flags'      : F_LOCK|F_EAX|F_LOOP_UNROLL,
     'sync_id'    : L_CXA,
     're_lkaddr_i': re_xcas_i,
     },
   '_cx_exclusive' :
   { 'flmatch'    : '',
     'flags'      : F_LOCK|F_EAX,
     'sync_id'    : L_CXE,
     're_lkaddr_i': re_xcas_i,
     },
   '_cx_exclusive_bh' :
   { 'flmatch'    : '',
     'flags'      : F_LOCK|F_EAX,
     'sync_id'    : L_CXE,
     're_lkaddr_i': re_xcas_i,
     },
   '_cx_exclusive_irq' :
   { 'flmatch'    : '',
     'flags'      : F_LOCK|F_EAX,
     'sync_id'    : L_CXE,
     're_lkaddr_i': re_xcas_i,
     },
   '_cx_exclusive_irqsave' :
   { 'flmatch'    : '',
     'flags'      : F_LOCK|F_EAX,
     'sync_id'    : L_CXE,
     're_lkaddr_i': re_xcas_i,
     },
   '_cx_unlock_wait' :
   { 'flmatch'    : '',
     'flags'      : F_EAX,
     'sync_id'    : L_CXA,
     're_lkaddr_i': re_xcas_i,
     },
   '_cx_atomic_trylock' :
   { 'flmatch'    : '',
     'flags'      : F_TRYLOCK|F_EAX,
     'sync_id'    : L_CXA,
     're_lkaddr_i': re_xcas_i,
     },
   '_cx_atomic_trylock_bh' :
   { 'flmatch'    : '',
     'flags'      : F_TRYLOCK|F_EAX,
     'sync_id'    : L_CXA,
     're_lkaddr_i': re_xcas_i,
     },
   '_cx_exclusive_trylock' :
   { 'flmatch'    : '',
     'flags'      : F_TRYLOCK|F_EAX,
     'sync_id'    : L_CXE,
     're_lkaddr_i': re_xcas_i,
     },
   '_cx_exclusive_trylock_bh' :
   { 'flmatch'    : '',
     'flags'      : F_TRYLOCK|F_EAX,
     'sync_id'    : L_CXE,
     're_lkaddr_i': re_xcas_i,
     },

   # we only track cx_exclusive unlocks
   '_cx_end' :
   {
     'flmatch'    : '',
     'flags'      : F_UNLOCK,
     'sync_id'    : L_CXE,
     're_lkaddr_i': re_raw_spin_unlock_i,
   },
   '_cx_end_bh' :
   {
     'flmatch'    : '',
     'flags'      : F_UNLOCK,
     'sync_id'    : L_CXE,
     're_lkaddr_i': re_raw_spin_unlock_i,
   },
   '_cx_end_irq' :
   {
     'flmatch'    : '',
     'flags'      : F_UNLOCK,
     'sync_id'    : L_CXE,
     're_lkaddr_i': re_raw_spin_unlock_i,
   },
   '_cx_end_irqrestore' :
   {
     'flmatch'    : '',
     'flags'      : F_UNLOCK,
     'sync_id'    : L_CXE,
     're_lkaddr_i': re_raw_spin_unlock_i,
   },
}

re_down_i = re.compile(r'^(?P<addr>[A-Fa-f0-9]+):\s+(?P<bytes>([0-9a-z][0-9a-z][ \t])+)\s*?\tlock decl (?P<offset>[A-Fa-f0-9x]+)?\(?\%?(?P<reg>[a-z][a-z][a-z])?\)?$')
re_down_read_i = re.compile(r'^(?P<addr>[A-Fa-f0-9]+):\s+(?P<bytes>([0-9a-z][0-9a-z][ \t])+)\s*?\tlock incl (?P<offset>[A-Fa-f0-9x]+)?\(\%(?P<reg>eax)\)$')
re_down_read_trylock_i = re.compile(r'^(?P<addr>[A-Fa-f0-9]+):\s+(?P<bytes>([0-9a-z][0-9a-z][ \t])+)\s*?\tlock cmpxchg.*?(?P<offset>[A-Fa-f0-9x]+)?\(?\%?(?P<reg>[a-z][a-z][a-z])?\)?$')
re_up_i = re.compile(r'^(?P<addr>[A-Fa-f0-9]+):\s+(?P<bytes>([0-9a-z][0-9a-z][ \t])+)\s*?\tlock incl (?P<offset>[A-Fa-f0-9x]+)?\(?\%?(?P<reg>[a-z][a-z][a-z])?\)?$')
re_raw_read_unlock_i = re_up_i
re_down_write_i = re.compile(r'^(?P<addr>[A-Fa-f0-9]+):\s+(?P<bytes>([0-9a-z][0-9a-z][ \t])+)\s*?\tlock xadd.*?(?P<offset>[A-Fa-f0-9x]+)?\(?\%?(?P<reg>[a-z][a-z][a-z])?\)?$')
re_up_write_i = re_up_read_i = re_down_write_i
re_raw_write_unlock_i = re.compile(r'^(?P<addr>[A-Fa-f0-9]+):\s+(?P<bytes>([0-9a-z][0-9a-z][ \t])+)\s*?\tlock addl \$0x1000000,(?P<offset>[A-Fa-f0-9x]+)?\(?\%?(?P<reg>[a-z][a-z][a-z])?\)?$')
re_raw_spin_trylock_i = re.compile(r'^(?P<addr>[A-Fa-f0-9]+):\s+(?P<bytes>([0-9a-z][0-9a-z][ \t])+)\s*\txchg .*?(?P<offset>[A-Fa-f0-9x]+)?\(?\%?(?P<reg>[a-z][a-z][a-z])?\)?$')
re_raw_read_lock_i = re.compile(r'^(?P<addr>[A-Fa-f0-9]+):\s+(?P<bytes>([0-9a-z][0-9a-z][ \t])+)\s*?\tlock subl \$0x1,(?P<offset>[A-Fa-f0-9x]+)?\(?\%?(?P<reg>[a-z][a-z][a-z])?\)?$')
re_raw_write_lock_i = re.compile(r'^(?P<addr>[A-Fa-f0-9]+):\s+(?P<bytes>([0-9a-z][0-9a-z][ \t])+)\s*?\tlock subl \$0x1000000,(?P<offset>[A-Fa-f0-9x]+)?\(?\%?(?P<reg>[a-z][a-z][a-z])?\)?$')
re_raw_spin_lock_i = re.compile(r'^\s*(?P<addr>[A-Fa-f0-9]+):\s+(?P<bytes>([0-9a-z][0-9a-z][ \t])+)\s*?\tlock decb\s+(?P<offset>[A-Fa-f0-9x]+)?\(?\%?(?P<reg>[a-z][a-z][a-z])?\)?$')
re_rwsem_atomic_add_i =re.compile(r'^(?P<addr>[A-Fa-f0-9]+):\s+(?P<bytes>([0-9a-z][0-9a-z][ \t])+)\s*?\tlock add.*?,(?P<offset>[A-Fa-f0-9x]+)?\(?\%?(?P<reg>[a-z][a-z][a-z])?\)?$')
re_nop_i = re.compile(r'^(?P<addr>[A-Fa-f0-9]+):\s+(?P<bytes>([0-9a-z][0-9a-z][ \t])+)\s*\tnop\s+$')
re_xchg_i = re.compile(r'^(?P<addr>[A-Fa-f0-9]+):\s+(?P<bytes>([0-9a-z][0-9a-z][ \t])+)\s*\txchg .*?(?P<offset>[A-Fa-f0-9x]+)?\(?\%?(?P<reg>[a-z][a-z][a-z])?\)?$')
re_movl_i = re.compile(r'^(?P<addr>[A-Fa-f0-9]+):\s+(?P<bytes>([0-9a-z][0-9a-z][ \t])+)\s*?\tmovl\s+\$0x1,(?P<offset>[A-Fa-f0-9x]+)?\(?\%?(?P<reg>[a-z][a-z][a-z])?\)?$')

# INLINED functions
re_i_s = [
    {'i'          : re_down_i,
     'func_name'  : 'down',
     'flmatch'    : 'include/asm/semaphore.h:100',
     'flags'      : F_INLINED|F_LOCK,
     'sync_id'    : L_SEMA,
     },
    {'i'          : re_down_i,
     'func_name'  : 'down_interruptible',
     'flmatch'    : 'include/asm/semaphore.h:124',
     'flags'      : F_INLINED|F_LOCK,
     'sync_id'    : L_SEMA,
     },
    # This is the inlined public interface.  It calls
    # __down_failed_trylock which calls __down_trylock in
    # lib/semaphore-sleepers.c:158 
    {'i'          : re_down_i,
     'func_name'  : 'down_trylock',
     'flmatch'    : 'include/asm/semaphore.h:149',
     'flags'      : F_INLINED|F_TRYLOCK,
     'sync_id'    : L_SEMA,
     },
    {'i'         : re_up_i,
     'func_name' : 'up',
     'flmatch'    : 'include/asm/semaphore.h:174',
     'flags'      : F_INLINED|F_UNLOCK,
     'sync_id'    : L_SEMA,
     },
    {'i'          : re_down_read_i,
     'func_name'  : '__down_read',
     'flmatch'    : 'include/asm/rwsem.h:101',
     'flags'      : F_INLINED|F_LOCK,
     'sync_id'    : L_RSEMA,
     },
    {'i'          : re_down_read_trylock_i,
     'func_name'  : '__down_read_trylock',
     'flmatch'    : 'include/asm/rwsem.h:127',
     'flags'      : F_INLINED|F_TRYLOCK,
     'sync_id'    : L_RSEMA,
     },
    {'i'          : re_down_write_i,
     'func_name'  : '__down_write',
     'flmatch'    : 'include/asm/rwsem.h:152',
     'flags'      : F_INLINED|F_LOCK,
     'sync_id'    : L_WSEMA,
     },
    {'i'          : re_up_read_i,
     'func_name'  : '__up_read',
     'flmatch'    : 'include/asm/rwsem.h:190',
     'flags'      : F_INLINED|F_UNLOCK,
     'sync_id'    : L_RSEMA,
     },
    {'i'          : re_up_write_i,
     'func_name'  : '__up_write',
     'flmatch'    : 'include/asm/rwsem.h:215',
     'flags'      : F_INLINED|F_UNLOCK,
     'sync_id'    : L_WSEMA,
     },
    # Inlined into __reacquire_kernel_lock & _spin_trylock_bh & _spin_trylock
    {'i'          : re_raw_spin_trylock_i,
     'func_name'  : '__raw_spin_trylock',
     'flmatch'       : 'include/asm/spinlock.h:69',
     'flags'      : F_INLINED|F_TRYLOCK,
     'sync_id'    : L_SPIN,
     },
    # Both are inlined into many functions including
    # _spin_unlock{,_bh,_irqrestore,...} 
    {'i'          : re_raw_spin_unlock_i,
     'func_name'  : '__raw_spin_unlock',
     'flmatch'    : 'include/asm/spinlock.h:92', 
     'flags'      : F_INLINED|F_UNLOCK,
     'sync_id'    : L_SPIN,
     },
    {'i'          : re_raw_spin_lock_i,
     'func_name'  : '__raw_spin_lock',
     'flmatch'       : 'include/asm/spinlock.h:54', 
     'flags'         : F_INLINED|F_LOCK,
     'sync_id'       : L_SPIN,
     },
    # Covers several, including generic__raw_read_trylock
    # 2.6.16-unmod
    {'i'          : re_raw_read_lock_i,
     'func_name'  : '__raw_read_lock',
     'flmatch'     : 'include/asm/spinlock.h:153',
     'flags'       : F_INLINED|F_LOCK|F_TIME_RET,
     'sync_id'     : L_RSPIN,
     },
    # 2.6.16 - tx
    {'i'          : re_raw_read_lock_i,
     'func_name'  : '__raw_read_lock',
     'flmatch'     : 'include/asm/spinlock.h:167',
     'flags'       : F_INLINED|F_LOCK|F_TIME_RET,
     'sync_id'     : L_RSPIN,
     },

    # All flavors of _write_lock share a retry loop
    # 2.6.16-unmod
    {'i'          : re_raw_write_lock_i,
     'func_name'  : '__raw_write_lock',
     'flmatch'    : 'include/asm/spinlock.h:158',
     'flags'      : F_INLINED|F_LOCK|F_TIME_RET,
     'sync_id'    : L_WSPIN,
     },
    # 2.6.16 - tx
    {'i'          : re_raw_write_lock_i,
     'func_name'  : '__raw_write_lock',
     'flmatch'    : 'include/asm/spinlock.h:172',
     'flags'      : F_INLINED|F_LOCK|F_TIME_RET,
     'sync_id'    : L_WSPIN,
     },


    # write_lock slowpath
    {'i'          : re_raw_write_lock_i,
     'func_name'  : '__write_lock_failed',
     'flmatch'    : 'only match func_name',
     'flags'      : F_LOCK|F_INLINED,
     'sync_id'    : L_WSPIN,
     },
    # read_lock slowpath
    {'i'          : re_down_i,
     'func_name'  : '__read_lock_failed',
     'flmatch'    : 'only match func_name',
     'flags'      : F_LOCK|F_INLINED,
     'sync_id'    : L_RSPIN,
     },
    # rwsem_atomic_add used to grant read locks en-masse in rwsem.c
    {'i'          : re_rwsem_atomic_add_i,
     'func_name'  : 'rwsem_atomic_add',
     'flmatch'    : 'include/asm/rwsem.h:266',
     'flags'      : F_LOCK|F_INLINED,
     'sync_id'    : L_RSEMA,
     },
    {'i'          : re_down_write_i,
     'func_name'  : 'rwsem_atomic_update',
     'flmatch'    : 'include/asm/rwsem.h:279',
     'flags'      : F_INLINED, # Could be lock or unlock
     'sync_id'    : L_WSEMA,
     },

    # 2.6.16 - unmod
    {'i'          : re_raw_read_unlock_i,
     'func_name'  : '__raw_read_unlock',
     'flmatch'    : 'include/asm/spinlock.h:182',
     'flags'      : F_UNLOCK|F_INLINED,
     'sync_id'    : L_RSPIN,
     },
    {'i'          : re_raw_write_unlock_i,
     'func_name'  : '__raw_write_unlock',
     'flmatch'    : 'include/asm/spinlock.h:187',
     'flags'      : F_UNLOCK|F_INLINED,
     'sync_id'    : L_WSPIN,
     },
    # 2.6.16 - tx
    {'i'          : re_raw_read_unlock_i,
     'func_name'  : '__raw_read_unlock',
     'flmatch'    : 'include/asm/spinlock.h:196',
     'flags'      : F_UNLOCK|F_INLINED,
     'sync_id'    : L_RSPIN,
     },
    {'i'          : re_raw_write_unlock_i,
     'func_name'  : '__raw_write_unlock',
     'flmatch'    : 'include/asm/spinlock.h:201',
     'flags'      : F_UNLOCK|F_INLINED,
     'sync_id'    : L_WSPIN,
     },

    
    {'i'          : re_nop_i,
     'func_name'  : 'rcu_read_lock',
     'flmatch'     : 'include/linux/rcupdate.h:168',
     'flags'       : F_INLINED|F_LOCK|F_NOADDR,
     'sync_id'     : L_RCU,
     },
    {'i'           : re_xchg_i,
     'func_name'   : '__mutex_lock_common',
     'flmatch'     : 'only match func_name',
     'flags'       : F_INLINED|F_LOCK,
     'sync_id'     : L_MUTEX,
     },
    {'i'           : re_movl_i,
     'func_name'   : '__mutex_unlock_slowpath',
     'flmatch'     : 'kernel/mutex.c:238',
     'flags'       : F_INLINED|F_LOCK,
     'sync_id'     : L_MUTEX,
     },

##################### 2.4 Kernel spinlocks and rwlocks
    {'i'          : re_raw_spin_trylock_i,
     'func_name'  : 'spin_trylock',
     'flmatch'       : 'include/asm/spinlock.h:224',
     'flags'      : F_INLINED|F_TRYLOCK,
     'sync_id'    : L_SPIN,
     },
    {'i'          : re_raw_spin_unlock_i,
     'func_name'  : 'spin_unlock',
     'flmatch'    : 'include/asm/spinlock.h:193', 
     'flags'      : F_INLINED|F_UNLOCK,
     'sync_id'    : L_SPIN,
     },
    {'i'          : re_raw_spin_lock_i,
     'func_name'  : 'spin_lock',
     'flmatch'       : 'include/asm/spinlock.h:241', 
     'flags'         : F_INLINED|F_LOCK,
     'sync_id'       : L_SPIN,
     },
    {'i'          : re_raw_spin_trylock_i,
     'func_name'  : 'raw_spin_trylock',
     'flmatch'       : 'include/asm/spinlock.h:127',
     'flags'      : F_INLINED|F_TRYLOCK,
     'sync_id'    : L_SPIN,
     },
    {'i'          : re_raw_spin_unlock_i,
     'func_name'  : 'raw_spin_unlock',
     'flmatch'    : 'include/asm/spinlock.h:96', 
     'flags'      : F_INLINED|F_UNLOCK,
     'sync_id'    : L_SPIN,
     },
    {'i'          : re_raw_spin_lock_i,
     'func_name'  : 'raw_spin_lock',
     'flmatch'       : 'include/asm/spinlock.h:144', 
     'flags'         : F_INLINED|F_LOCK,
     'sync_id'       : L_SPIN,
     },
    {'i'          : re_raw_read_lock_i,
     'func_name'  : 'read_lock',
     'flmatch'     : 'include/asm/spinlock.h:294',
     'flags'       : F_INLINED|F_LOCK|F_TIME_RET,
     'sync_id'     : L_RSPIN,
     },
    {'i'          : re_raw_write_lock_i,
     'func_name'  : 'write_lock',
     'flmatch'    : 'include/asm/spinlock.h:303',
     'flags'      : F_INLINED|F_LOCK|F_TIME_RET,
     'sync_id'    : L_WSPIN,
     },
    {'i'          : re_raw_write_lock_i,
     'func_name'  : '__write_lock_failed',
     'flmatch'    : 'only match func_name',
     'flags'      : F_LOCK|F_INLINED,
     'sync_id'    : L_WSPIN,
     },
    {'i'          : re_down_i,
     'func_name'  : '__read_lock_failed',
     'flmatch'    : 'only match func_name',
     'flags'      : F_LOCK|F_INLINED,
     'sync_id'    : L_RSPIN,
     },
    {'i'          : re_raw_read_unlock_i,
     'func_name'  : 'read_unlock',
     'flmatch'    : 'include/asm/spinlock.h:320',
     'flags'      : F_UNLOCK|F_INLINED,
     'sync_id'    : L_RSPIN,
     },
    {'i'          : re_raw_write_unlock_i,
     'func_name'  : 'write_unlock',
     'flmatch'    : 'include/asm/spinlock.h:325',
     'flags'      : F_UNLOCK|F_INLINED,
     'sync_id'    : L_WSPIN,
     },
    ################ End 2.4 locks

    ]

##################################################################
## Sync_char_pre works in two passes.  The first collects information
## from objdump on vmlinux into an array.  The second phase resolves
## dependencies amoung array entries, and prints the array to the
## output file 

## Function record base class, and subclasses
class func_rec :
    def __init__(self, func_pc, func_name, lock_pc) :
        self.func_pc = func_pc
        self.func_name = func_name
        self.lock_pc = lock_pc
    # Vestige of earlier error where we had inter-instruction dependencies.
    def resolve(self) : pass
    def pr(self, ostr) :
        pass
class fr_lock(func_rec) :
    def __init__(self, func_pc, func_name, lock_pc, lock_off, lock_reg,
                 flags, sync_id) :
        func_rec.__init__(self, func_pc, func_name, lock_pc)
        self.lock_off = lock_off
        self.lock_reg = lock_reg
        self.flags    = flags
        self.sync_id  = sync_id
    def resolve(self) : pass
    def pr(self, ostr) :
        print >>ostr, '0x%x %10s %s %d %#4x 0x%8s %s' % \
              (int(self.lock_pc, 16), self.lock_off, self.lock_reg, self.sync_id,
               self.flags, self.func_pc, self.func_name)

## This class parses the output of objdump using a state machine.
class SCAN_OBJ:
    '''Scan output of objdump using a  state machine to look for important kernel features'''
    ni_funcs = noninlined_funcs
    def _init_fsm(self) :
        '''Initialize the finite state machine that scans objdump --disassemble --line-number output.'''
        self.fsm = fsm_mod.FSM()
        self.fsm.start('scan')
        # Start state: scan
        # 'in_func' entered at a function definition where we find
        # a non-inlined function in s_func dict.  While in
        # 'in_func' we do not look for inlined functions, but we do
        # look for lock/unlock instructions to find how their address
        # is generated.  Return to scan on finding the next func_def.
        # 'inlined' entered when we find an inlined function in s_func
        # We look for lock/unlock instructions, and return to 'scan'
        # when we find one.
        # If we are not interested, transition to 'scan' by raising RestartError
        self.fsm.add('scan',     self.re_func_def,  'in_func',   self.in_func)
        # Record inlined function name
        self.fsm.add('scan',     self.re_func,      'inlined',self.set_func_name)
        # Record filename & line number info
        self.fsm.add('scan',     self.re_fl,        'inlined',self.set_fl)
        # Default scan action, keep scanning
        self.fsm.add('scan',     None,              'scan',   None)

        self.fsm.add('inlined',  self.re_fl,        'inlined',self.set_fl)
        # Allow inlined func name to change while looking for inlined func
        self.fsm.add('inlined',  self.re_func,      'inlined',self.set_func_name)
        # First check for any inst in case we are in nexti and inlined
        # found_nexti also searches for any important instruction
        self.fsm.add('inlined',  self.re_anyinst_i, 'inlined',self.found_nexti)
        # If we don't find lock addr instruction, go back to scanning
        self.fsm.add('inlined',  self.re_func_def,  'in_func',  self.in_func)
        self.fsm.add('inlined',  None,              'inlined', None)

    # Our fsm implementation does not support dyanmically changing the regexps
    # that form the transition arcs, so each time we change the regex, install
    # all transition rules for that state.
    def _fsm_in_func(self, re_lockaddr_i, unrolled_loop) :
        self.fsm.del_state('in_func')
        # Store this here in case we need to print it later
        self.re_lockaddr_i = re_lockaddr_i
        # If we are within a function and find a function definition,
        # see if we are interested, otherwise switch back to 'scan'
        self.fsm.add('in_func',  self.re_func_def,  'in_func',   self.in_func)
        # Search for instruction that has lock address
        # NB: We would pick up an inlined function after
        # the lock instruction for this non-inlined function, but we
        # don't pick up another lockaddr_i.

        # In the case of loop unrolling that involves the locked
        # instruction (e.g. __down), we can't quit on the first lock
        # instruction.  Instead, we use the empty line objdump inserts
        # between functions.
        if unrolled_loop :
            self.fsm.add('in_func',  self.re_lockaddr_i,'in_func',self.found_lock_addr)
            self.fsm.add('in_func',  self.re_whitespace,'scan',None)
        else :
            self.fsm.add('in_func',  self.re_lockaddr_i,'scan',self.found_lock_addr)
        self.fsm.add('in_func',  None,              'in_func',None)

    def __init__(self) :
        '''Initialize SCAN_OBJ object.'''
        self.func_name  = ''
        # Where the last function (not inlined) started
        self.func_pc  = '0'
        # Current inline filename & line number
        self.file_line = 'bogus file and line'
        self.lock_ra = '0' # nexti is associated with this lock RA
        self.insts = [] # List of instrumentation points
        self.re_func_def = re.compile(r'^(?P<addr>[A-Fa-f0-9]+) <(?P<func_name>[A-Za-z0-9_$#@./]+)>:')
        self.re_func = re.compile(r'^(?P<func_name>[A-Za-z0-9_$#@./]+)\(\):')
        self.re_fl   = re.compile(r'^(?P<file_line>[A-Za-z0-9_$#@./-]+:[0-9]+)')
        self.re_whitespace = re.compile(r'^\s*$')
        # Instruction must have opcode.  A line might be a
        # continuation of previous line, e.g.,  
        # c016baa9:	f0 81 05 6c 9b 2e c0 	lock addl $0x1000000,0xc02e9b6c
        # c016bab0:	00 00 00 01 
        self.re_anyinst_i = re.compile(r'''
           ^\s*(?P<addr>[A-Fa-f0-9]+):\s+              # The instruction address, can be prefixed with whitespace in a smaller app
           (?P<bytes>([0-9a-z][0-9a-z][ \t])+)\s*?\t[a-z]+.*?$
        ''', re.VERBOSE)
        # Detect direct and indirect calls, e.g., call   *0x38(%edx)
        # call   c02a01da <mutex_unlock>, call   *%eax
        # self.re_call_i = re.compile(r'^(?P<addr>[A-Fa-f0-9]+):\s+.*?\tcall\s+(\*[a-zA-F0-9x%]+|[a-fA-F0-9x]+ )')
        # This gets set dynamically to 're_lkaddr_i' of function we are looking at
        self._init_fsm()

    # Remember the function name
    def set_func_name(self, state, input, m) :
        # Special case for the xchg in the mutex implementation.
        # Since we don't actually care about the inlined __xchg()
        # function, we just don't set its name as such.
        if m.group('func_name') != '__xchg' :
            self.func_name = m.group('func_name')
        
    def record_lock_info(self, m, func_name, flags, sync_id) :
        assert func_name != '', func_name
        assert m.group('addr') != '0', (func_name, m.group(0))
        addr_off = '0x0'
        addr_reg = 'nil'
        try :
            addr_off = m.group('offset')
            if addr_off == None: addr_off = '0x0'
        except IndexError : pass
        try:
            addr_reg = m.group('reg')
            if addr_reg == None: addr_reg = 'nil'
        except IndexError : pass
        # Override match for F_EAX flag
        if flags & F_EAX :
            addr_reg = 'eax'
        if (flags & F_NOADDR) == 0 :
            # No atheists in foxholes and no locks at address 0
            assert not (addr_off == '0x0' and addr_reg == 'nil'), func_name
        # Append the record
        self.insts.append(fr_lock(self.func_pc, func_name, m.group('addr'),
                                  addr_off, addr_reg, flags, sync_id))
        del addr_off, addr_reg

    def real_found_lock_addr(self, state, input, m, func_name, lock_ra, flags,
                             sync_id) :
        self.record_lock_info(m, func_name, flags, sync_id)
        self.lock_ra = lock_ra
        # F_EAX means mark this address, the first address of the function
        if (flags & F_EAX) == 0 :
            # Insert record with bogus "zero" address.  We will look
            # for this flag when we fine the next instruction.  If
            # something gets messed up, the zero will show up in the output as an
            # address (e.g., "0x    zero") indicating something is seriously wrong.
            self.insts.append(func_rec(self.lock_ra, func_name, "zero"))

    def found_lock_addr(self, state, input, m) :
        self.real_found_lock_addr(state, input, m, self.func_name,
                                  m.group('addr'),
                                  self.ni_funcs[self.func_name]['flags'],
                                  self.ni_funcs[self.func_name]['sync_id'])

    def found_nexti(self, state, input, m) :
        try:
            if self.insts[len(self.insts) - 1].lock_pc == "zero" :
                self.insts[len(self.insts) - 1].lock_pc = m.group('addr')
        except IndexError:
            # Transient when self.insts is empty
            pass
        # Do work of text parsing state machine manually, doing this transition
        lock_ra = m.group('addr')
        for re_i in re_i_s :
            m = re_i['i'].match(input)
            if m :
                if self.func_name == re_i['func_name'] \
                   or self.file_line.find(re_i['flmatch']) != -1 :
                    if opt_verbose :
                        print self.func_name, self.file_line
                        print input,
                    self.real_found_lock_addr(state, input, m, re_i['func_name'],
                                              lock_ra, re_i['flags'],
                                              re_i['sync_id'])

    def in_func(self, state, input, m) :
        self.func_name = m.group('func_name')
        self.func_pc = m.group('addr')
        try :
            fr = self.ni_funcs[self.func_name]
            # Ignore all inlined calls until next function
            if opt_verbose :
                print 'IN_FUNC %s %s %s' % (self.func_name,
                                            self.func_pc,
                                            fr['flmatch'])
            # Look for this instruction in this function
            self._fsm_in_func(self.ni_funcs[self.func_name]['re_lkaddr_i'], fr['flags'] & F_LOOP_UNROLL)
        except KeyError :
            self.func_name = '' # 'scan' continues,
                                # but we are not interested in this function
            raise fsm_mod.RestartException('scan')
        
    def set_fl(self, state, input, m) :
        '''Call this when we see source line information in the input'''
        self.file_line = m.group('file_line')

    def inlined_err(self, state, input, m) :
        if self.re_lockaddr_i == None :
            print 'Inlined func %s\n\tFound:%s'%(
                self.func_name, m.group(0))
        else :
            print 'Inlined func %s, but no instr matching\n\t%s\n\tFound:%s'%(
                self.func_name, self.re_lockaddr_i.pattern, m.group(0))
    def nexti_err(self, state, input, m) :
        print 'Nexti error %s\n%s' %(self.func_name, m.group(0))

    def scan(self, istr, ostr) :
        '''Scan the input stream for functions and their return instructions.'''
        for line in istr :
            try:
                self.fsm.execute(line)
            except fsm_mod.FSMError:
                print 'Invalid input: %s' % line,
        ##fsm.execute(fsm_mod.FSMEOF) ## No cleanup action needed
        ## Now actualy resolve and print records
        for inst in self.insts :
            inst.resolve()
            inst.pr(ostr)

def get_static_locks(ostr, nm, vmlinux) :
    nm_sym = {}
    sync_common.GET_NM(nm_sym, os.popen('%s %s' % (nm, vmlinux)), True)
    spinlock_names = ['kernel_flag', 'pool_lock', 'vfsmount_lock',
                      'dcache_lock', 'logbuf_lock', 'i8259A_lock',
                      'files_lock', 'mmlist_lock', 'unix_table_lock',
                      'inet_peer_unused_lock', 'pci_bus_lock', 'kmap_lock',
                      'inode_lock', 'i8253_lock', 'rtc_lock',
                      'tty_ldisc_lock', 'vga_lock', 'ide_lock',
                      'call_lock', 'sb_lock', 'unnamed_dev_lock',
                      # This is actually a timer_base_t object that is
                      # used for non-per-cpu timers.  The first field
                      # is a spinlock, so we can use its address to
                      # get these static locks.
                      '__init_timer_base',
                      'proc_inum_lock', 'set_atomicity_lock',
                      'ioapic_lock', 'workqueue_lock', 'net_family_lock',
                      'pci_config_lock', 'sequence_lock', 'pci_lock',
                      'uidhash_lock', 'pdflush_lock', 'bdev_lock',
                      'swap_lock', 'mb_cache_spinlock',
                      'cache_list_lock', 'sysctl_lock', 'elv_list_lock',
                      'cpa_lock', 'pgd_lock', 'serio_event_lock',
                      'i8042_lock', 'lweventlist_lock', 'net_todo_list_lock',
                      'cdrom_lock', 'inet_proto_lock', 'inetsw_lock',
                      'ptype_lock', 'tcp_cong_list_lock', 'inet_diag_register_lock',
                      'cdev_lock', 'tlbstate_lock', 'redirect_lock',
                      'rt_flush_lock', 'task_capability_lock',
                      'rtc_task_lock', 'acpi_device_lock', 'acpi_prt_lock',

                      # STAMP
                      'globalLock',

                       #From here.. 2.4
                      'kernel_flag_cacheline',
                      'log_wait', # It is wait_queue_head_t whose first field is lock
	              'timerlist_lock',
		      'global_bh_lock',
	              'contig_page_data',
	              'lru_list_lock_cacheline',
	              'proc_alloc_map_lock',
		      'runqueue_lock',
                      'tasklist_lock',
                      'lastpid_lock',
                      'nl_table_wait',
                      'context_task_wq',
                      'kswapd_wait',
                      'swaplock',
                      'emergency_lock',
                      'bdflush_wait',
                      'kupdate_wait',
                      'nls_lock',
                      'kbd_controller_lock',
                      'ime_lock',
                      'io_request_lock',
                      'pagecache_lock_cacheline',
                      'pagemap_lru_lock_cacheline',
                      'unused_list_lock',
                      'tqueue_lock',
                      'page_uptodate_lock.0',
                      'buffer_wait',
                      'random_write_wait',
                      'random_read_wait',
                      'context_task_done',
                      'journal_datalist_lock',
                      'arbitration_lock',
                      'tty_ldisc_wait',
                      'kmap_lock_cacheline',
                      'modlist_lock',
                      'shmem_ilock',
                      'semaphore_lock',
                      'jh_splice_lock',
                      'hash_wait',
                      'ip_lock',

                     
                      ]
    for name in spinlock_names :
        if nm_sym.has_key(name):
            print >>ostr, '0x%x 4 1 %s' % (nm_sym[name], name)



#############################################################
# Main program starts here
def usage() :
    print sys.argv[0] + \
''': [-v verbose output (false)]
                   [-h this help message]'''
###################################################################
## Main program starts here
## Get options
try :
    opts, args = getopt.getopt(sys.argv[1:],'hvx:')
except getopt.GetoptError :
    usage()
    sys.exit(2)
opt_verbose = False
vmlinux = 'vmlinux'
for o, a in opts :
    if o == '-v' :
        opt_verbose = True
    if o == '-h' :
        usage()
        sys.exit(2)
    if o == '-x' :
        vmlinux = a

# Please change these to the local location in your file system.
outf = 'sync_char.map'
vstr = ''

# Figure out if we are dealing with linux or not
vmlinux_re = re.compile('''vmlinux''')
m = vmlinux_re.match(vmlinux)
if m :

    version_re = re.compile('''.*vmlinux-(?P<version>.*)''')
    m = version_re.match(vmlinux)
    if m :
        vstr = m.group('version')
    else :
        version_file = open('.kernelrelease', 'r')
        vstr = version_file.readline()
        version_file.close()
        vstr = vstr.rstrip()

    init1 = '../sws/%s.%s' % (outf, vstr)
else :
    init1 = '%s.%s' % (outf, vmlinux)


if not os.access(init1, os.W_OK) and os.access(init1, os.F_OK) :
    raise AssertionError, 'Cannot write %s\nPlease change this path in %s'%\
          (init1, sys.argv[0])


#############################################################
## Open files
ostr = open(outf, 'w')
estr = sys.stderr

scan_obj = SCAN_OBJ()
scan_obj.scan(os.popen('%s %s' % (objdump, vmlinux)), ostr)

# Now print certain static lock addresses if they are present
get_static_locks(ostr, nm, vmlinux)

ostr.close() # Flush those lines

shutil.copy(outf, init1)




##################################################################
## Notes on the syntax of objdump
    # Most of the time objdump prints something like this
#down():
#include/asm/semaphore.h:100
#c0259a2b:       f0 ff 4e 54             lock decl 0x54(%esi)
#c0259a2f:       0f 88 cd 01 00 00       js     c0259c02 <.text.lock.tty_ioctl+0x1a>
    # But somtimes it looks like this (file:line is optional),
    # this is in tty_ioctl.c:100
#down():
#c025952e:       f0 ff 4d 54             lock decl 0x54(%ebp)
#c0259532:       0f 88 b0 06 00 00       js     c0259be8 <.text.lock.tty_ioctl>
    # For some functions, objdump prints multiple function identifiers
    # with different line numbers.  If the flmatch is '', take
    # anything, otherwise match that reg exp on the line after the
    # function name appears.  Use flmatch for INLINED FUNCTIONS ONLY.
    # If the function is not inlined flmatch should either be '', or a
    # flag value like 'ignore_call'
    # Sometimes the file_line appears by itself, and it is wrong.  This is not an "up"
#include/asm/semaphore.h:174
#c0106f4c:       31 d2                   xor    %edx,%edx
#arch/i386/kernel/ldt.c:107
    # Sometimes the func_name is wrong.  This is not an "up".  I think
    # the function.
    # this happens when the up/down is the last call before the end of
#up():
#fs/lockd/svclock.c:375
#c01e3a5e:       b8 00 00 00 02          mov    $0x2000000,%eax
#nlmsvc_lock():
#fs/lockd/svclock.c:376
    # Objdump does not mark "up" consistently at its start
    # instruction, which is "lock incl ".  Sometimes there are
    # instructions before the "lock incl " instruction.
    # This is a problem.  Is it really a read_unlock?  There is no locked increment 
#    __raw_read_unlock():
#include/asm/spinlock.h:181
#c0116242:	c7 44 24 08 00 00 00 	movl   $0x0,0x8(%esp)
#c0116249:	00 
#current_thread_info():
#include/asm/thread_info.h:91
#c011624a:	be 00 e0 ff ff       	mov    $0xffffe000,%esi

# Ugh, sometimes the function name does not appear.
#c015bba3:	75 f6                	jne    c015bb9b <register_binfmt+0x32>
#include/asm/spinlock.h:186
#c015bba5:	f0 81 05 c4 95 2e c0 	lock addl $0x1000000,0xc02e95c4
#c015bbac:	00 00 00 01 
#c015bbb0:	b8 f0 ff ff ff       	mov    $0xfffffff0,%eax
#fs/exec.c:90

# Hell and tarnation.  I have to change the down detection algorithm
# to deal with this. 
#down():
#include/asm/semaphore.h:100
#c02138ca:	f0 ff 8a d4 00 00 00 	lock decl 0xd4(%edx)
#c02138d1:	0f 88 33 01 00 00    	js     c0213a0a <.text.lock.dd+0x1c>
#drivers/base/dd.c:167
#c02138d7:	f0 ff 8b d4 00 00 00 	lock decl 0xd4(%ebx)
#c02138de:	0f 88 36 01 00 00    	js     c0213a1a <.text.lock.dd+0x2c>

# Watch out for the back to back inlined functions.  'inlined' overlaps 'nexti'
#__raw_spin_unlock():
#include/asm/spinlock.h:91
#c011752b:	c6 80 04 05 00 00 01 	movb   $0x1,0x504(%eax)
#__raw_write_unlock():
#include/asm/spinlock.h:186
#c0117532:	f0 81 05 80 18 32 c0 	lock addl $0x1000000,0xc0321880
#c0117539:	00 00 00 01

# There can be multiple inlined funcs without the inlined name
#d_validate():
#include/asm/spinlock.h:91
#c01697a8:	c6 05 00 92 32 c0 01 	movb   $0x1,0xc0329200
#c01697af:	b8 01 00 00 00       	mov    $0x1,%eax
#fs/dcache.c:1283
#c01697b4:	5b                   	pop    %ebx
#c01697b5:	5e                   	pop    %esi
#c01697b6:	c3                   	ret    
#include/asm/spinlock.h:91
#c01697b7:	c6 05 00 92 32 c0 01 	movb   $0x1,0xc0329200
#c01697be:	31 c0                	xor    %eax,%eax

# Sometimes there are multiple file name/lineno records, but the second one is bogus
# fs/ext3/acl.c:149
# c019864d:	89 3b                	mov    %edi,(%ebx)
# include/asm/spinlock.h:91
# c019864f:	c6 46 70 01          	movb   $0x1,0x70(%esi)
# c0198653:	31 db                	xor    %ebx,%ebx
# c0198655:	e9 d5 fe ff ff       	jmp    c019852f <ext3_set_acl+0x34>
# fs/ext3/acl.c:254
# c019865a:	3d 00 40 00 00       	cmp    $0x4000,%eax
# c019865f:	0f 84 d6 00 00 00    	je     c019873b <ext3_set_acl+0x240>
# fs/ext3/acl.c:255
# c0198665:	bb f3 ff ff ff       	mov    $0xfffffff3,%ebx
# c019866a:	85 ff                	test   %edi,%edi
# c019866c:	0f 85 bd fe ff ff    	jne    c019852f <ext3_set_acl+0x34>
# include/asm/spinlock.h:91
# c0198672:	31 db                	xor    %ebx,%ebx
# c0198674:	e9 b6 fe ff ff       	jmp    c019852f <ext3_set_acl+0x34>
# ext3_acl_to_disk():

# Can't be too lazy about allowing func_name & file_line persist
# The second movb is not a spin_unlock
#__raw_spin_unlock():
#include/asm/spinlock.h:91
#c014ca2f:	c6 47 18 01          	movb   $0x1,0x18(%edi)
#mm/shmem.c:370
#c014ca33:	8b 54 24 10          	mov    0x10(%esp),%edx
#c014ca37:	c6 02 01             	movb   $0x1,(%edx)
