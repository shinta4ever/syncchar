DIRS := bayes genome intruder kmeans labyrinth ssca2 vacation yada empty
VARIANTS := metatm lock ordertm ordertm_lock stm 
PREFIX := .
ABS_PREFIX := $(abspath $(PREFIX))

export ABS_PREFIX

all: $(DIRS)
clean: $(addsuffix _clean,$(DIRS))
install: $(addsuffix _install,$(DIRS))

# Make tl2 be compiled for stm and ordertm targets
tl2:
	$(MAKE) -C tl2-x86-0.9.6
tl2_clean:
	$(MAKE) -C tl2-x86-0.9.6 clean

# Compile, install, and clean targets for each dir and variant
%_compile: tl2
	$(MAKE) -C lib clean
	$(MAKE) -C $(basename $*) -f Makefile$(suffix $*)

%_do_clean: tl2_clean
	$(MAKE) -C $(basename $*) -f Makefile$(suffix $*) clean

%_do_install:
	$(MAKE) -C $(basename $*) -f Makefile$(suffix $*) install

# Targets for compile/install/clean all variants
$(DIRS): %: $(addsuffix _compile,$(addprefix %.,$(VARIANTS)))
$(addsuffix _clean,$(DIRS)): %_clean: $(addsuffix _do_clean,$(addprefix %.,$(VARIANTS)))
$(addsuffix _install,$(DIRS)): %_install: $(addsuffix _do_install,$(addprefix %.,$(VARIANTS)))

src_install:
	for i in $(DIRS); do \
		mkdir -p $(ABS_PREFIX)/$$i; \
		cp $$i/* $(ABS_PREFIX)/$$i/; \
	done;
	cp * $(ABS_PREFIX); \
	cp -r tl2-x86-0.9.6 common lib $(ABS_PREFIX); \
