# ==============================================================================
#
# Defines.common.mk
#
# ==============================================================================


PROG := bayes

SRCS += \
	adtree.c \
	bayes.c \
	data.c \
	learner.c \
	net.c \
	sort.c \
	$(LIB)/memory.c \
	$(LIB)/bitmap.c \
	$(LIB)/list.c \
	$(LIB)/mt19937ar.c \
	$(LIB)/queue.c \
	$(LIB)/random.c \
	$(LIB)/thread.c \
	$(LIB)/vector.c \
	$(LIB)/tm.c \
#
OBJS := ${SRCS:.c=.o}
DOTIS := ${SRCS:.c=.i}

CFLAGS += -DLIST_NO_DUPLICATES
CFLAGS += -DLEARNER_TRY_REMOVE
CFLAGS += -DLEARNER_TRY_REVERSE


# ==============================================================================
#
# End of Defines.common.mk
#
# ==============================================================================
