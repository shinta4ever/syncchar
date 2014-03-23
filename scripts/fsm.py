
"""
Finite state machine class.

The fsm class stores dictionary of state/input keys, values are
next state and action

when searching for matching state/input key, exact match
is checked first, then the input is matched against any regular
expressions associated with the state. As a last resort, state/None
can be used as a default for that state.

action is a function to execute which takes the current state
and input as arguments.  For regex, it also takes the match
object.  Note, this is not in the example below.

Exported symbols:

    class FSM
    string FSMError - raised when an execution error occurs
    int FSMEOF - used as a special input by users to signal
    	termination of the machine

"""

class RestartException(Exception) :
    def __init__(self, value) :
        self.restartto = value
    def __str__(self):
        return repr(self.restartto)

FSMError = 'Invalid input to finite state machine'
FSMEOF = -1

import re
_rgxt = type(re.compile('foo'))
del re

class FSM:
    """
    Finite State Machine
    simple example:

	def do_faq(state, input): send('faqfile')
	def do_help(state, input): send('helpfile')
	def cleanup(state, input): pass

    	fsm = FSM()
	fsm.add('start', re.compile('help', re.I),
		'start', do_help)
	fsm.add('start', 'faq', 'start', do_faq)
	# matches anything, does nothing
	fsm.add('start', None, 'start')
	fsm.add('start', FSMEOF, 'done', cleanup)
	...

	fsm.start('start')
	for line in map(string.strip, sys.stdin.readlines()):
	    try:
		fsm.execute(line)
	    except FSMError:
		sys.stderr.write('Invalid input: %s\n' % `line`)
	fsm.execute(FSMEOF)
    """

    def __init__(self):
	self.states = {}
	self.state = None
	self.dbg = None

    # add another state to the fsm
    def add(self, state, input, newstate, action=None):
	"""add a new state to the state machine"""
	try:
	    self.states[state][input] = (newstate, action)
	except KeyError:
	    self.states[state] = {}
	    self.states[state][input] = (newstate, action)

    def del_state(self, state) :
        self.states[state] = {}

    # perform a state transition and action execution
    def execute(self, input):
	"""execute the action for the current (state,input) pair"""

	if not self.states.has_key(self.state):
	    raise FSMError

	state = self.states[self.state]
	# exact state match?
	if state.has_key(input):
	    newstate, action = state[input]
	    if action is not None:
		try:
		    apply(action, (self.state, input, None))
		except RestartException, (instance):
		    # action routine can raise RestartException to force
		    # jump to a different state - usually back to start
		    # if input didn't look like it was supposed to
		    self.state = instance.restartto
		    return
	    self.state = newstate
	    return

	# no, how about a regex match? (first match wins)
	else:
	    for s in state.keys():
		if type(s) == _rgxt:
		    m = s.match(input)
		    if m :
		    	newstate, action = state[s]
		    	if action is not None:
				try:
			    		apply(action, (self.state, input, m))
				except RestartException, (instance):
			    	# action routine can raise RestartException to force
			    	# jump to a different state - usually back to start
			   	# if input didn't look like it was supposed to
			    		self.state = instance.restartto
			    		return
				except:
			    		print (action, (self.state, input))
			    		raise
		    		self.state = newstate
		    		return
			
	    if state.has_key(None):
		newstate, action = state[None]
		if action is not None:
		    try:
			apply(action, (self.state, input, None))
		    except RestartException, (restartto):
			# action routine can raise RestartException to force
			# jump to a different state - usually back to start
			# if input didn't look like it was supposed to
			self.state = instance.restartto
			return
		self.state = newstate
		return

	    # oh well, bombed out...
	    else:
		raise FSMError

    # define the start state
    def start(self, state):
	"""set the start state"""
	self.state = state

    # assign a writable file to catch debugging transitions
    def debug(self, out=None):
	"""assign a debugging file handle"""
	self.dbg = out
