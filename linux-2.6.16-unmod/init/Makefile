#
# Makefile for the linux kernel.
#

obj-y				:= main.o version.o mounts.o initramfs.o
obj-$(CONFIG_GENERIC_CALIBRATE_DELAY) += calibrate.o
obj-y += pcount.o
obj-y += pkmllc.o
obj-y += prbtree.o
obj-y += pprob.o
obj-y += prw.o
obj-y += pipebm.o
obj-y += listbm.o

mounts-y			:= do_mounts.o
mounts-$(CONFIG_DEVFS_FS)	+= do_mounts_devfs.o
mounts-$(CONFIG_BLK_DEV_RAM)	+= do_mounts_rd.o
mounts-$(CONFIG_BLK_DEV_INITRD)	+= do_mounts_initrd.o
mounts-$(CONFIG_BLK_DEV_MD)	+= do_mounts_md.o

# files to be removed upon make clean
clean-files := ../include/linux/compile.h

# dependencies on generated files need to be listed explicitly

$(obj)/version.o: include/linux/compile.h

# compile.h changes depending on hostname, generation number, etc,
# so we regenerate it always.
# mkcompile_h will make sure to only update the
# actual file if its content has changed.

include/linux/compile.h: FORCE
	@echo '  CHK     $@'
	$(Q)$(CONFIG_SHELL) $(srctree)/scripts/mkcompile_h $@ \
	"$(UTS_MACHINE)" "$(CONFIG_SMP)" "$(CONFIG_PREEMPT)" "$(CC) $(CFLAGS)"
