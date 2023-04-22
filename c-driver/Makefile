#
# qc-usb, Logitech QuickCam video driver with V4L support
# Derived from qce-ga, linux V4L driver for the QuickCam Express and Dexxa QuickCam
#
# Makefile - driver build system
#
# Copyright (C) 2003  Tuukka Toivonen
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
#

# Requires GNU Make

ifeq ($(KERNELRELEASE),)	# Test whether we were called by 2.6.x build system
# Called by user

# Get current directory
PWD := $(shell pwd)

# Get PREFIX (where to install userspace utilities)
ifndef PREFIX
PREFIX := /usr/local
endif

# Get MODULE_DIR (where to install kernel module)
ifndef MODULE_DIR
ifndef LINUX_DIR
MODULE_DIR := /lib/modules/$(shell uname -r)
else
# Note: the brackets on line below contain a space and a _tab_!
#RELEASE := $(shell awk -F \" '/[ 	]*\#[ 	]*define[ 	]*UTS_RELEASE[ 	]*/ { print $$2 }' $(LINUX_DIR)/include/linux/version.h|tail -n 1)
# This seem to work better
#LINUX_DIR := /usr/src/linux
RELEASE := $(shell if test -r $(LINUX_DIR)/include/linux/version.h && fgrep -q UTS_RELEASE $(LINUX_DIR)/include/linux/version.h; then \
		((echo "\#include <linux/version.h>" ; echo "kernsrcver=UTS_RELEASE") | cpp -I $(LINUX_DIR)/include | grep "^kernsrcver=" | cut -d \" -f 2); \
	elif test -r $(LINUX_DIR)/include/linux/utsrelease.h && fgrep -q UTS_RELEASE $(LINUX_DIR)/include/linux/utsrelease.h; then \
		((echo "\#include <linux/utsrelease.h>"; echo "kernsrcver=UTS_RELEASE") | cpp -I $(LINUX_DIR)/include | grep "^kernsrcver=" | cut -d \" -f 2); \
	fi)
MODULE_DIR := /lib/modules/$(RELEASE)
endif
endif

# Get LINUX_DIR (where the kernel source is or at least headers)
ifndef LINUX_DIR
LINUX_DIR := $(MODULE_DIR)/build
endif

HAVE_UTSRELEASE_H := $(shell if test -r $(LINUX_DIR)/include/linux/version.h && fgrep -q UTS_RELEASE $(LINUX_DIR)/include/linux/version.h; then \
		echo 0; \
	elif test -r $(LINUX_DIR)/include/linux/utsrelease.h && fgrep -q UTS_RELEASE $(LINUX_DIR)/include/linux/utsrelease.h; then \
		echo 1; \
	fi)

ifndef USER_OPT
USER_OPT := -DHAVE_UTSRELEASE_H=$(HAVE_UTSRELEASE_H)
else
USER_OPT := $(USER_OPT) -DHAVE_UTSRELEASE_H=$(HAVE_UTSRELEASE_H)
endif

ARCH    := $(shell uname -m | sed -e s/i.86/i386/ -e s/sun4u/sparc64/ -e s/arm.*/arm/ -e s/sa110/arm/)
INSTALL := $(shell which install)
DEPMOD  := $(shell which depmod || which /sbin/depmod || echo "true You should now run depmod")

# Get VERSION_CODE (from version.h in kernel source directory)
VERSION_CODE := $(shell awk '/[ 	]*\#[ 	]*define[ 	]*LINUX_VERSION_CODE[ 	]*/ { print $$3 }' $(LINUX_DIR)/include/linux/version.h|tail -n 1)

ifeq ($(shell if [ $(VERSION_CODE) -ge 132608 ]; then echo y; fi),y)
MODULE_NAME := quickcam.ko	# for 2.6.x
else
MODULE_NAME := quickcam.o	# for 2.4.x and older
endif

.PHONY: help
help:
	@echo '-=- Logitech QuickCam USB camera driver -=-'
	@echo
	@echo "Makefile target examples:"
	@echo "make all - Compile driver and utilities against current running kernel"
	@echo "make all USER_OPT=-DDEBUG - Compile with debugging code and messages"
	@echo "make all LINUX_DIR=/usr/src/linux - Compile against specified kernel source"
	@echo "make install - Copy driver and utilities into standard locations (needs root)"
	@echo "make install PREFIX=/usr - Copy utilities to /usr/bin instead of /usr/local/bin"
	@echo "make install MODULE_DIR=/lib/modules/2.4.0 - Copy module to /lib/modules/2.4.0/misc"
ifeq ($(VERSION_CODE),132116)
	@echo "make patch - Modify kernel source to include the driver for kernel 2.4.20"
endif
ifeq ($(VERSION_CODE),132615)
	@echo "make patch - Modify kernel source to include the driver for kernel 2.6.7"
endif
ifeq ($(VERSION_CODE),132616)
	@echo "make patch - Modify kernel source to include the driver for kernel 2.6.8/2.6.8.1"
endif
	@echo "make clean - Remove object files from the source directory"
	@echo
	@echo "Current configuration:"
	@echo "Driver source directory (PWD):         $(PWD)"
	@echo "Kernel source directory (LINUX_DIR):   $(LINUX_DIR)"
	@echo "Module install directory (MODULE_DIR): $(MODULE_DIR)"
	@echo "Utility install directory (PREFIX):    $(PREFIX)"
	@echo "User options (USER_OPT):               $(USER_OPT)"
	@echo "Driver file name (use with insmod):    $(MODULE_NAME)"
	@echo "Kernel version code:                   $(VERSION_CODE)"

all: $(MODULE_NAME) qcset

clean:
	rm -f *.o qcset show *~ .\#* .*.cmd *.mod.c *.ko
	rm -rf .tmp_versions

# Is it 2.6.0 or newer?
ifeq ($(shell if [ $(VERSION_CODE) -ge 132608 ]; then echo y; fi),y)
# Yes, 2.6.0 or newer

quickcam.ko: qc-driver.c qc-formats.c qc-mjpeg.c qc-hdcs.c qc-pb0100.c qc-vv6410.c qc-memory.c qc-memory.h quickcam.h
	+make -C "$(LINUX_DIR)" SUBDIRS="$(PWD)" modules V=1 USER_OPT="$(USER_OPT)"

else
# No, 2.4.x or older

MODULE_INC    := -I$(LINUX_DIR)/include -nostdinc -iwithprefix include
MODULE_DEF    := -DMODULE -D__KERNEL__ -DNOKERNEL
MODULE_OPT    := -O2 -fomit-frame-pointer -fno-strict-aliasing -fno-common
MODULE_WARN   := -Wall -Wstrict-prototypes -Wno-trigraphs
EXTRA_CFLAGS  := $(MODULE_INC) $(MODULE_DEF) $(MODULE_OPT) $(MODULE_WARN) $(USER_OPT) -pipe

# x86 build fails if these are included, alpha build fails if they are not.
# For other architectures, they might or might not be needed.
# By not including Makefile, kernel headers are sufficient for compilation.
# Otherwise, full kernel source package is required to be installed.
ifeq ($(ARCH),alpha)
include $(LINUX_DIR)/Rules.make
include $(LINUX_DIR)/arch/$(ARCH)/Makefile
endif
ifeq ($(ARCH),sparc)
include $(LINUX_DIR)/Rules.make
include $(LINUX_DIR)/arch/$(ARCH)/Makefile
endif
ifeq ($(ARCH),sparc64)
include $(LINUX_DIR)/Rules.make
include $(LINUX_DIR)/arch/$(ARCH)/Makefile
endif

ifndef LD
LD := ld
endif
ifndef CC
CC := gcc
endif

quickcam.o: qc-driver.o qc-hdcs.o qc-pb0100.o qc-vv6410.o qc-formats.o qc-mjpeg.o qc-memory.o
	$(LD) -r -o quickcam.o qc-driver.o qc-hdcs.o qc-pb0100.o qc-vv6410.o qc-formats.o qc-mjpeg.o qc-memory.o

qc-driver.o: qc-driver.c quickcam.h qc-memory.h
qc-formats.o: qc-formats.c quickcam.h
qc-mjpeg.o: qc-mjpeg.c quickcam.h
qc-hdcs.o: qc-hdcs.c quickcam.h
qc-pb0100.o: qc-pb0100.c quickcam.h
qc-vv6410.o: qc-vv6410.c quickcam.h
qc-memory.o: qc-memory.c

.c.o:
	$(CC) $(EXTRA_CFLAGS) -c $<

endif # for kernel version 2.6.x test

.PHONY: install
install: $(MODULE_NAME)
	$(INSTALL) -c -D -m 644 $(MODULE_NAME) $(MODULE_DIR)/misc/$(MODULE_NAME)
	$(INSTALL) -c -D -m 755 qcset $(PREFIX)/bin/qcset
	-$(DEPMOD) -a

qcset: qcset.c quickcam.h
	gcc -Wall -O2 -s qcset.c -o qcset -lm

.PHONY: patch
patch: kernel-patch kernel-files

ifeq ($(VERSION_CODE),132116)		# Kernel 2.4.20
KERNEL_PATCHNAME := linux-2.4.20-quickcam.patch
KERNEL_VIDEODIR  := drivers/usb
endif
ifeq ($(VERSION_CODE),132615)		# Kernel 2.6.7
KERNEL_PATCHNAME := linux-2.6.7-quickcam.patch
KERNEL_VIDEODIR  := drivers/usb/media
endif
ifeq ($(VERSION_CODE),132616)		# Kernel 2.6.8 / 2.6.8.1
KERNEL_PATCHNAME := linux-2.6.8.1-quickcam.patch
KERNEL_VIDEODIR  := drivers/usb/media
endif

.PHONY: kernel-patch
kernel-patch:
	(cd $(LINUX_DIR) && patch -s -p1) < $(KERNEL_PATCHNAME)

.PHONY: kernel-files
kernel-files:
	$(INSTALL) -c -D -m 644 quickcam.h   $(LINUX_DIR)/include/linux/quickcam.h
	$(INSTALL) -c -D -m 644 qc-driver.c  $(LINUX_DIR)/$(KERNEL_VIDEODIR)/qc-driver.c
	$(INSTALL) -c -D -m 644 qc-formats.c $(LINUX_DIR)/$(KERNEL_VIDEODIR)/qc-formats.c
	$(INSTALL) -c -D -m 644 qc-mjpeg.c   $(LINUX_DIR)/$(KERNEL_VIDEODIR)/qc-mjpeg.c
	$(INSTALL) -c -D -m 644 qc-memory.c  $(LINUX_DIR)/$(KERNEL_VIDEODIR)/qc-memory.c
	$(INSTALL) -c -D -m 644 qc-memory.h  $(LINUX_DIR)/$(KERNEL_VIDEODIR)/qc-memory.h
	$(INSTALL) -c -D -m 644 qc-vv6410.c  $(LINUX_DIR)/$(KERNEL_VIDEODIR)/qc-vv6410.c
	$(INSTALL) -c -D -m 644 qc-hdcs.c    $(LINUX_DIR)/$(KERNEL_VIDEODIR)/qc-hdcs.c
	$(INSTALL) -c -D -m 644 qc-pb0100.c  $(LINUX_DIR)/$(KERNEL_VIDEODIR)/qc-pb0100.c

# Some test programs below, not necessary

show: show.o 
	gcc -g show.o -o show -L/usr/X11R6/lib -lX11 -lXext

show.o: show.c
	gcc -O9 -fomit-frame-pointer -mcpu=pentiumpro -march=pentiumpro -g -Wall -c show.c

else
# Called recursively by 2.6.x kernel build process

EXTRA_CFLAGS += -DNOKERNEL $(USER_OPT)

obj-m := quickcam.o
quickcam-objs := qc-driver.o qc-hdcs.o qc-pb0100.o qc-vv6410.o qc-formats.o qc-mjpeg.o qc-memory.o

endif
