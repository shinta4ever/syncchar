# SyncChar Project
# File Name: Makefile
#
# Description: 
#
# Operating Systems & Architecture Group
# University of Texas at Austin - Department of Computer Sciences
# Copyright 2006, 2007. All Rights Reserved.
# See LICENSE file for license terms.

MODULE_CLASSES=sync_char

SRC_FILES = sync_char.cc WorkSet.cc \
		../common/memaccess.cc ../common/osacache.cc \
		../common/osacommon.cc ../common/os.cc ../common/MachineInfo.cc \
		../common/osaassert.cc

MODULE_CFLAGS = -D_USE_SIMICS -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -g -O2

EXTRA_VPATH=

include $(MODULE_MAKEFILE)
