# MetaTM Project
# File Name: Makefile
#
# Description: 
#
# Operating Systems & Architecture Group
# University of Texas at Austin - Department of Computer Sciences
# Copyright 2007. All Rights Reserved.
# See LICENSE file for license terms.

MODULE_CLASSES=common

SRC_FILES = common.cc common_simics.cc\
			memaccess.cc osacache.cc \
			osacommon.cc os.cc MachineInfo.cc \
			osaassert.cc allochist.cc profile.cc osacachetrace.cc 

MODULE_CFLAGS = -D_USE_SIMICS -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -g -O2

EXTRA_VPATH=

include $(MODULE_MAKEFILE)
