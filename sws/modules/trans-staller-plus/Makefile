#                                                              -*- Makefile -*-
# Copyright (c) Virtutech AB, All Rights Reserved
#
# Simics module makefile
#

MODULE_CLASSES = trans-staller-plus

SRC_FILES = staller.cc \
	    stconfig.cc \
            ststats.cc \
	    stdatapoint.cc \


MODULE_CFLAGS = -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_USE_SIMICS -g -O2
MODULE_CCFLAGS = -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -g -O2

include $(MODULE_MAKEFILE)
