#!/bin/sh

# {{{ [fold] Copyright blaablaas
#
# qc-usb, Logitech QuickCam video driver with V4L support
# Derived from qce-ga, linux V4L driver for the QuickCam Express and Dexxa QuickCam
#
# quickcam.sh - Automated driver build and installation system
#
# Copyright (C) 2003-2004  Tuukka Toivonen
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
# }}}

# {{{ [fold] Helper functions
KERNEL26X=132608

askreturn() {
	echo -n "Press Ctrl+C to quit, Enter to continue ---> "
	read x
	echo ""
}

askreturnfail() {
	echo "WARNING: If you press Enter, I'll try to continue anyway,"
	echo "but this probably will fail. You SHOULD press Ctrl+C now."
	askreturn
}

checkprogs() {
	ret=0
	while [ "$1" != "" ]; do
		if which "$1"; then
			true
		else
			echo "Warning: $1 missing"
			ret=1
		fi
		shift
	done
	return $ret
}

findprog() {
	P="`which "$1" || which "/sbin/$1" || which "/usr/sbin/$1" || which "/usr/local/bin/$1" || which "/usr/local/sbin/$1"`"
	if [ $? != 0 ]; then
		echo "[!] Couldn't find program $1"
		askreturnfail
	else
		echo "Found program $P"
	fi
}

qcrealpath() {
	P="$1"
	while [ -L "$P" ]; do
		P="`readlink -f -n "$P"`"
	done
	if [ ! -e "$P" ]; then
		return 1
	fi
	echo "$P"
	return 0
}

configurekernel() {
	if [ "$KERN_PATCHLEVEL" -ge "6" ]; then
		(cd "$LINUX_DIR" && make oldconfig && make modules_prepare)
	elif [ "$KERN_PATCHLEVEL" -le "4" ]; then
		(cd "$LINUX_DIR" && make oldconfig && make dep)
	else
		echo "[!] Unknown kernel patchlevel $KERN_PATCHLEVEL"
		return 1
	fi
	if [ "$?" != "0" ]; then
		echo "[!] Kernel configuration failed, see messages above."
		askreturnfail
		return 1
	fi
	return 0
}
# }}}
# {{{ [fold] Special root code
if  [ "$QCINSTCMD" != "" ]; then
	if [ "`whoami`" != "root" ]; then
		echo "[!] Root mode failed, bug in the script?"
		askreturnfail
		exit 1
	fi
	echo "=== Entering root mode ==="
	R=0
	if [ "$QCINSTCMD" = "conf" ]; then
		echo "Trying to configure kernel source as root..."
		configurekernel
		R="$?"
	elif [ "$QCINSTCMD" = "mod" ]; then
		# Load necessary modules
		echo "Now you will see some error messages."
		echo "They are probably harmless and you can ignore them"
		echo "(until leaving root mode)."
		$MODPROBE usbcore
		$MODPROBE usb-uhci || $MODPROBE uhci || $MODPROBE uhci_hcd
		$MODPROBE usb-ohci || $MODPROBE ohci_hcd
		$MODPROBE hc_sl811
		$MODPROBE videodev
		$MOUNT none /proc/bus/usb -t usbdevfs
	elif [ "$QCINSTCMD" = "load" ]; then
		# Load qc-usb driver
		if [ -w /proc/sys/kernel/sysrq ]; then
			echo ""
			echo "I will now try to enable the SysRq key."
			echo "If your computer crashes, you can try pressing:"
			echo "	Alt + SysRq + S: Emergency Sync (write everything on hard disk)"
			echo "	Alt + SysRq + U: Unmount all harddisks"
			echo "	Alt + SysRq + B: Reboot system immediately"
			askreturn
			echo "1" >/proc/sys/kernel/sysrq
		fi
		echo "Now I finally will try to load the module."
		echo "If you're unlucky, your computer might crash right now!!!!"
		echo "Consider long if you really want to continue."
		askreturn
		echo "You decided to do it, here we go..."
		dmesg -c >/dev/null
		$INSMOD "./$MODULE_NAME"
	elif [ "$QCINSTCMD" = "unload" ]; then
		echo "Trying to unload QuickCam driver..."
		$RMMOD quickcam || $RMMOD mod_quickcam
	elif [ "$QCINSTCMD" = "inst" ]; then
		# Install driver and utilities
		make install
	fi
	echo "=== Leaving root mode ==="
	exit $R
fi
# }}}
# {{{ [fold] Greet messages
echo "-=- Logitech QuickCam USB camera driver installer -=-"
echo "Hello! I am the (hopefully) easy-to-use, fully automated"
echo "qc-usb driver installation script."
echo "At the moment, this is experimental, and if it doesn't work,"
echo "don't hesitate to quit this with Ctrl+C and install the"
echo "driver manually."
echo ""
echo "The driver is provided in source code form, so it has to be"
echo "compiled. This should happen automatically, but it does mean"
echo "that there are some steps required before installation."
echo ""
echo "You also need to know \"root\" user password to test and"
echo "install the driver."
echo ""
echo "Basically you need only to keep hitting Enter whenever you"
echo "see this prompt: --->. Sometimes you're asked root password."
echo "Pay special attention to lines beginning with [!]."
echo "It means that some trouble has been detected."
echo ""
echo "To most important location is the path to your kernel source"
echo "or headers. This can be guessed, but you can specify it by"
echo "giving it as an argument to this script like this:"
echo "	./quickcam.sh LINUX_DIR=/usr/src/linux"
echo ""
#echo "Other possible parameters, for which the default values should"
#echo "be good, are MODULE_DIR= for kernel module directory,"
#echo "PREFIX= for utility program directory, USER_OPT= for special"
#echo "driver parameters, and CC= for C compiler."
#echo ""
echo "If you haven't done it yet, now it would be a good moment to"
echo "take a look at file README."
# }}}
# {{{ [fold] Evaluate parameters
while [ "$1" != "" ]; do
	echo "Argument found: $1"
	eval "$1"
	shift
done
if [ "$LINUX_DIR" != "" ]; then
	export LINUX_DIR
fi
if [ "$MODULE_DIR" != "" ]; then
	export MODULE_DIR
fi
if [ "$PREFIX" != "" ]; then
	export PREFIX
fi
if [ "$USER_OPT" != "" ]; then
	export USER_OPT
fi
if [ "$CC" != "" ]; then
	export CC
fi
if [ "$LD" != "" ]; then
	export LD
fi
# }}}
# {{{ [fold] Test programs and versions
echo ""
echo "Next I'm going to check if you have some important programs installed"
echo "and if they and the kernel are of suitable version."
askreturn
if [ "$CC" != "" ]; then
	echo "Using specified C compiler from environment CC=$CC"
else
	CC=gcc; export CC
fi
if [ "$LD" != "" ]; then
	echo "Using specified linker from environment ld=$LD"
else
	LD=ld; export LD
fi
checkprogs "$0" whoami su ls cat "$CC" gcc make grep egrep awk sed tail head install ld uname tr xawtv xdpyinfo dmesg wc
if [ $? != 0 ]; then
	echo "[!] Some important programs can not be found on default path."
	echo "Probably they aren't installed."
	echo "You should install them, for example, by using apt-get or rpm."
	askreturnfail
fi

REALPATH="`which realpath || which /usr/local/bin/realpath`"
if [ $? != 0 ]; then
	REALPATH="qcrealpath"
	which readlink
	if [ $? != 0 ]; then
		echo "[!] Can not find either program readlink nor realpath."
		echo "Either one is required for running this script."
		askreturnfail
	fi
fi

CCVER="`$CC -v 2>&1|grep -i version`"
echo "$CC version: $CCVER"
GCCVER="`gcc -v 2>&1|grep -i version`"
echo "gcc version: $GCCVER"
MKVER="`make -v 2>&1|grep -i make|head -n 1`"
echo "Make version: $MKVER"
LDVER="`ld -v 2>&1|grep -i ld`"
echo "Linker version: $LDVER"

echo "$MKVER" | grep GNU >/dev/null
if [ $? != 0 ]; then
	echo "[!] Make doesn't appear to be GNU Make."
	askreturnfail
fi

OSNAME="`uname -s`"
if [ "$OSNAME" != "Linux" ]; then
	echo "[!] You don't appear to have Linux, but $OSNAME."
	echo "Other kernels are not supported."
	askreturnfail
fi

if [ ! -r /proc/version ]; then
	echo "[!] Virtual file /proc/version is missing."
	echo "Maybe procfs is not mounted? Please mount it."
	echo "If kernel is configured with procfs support, this can be achieved with"
	echo "	mount none /proc -t proc"
	askreturnfail
fi

OSCCVER="`cat /proc/version | sed 's,[^)(]*([^)(]*)[^)(]*(\(.*\))[^)(]*,\1,g'`"
echo "Kernel compiler: $OSCCVER"
if [ "$OSCCVER" != "$CCVER" ]; then
	echo "[!] Kernel compiler and $CC seem to be different versions."
	echo "Instead, they should be the same. If you have many compilers"
	echo "installed, you can specify the correct one with command (in bash)"
	echo "	export CC=kgcc"
	echo "before trying to install the driver. Replace kgcc with the command"
	echo "required for compiling kernels (kgcc is often used in Red Hat systems)."
	askreturnfail
fi

####

echo "Looking for more necessary programs..."
findprog depmod   && DEPMOD="$P"
findprog insmod   && INSMOD="$P"
findprog rmmod    && RMMOD="$P"
findprog modprobe && MODPROBE="$P"
findprog mount    && MOUNT="$P"
findprog lsusb    && LSUSB="$P"
export DEPMOD INSMOD RMMOD MODPROBE MOUNT
DEPMODVER=`$DEPMOD -V 2>&1 | egrep '(^depmod version |module-init-tools)'`
INSMODVER=`$INSMOD -V 2>&1 | egrep '(^insmod version |module-init-tools)'`
RMMODVER=`$RMMOD -V 2>&1 | egrep '(^rmmod version |module-init-tools)'`
MODPROBEVER=`$MODPROBE -V 2>&1 | egrep '(^modprobe version |module-init-tools)'`
if [ "$INSMODVER" = "" ]; then INSMODVER="module-init-tools - something"; fi
echo "depmod version: $DEPMODVER"
echo "insmod version: $INSMODVER"
echo "rmmod version: $RMMODVER"
echo "modprobe version: $MODPROBEVER"
# }}}
# {{{ [fold] Test if we're not root and in the correct directory
echo "Checking whether we're root... `whoami`"
if [ "`whoami`" = "root" ]; then
	echo "[!] Running script as root."
	echo "You shouldn't run this script as root. It should work,"
	echo "but is unsafe. Please run this as an ordinary user."
	echo "When root access is really needed, you will be prompted"
	echo "for the root password."
	askreturnfail
fi
echo "Checking for driver source code..."
if [ ! -r ./Makefile -o ! -r ./qc-driver.c ]; then
	echo "[!] Driver source not found."
	echo "The qc-usb driver source code must be in the current directory,"
	echo "but I didn't find it."
	askreturnfail
fi
echo "Checking for write permission..."
if [ ! -w . ]; then
	echo "[!] Current directory not writable"
	echo "You don't seem to have write permission to the current directory"
	echo "($PWD)"
	askreturnfail
fi
# }}}
# {{{ [fold] Check kernel source
echo ""
echo "Previous round done. Now checking if you have kernel source installed."
askreturn
KERNEL_SOURCE="`make | grep LINUX_DIR | tail -n 1 | awk -F : '{print $2}' | awk '{print $1}'`"
echo "Kernel source directory: $KERNEL_SOURCE"
if [ ! -r "$KERNEL_SOURCE/include/linux/videodev.h" ]; then
	echo "[!] Can not find kernel source or even headers."
	echo "Make sure that they are installed (install with e.g. rpm or apt-get"
	echo "if necessary) and ensure that you have read rights to the files."
	askreturnfail
fi
HAVEFULLSRC="y"
if [ ! -r "$KERNEL_SOURCE/Makefile" ]; then
	HAVEFULLSRC="n"
	echo "[!] Kernel headers installed, but not complete source code."
	echo "Installation still may work with some architectures like Intel x86,"
	echo "but will definitely fail on others. Note that for kernel 2.6.x"
	echo "we need always full source code."
	askreturn
else
	KERN_PATCHLEVEL=`awk -F = '/^PATCHLEVEL *=/ {print $2}' < "$KERNEL_SOURCE/Makefile"|tr -d ' 	'`
	KERN_VERSION=`awk -F = '/^VERSION *=/ {print $2}' < "$KERNEL_SOURCE/Makefile"|tr -d ' 	'`
	echo "Detected kernel version is $KERN_VERSION.$KERN_PATCHLEVEL.x."
	if [ "$KERN_VERSION" != "2" ]; then
		echo "[!] Kernel major version is not 2, which is completely something"
		echo "not expected. Either it was misdetected or you are using"
		echo "not supported kernel version."
		askreturnfail
	fi
fi
if [ ! -r "$KERNEL_SOURCE/.config" ]; then
	HAVEFULLSRC="n"
	echo "[!] Kernel configuration file $KERNEL_SOURCE/.config not found."
	echo "If the headers have been already configured properly, you might"
	echo "not need it. But it would be better not to trust this, you"
	echo "really should know where is your configuration file, and"
	echo "copy it into its place. Often it can be found in /boot/"
	echo "directory with a name like /boot/config-2.4.26 or something."
	askreturnfail
fi

KERNELOK="y"
if [ ! -r "$KERNEL_SOURCE/include/linux/version.h" ]; then
	echo "[!] Can not find version.h in kernel source."
	KERNELOK="n"
fi
if [ ! -r "$KERNEL_SOURCE/include/linux/autoconf.h" ]; then
	echo "[!] Kernel source is not configured properly."
	KERNELOK="n"
fi
if [ "$KERNELOK" != "y" ]; then
	if [ "$HAVEFULLSRC" == "y" ]; then
		echo "You have full kernel source code but it is not configured"
		echo "properly. You can configure it by entering the source"
		echo "directory and typing (with 2.2.x and 2.4.x kernel versions)"
		echo "	make oldconfig"
		echo "	make dep"
		echo "or (with 2.6.x kernel versions)"
		echo "	make oldconfig"
		echo "	make modules_prepare"
		echo "It is also good idea to first clean up completely the kernel"
		echo "source by typing \"make mrproper\", but this will also delete the"
		echo ".config file, which has to be copied somewhere else to keep safe."
		echo "I can also try to do this automatically, in which case"
		echo "just keep pressing enter, otherwise abort now."
		askreturn
		echo "You want me to configure the kernel automatically. All right."
		echo "If the kernel configuration file doesn't match exactly the"
		echo "kernel source, you will be asked some questions."
		echo "This is not a good sign, but if the difference is very small"
		echo "(maybe a few unimportant questions) it still might work."
		echo "Usually you can just select the default answer for the questions."
		askreturn
		if [ ! -w "$KERNEL_SOURCE" ]; then
			echo "You don't have write permission to the kernel source,"
			echo "so I must obtain root access to configure it. Type"
			echo "the root password now (Ctrl+D to cancel):"
			QCINSTCMD="conf" KERN_PATCHLEVEL="$KERN_PATCHLEVEL" LINUX_DIR="$LINUX_DIR" su -p -c "$0"
		else
			configurekernel
		fi
		KERNELOK="y"
		if [ ! -r "$KERNEL_SOURCE/include/linux/version.h" ]; then
			echo "[!] Can still not find version.h in kernel source."
			KERNELOK="n"
		fi
		if [ ! -r "$KERNEL_SOURCE/include/linux/autoconf.h" ]; then
			echo "[!] Kernel source is still not configured properly."
			KERNELOK="n"
		fi
		if [ "$KERNELOK" != "y" ]; then
			echo "[!] Kernel configuration failed."
			echo "Check if you got any special messages above."
			askreturnfail
		fi
	else
		echo "You have only kernel headers but they are not configured"
		echo "properly. It's pointless trying to continue, this won't work."
		echo "Either install properly configured kernel headers or full"
		echo "source with kernel configuration file. In the latter case"
		echo "I can configure the kernel source using the configuration"
		echo "file automatically."
		askreturnfail
	fi
fi

HAVE_UTSRELEASE_H=0
UTS_FILE=""
if test -r $KERNEL_SOURCE/include/linux/version.h && fgrep -q UTS_RELEASE $KERNEL_SOURCE/include/linux/version.h; then
	kernsrcver=`(echo "#include <linux/version.h>"; echo "kernsrcver=UTS_RELEASE") | cpp -I $KERNEL_SOURCE/include | grep "^kernsrcver=" | cut -d \" -f 2`
	UTS_FILE=$KERNEL_SOURCE/include/linux/version.h
elif test -r $KERNEL_SOURCE/include/linux/utsrelease.h && fgrep -q UTS_RELEASE $KERNEL_SOURCE/include/linux/utsrelease.h; then
	kernsrcver=`(echo "#include <linux/utsrelease.h>"; echo "kernsrcver=UTS_RELEASE") | cpp -I $KERNEL_SOURCE/include | grep "^kernsrcver=" | cut -d \" -f 2`
	HAVE_UTSRELEASE_H=1
	UTS_FILE=$KERNEL_SOURCE/include/linux/utsrelease.h
fi
if test -z "$kernsrcver"; then
    echo "Couldn't find KERNEL_UTS version"
    exit 1
fi
#	export USER_OPT="$USER_OPT -DHAVE_UTSRELEASE_H=$HAVE_UTSRELEASE_H"

KERNEL_VERSION="`make | grep 'Kernel version code' | tail -n 1 | awk -F : '{print $2}' | awk '{print $1}'`"
#KERNEL_UTS=`awk -F \" '/[ 	]*\#[ 	]*define[ 	]*UTS_RELEASE[ 	]*/ { print $2 }' "$KERNEL_SOURCE/include/linux/version.h"|tail -n 1`
#KERNEL_UTS="`make | grep 'Kernel version' | tail -n 1 | awk -F : '{print $2}' | awk '{print $1}'`"
KERNEL_UTS=$kernsrcver
MODULE_NAME="`make | grep 'Driver file name' | tail -n 1 | awk -F : '{print $2}' | awk '{print $1}'`"
INSTALL_DIR="`make | grep 'Module install directory' | tail -n 1 | awk -F : '{print $2}' | awk '{print $1}'`"
export MODULE_NAME

#UTS_COUNT=`grep UTS_RELEASE < "$KERNEL_SOURCE/include/linux/version.h" | wc -l`
UTS_COUNT=`grep UTS_RELEASE < "$UTS_FILE" | wc -l`

if [ $? != 0 ]; then UTS_COUNT=0; fi
if [ $UTS_COUNT -ne 1 ]; then
	echo "[!] Multiple kernel versions specified in linux/version.h"
	echo "This is probably a heavily modified Red Hat or other distributor"
	echo "kernel, and the kernel version check doesn't work."
	echo "So we can not check if your kernel version is correct, so we"
	echo "must just hope so."
	askreturn
fi
echo "Kernel version name: $KERNEL_UTS"
echo "Kernel source version code: $KERNEL_VERSION"
echo "Driver file name: $MODULE_NAME"
echo "Module install directory: $INSTALL_DIR"
if [ "$KERNEL_VERSION" -ge "$KERNEL26X" -a "$HAVEFULLSRC" != "y" ]; then
	echo "[!] Not complete kernel 2.6.x source found (possibly just headers)"
	echo "Remember that pure headers for 2.6.x kernel are not sufficient."
	askreturnfail
fi
# Hmm, it appears that we don't actually need write permission even if not using O= option
#if [ "$KERNEL_VERSION" -ge "$KERNEL26X" -a ! -w "$KERNEL_SOURCE/Makefile" ]; then
#	echo "[!] You have 2.6.x version kernel but not write permissions"
#	echo "to the source code. For 2.6.x the driver installation"
#	echo "requires write access to the kernel sources."
#	askreturnfail
#fi
if [ $UTS_COUNT = 1 -a "$KERNEL_UTS" != "`uname -r`" ]; then
	echo "[!] Kernel source version mismatch."
	echo "This script assumes that the running kernel should be same as"
	echo "the kernel sources against which the driver will be compiled,"
	echo "but they don't seem to be."
	echo "Kernel source is \"$KERNEL_UTS\" but running kernel is \"`uname -r`\"."
	echo "You may need to do \"make bzImage\" to correct this error."
	askreturnfail
fi
if [ "$KERNEL_VERSION" -lt "131602" ]; then
	echo "[!] You have older kernel than 2.2.18, it is not supported."
	askreturnfail
fi
make | grep ':' | tail -n 7

echo $DEPMODVER | grep module-init-tools >/dev/null
DEPMOD_MIT=$?
echo $INSMODVER | grep module-init-tools >/dev/null
INSMOD_MIT=$?
echo $RMMODVER | grep module-init-tools >/dev/null
RMMOD_MIT=$?
echo $MODPROBEVER | grep module-init-tools >/dev/null
MODPROBE_MIT=$?
if [ "$KERNEL_VERSION" -ge "$KERNEL26X" ]; then
	if [ $DEPMOD_MIT != 0 -o $INSMOD_MIT != 0 -o $RMMOD_MIT != 0 -o $MODPROBE_MIT != 0 ]; then
		echo "[!] Using modutils with 2.6.x kernel."
		echo "2.6.x requires newer module-init-tools."
		echo "WARNING!! Using old rmmod with 2.6.x kernel may crash the computer!"
		askreturnfail
	fi
else
	if [ $DEPMOD_MIT = 0 -o $INSMOD_MIT = 0 -o $RMMOD_MIT = 0 -o $MODPROBE_MIT = 0 ]; then
		echo "[!] Using module-init-tools with 2.4.x/2.2.x kernel."
		echo "These kernels require older modutils."
		askreturnfail
	fi
fi
# }}}
# {{{ [fold] Load modules if necessary
echo ""
echo "The QuickCam driver requires other drivers from kernel."
echo "I'll now check if those seem to be loaded."
askreturn

checkvideo() {
	cat /proc/modules | awk '{print $1}' | grep '^videodev' >/dev/null
	MOD_VID=$?
	cat /proc/devices | grep ' video_capture$' >/dev/null
	DEV_VID=$?
	if [ $MOD_VID != 0 -a $DEV_VID != 0 ]; then
		echo "[!] Linux video driver appears to be not loaded."
		echo "You could load it as root with command:"
		echo "	modprobe videodev"
		echo "(but I can do it for you automatically)"
		return 1
	fi
	if [ $DEV_VID = 0 ]; then
		NUM_VID="`cat /proc/devices | grep ' video_capture$' | awk '{print $1}'`"
		if [ "$NUM_VID" != "81" ]; then
			echo "[!] Video device is loaded but it has unusual major number $NUM_VID."
			echo "This may cause problems."
			askreturn
		fi
	fi
	return 0
}

checkusb() {
	cat /proc/modules | awk '{print $1}' | egrep '(^uhci|^usb-uhci|^usb-ohci)' >/dev/null
	MOD_HCD=$?
	if [ -d /proc/bus/usb ]; then PROCFS_USB=0; else PROCFS_USB=1; fi
	cat /proc/devices | grep ' usb$' >/dev/null
	DEV_USB=$?
	LSUSBOUT="`$LSUSB 2>&1`"
	LSUSBOK=$?
	echo "$LSUSBOUT" | grep 'Permission denied' >/dev/null
	if [ $? = 0 -o "$LSUSBOUT" = "" ]; then
		LSUSBOK=1
	fi
	echo "$LSUSBOUT" | grep 'No such file or directory' >/dev/null
	if [ $? = 0 ]; then
		LSUSBOK=1
	fi
	if [ $MOD_HCD != 0 -a $LSUSBOK != 0 ]; then
		echo "[!] USB host driver appears not to be loaded."
		echo "If your computer is typical PC with modularized UHCI or OHCI,"
		echo "you probably should issue the following commands:"
		echo "	modprobe uhci || modprobe usb-uhci || modprobe usb-ohci"
		echo "as root and retry. I can also do this automatically"
		echo "for testing purposes, but the modules need to be reloaded"
		echo "after each reboot."
	fi
	if [ $DEV_USB != 0 -a $LSUSBOK != 0 ]; then
		echo "[!] USB driver doesn't appear to be installed."
		return 1
	fi
	if [ $LSUSBOK != 0 ]; then
		echo "[!] lsusb ($LSUSB) doesn't work. Maybe USB filesystem is not mounted."
		echo "To mount it, do"
		echo "	mount none /proc/bus/usb -t usbdevfs"
		echo "as root, or insert line"
		echo "	none /proc/bus/usb usbdevfs defaults 0 0"
		echo "into /etc/fstab, and do command"
		echo "	mount -a"
		echo "as root."
		echo "Another possibility is that you're using too old version of lsusb,"
		echo "which would require root permissions to list USB devices."
		echo "In this case, don't worry, we just can't check if your camera"
		echo "is supported (but you can do it manually as root)."
		echo "Without lsusb, I can not detect automatically your camera."
		return 1
	fi
	return 0
}

echo "Modules loaded into the kernel:"
cat /proc/modules | awk '{print $1}' | tr '\n' ' '
echo ""
cat /proc/modules | awk '{print $1}' | egrep '(^quickcam|^mod_quickcam)' >/dev/null
if [ $? = 0 ]; then
	echo "[!] The QuickCam driver is already loaded!"
	echo "You should first remove the (old?) module by issuing"
	echo "	rmmod mod_quickcam || rmmod quickcam"
	echo "as root, otherwise I will fail to install the new module."
	echo "I will now try to unload it for you automatically,"
	echo "if you just give me the root password (Ctrl+D to cancel):"
	QCINSTCMD="unload" su -p -c "$0"
fi
cat /proc/modules | awk '{print $1}' | egrep '(^quickcam|^mod_quickcam)' >/dev/null
if [ $? = 0 ]; then
	echo "[!] QuickCam driver failed to unload."
	echo "It is not possible to install new version before unloading"
	echo "the older version. Check that no application is using the"
	echo "camera, using e.g."
	echo "	lsof /dev/video0"
	echo "or whatever is the camera device file."
	askreturnfail
fi

checkvideo
VIDEO_OK=$?
checkusb
USB_OK=$?
if [ $VIDEO_OK != 0 -o $USB_OK != 0 ]; then
	echo "I will now try to load the missing modules."
	echo "Type root password and press Enter (or Ctrl+D to abort)."
	QCINSTCMD="mod" su -p -c "$0"
	echo "Modules loaded now into the kernel:"
	cat /proc/modules | awk '{print $1}' | tr '\n' ' '
	echo ""
	checkvideo
	VIDEO_OK=$?
	checkusb
	USB_OK=$?
fi
if [ $VIDEO_OK != 0 -o $USB_OK != 0 ]; then
	echo "[!] Failed again. I did not succeed loading the necessary drivers."
	askreturnfail
fi
# }}}
# {{{ [fold] Detect camera
echo ""
echo "Next round: let's see if you have a supported QuickCam."
echo "Please plug in your USB camera before continuing."
askreturn
echo "I can find the following probably compatible devices:"
$LSUSB | grep -i 'ID 046d:' | egrep '(:0840 |:0850 |:0870 )'
FOUNDCAM=$?
if [ $FOUNDCAM != 0 ]; then
	echo "[!] Didn't find compatible cameras."
	echo "If you got message: \"Permission denied\", it means that"
	echo "you simply have too old lsusb, and you can ignore this problem."
	echo "In this case you have to be root to use lsusb, but I won't do that."
	askreturnfail
fi
# }}}
# {{{ [fold] Compilation
echo ""
echo "Another round done. Let's now compile the driver, it takes a while."
echo "This step will also clear old unnecessary files from the directory."
askreturn
make clean && make all
ls -la "$MODULE_NAME"
if [ ! -r "$MODULE_NAME" ]; then
	echo "[!] Looks like the driver compilation failed."
	echo "Did you get any error messages above?"
	echo "If asking for help, show what error messages you got."
	askreturnfail
fi
if [ ! -x ./qcset ]; then
	echo "[!] Looks like compilation of the utility programs failed."
	askreturnfail
fi
# }}}
# {{{ [fold] Load and check camera device file
echo ""
echo "Now everything should be well and the driver compiled."
echo "Let's then try actually loading the fresh driver and testing"
echo "if it works."
askreturn
echo "To load the driver, I need to know the root password."
QCINSTCMD="load" su -p -c "$0"
cat /proc/modules | awk '{print $1}' | egrep '(^quickcam)' >/dev/null
if [ $? != 0 ]; then
	dmesg
	echo "[!] The QuickCam driver failed to load!"
	echo "If you saw any special error messages, like about"
	echo "unresolved symbols, tell about them when asking for help."
	askreturnfail
fi
echo "The driver detected the following supported cameras:"
dmesg | grep '^quickcam'
if [ $? != 0 ]; then
	echo "[!] No cameras detected."
	echo "Try unloading and reloading the driver manually with"
	echo "	rmmod quickcam; insmod ./$MODULE_NAME debug=-1"
	echo "and then checking whether there are any messages indicating"
	echo "problems with command"
	echo "	dmesg"
	askreturnfail
fi
VIDEODEV=`dmesg | awk '/^quickcam: Registered device:/ { print $4 }' | head -n 1`
VIDEODEV_REAL="`$REALPATH $VIDEODEV`"
echo "I will be using $VIDEODEV, if there are more cameras I'll not test them."
askreturn
echo "Testing if $VIDEODEV is correct."
ls -la "$VIDEODEV"
if [ "$VIDEODEV" != "$VIDEODEV_REAL" ]; then
	ls -la "$VIDEODEV_REAL"
	echo "$VIDEODEV is a symbolic link to $VIDEODEV_REAL."
fi
if [ ! -r "$VIDEODEV_REAL" -o ! -w "$VIDEODEV_REAL" ]; then
	echo "[!] You don't have read/write access to $VIDEODEV."
	echo "On many distributions, you should add yourself into the"
	echo "\"video\" group by giving command"
	echo "	adduser `whoami` video"
	echo "and then log in again to be able to access the video."
	echo "A quick alternative is just to do"
	echo "	chmod a+rw $VIDEODEV_REAL"
	askreturnfail
fi
VIDEODEV_MAJ=`ls -la "$VIDEODEV_REAL" | awk '{print $5}' | tr -d ','`
VIDEODEV_MIN=`ls -la "$VIDEODEV_REAL" | awk '{print $6}'`
if [ "$VIDEODEV_MAJ" != "81" ]; then
	echo "[!] $VIDEODEV_REAL major number is $VIDEODEV_MAJ."
	echo "Usually it should be 81, so there are problems ahead."
	askreturnfail
fi
CAMERA_MIN=`echo $VIDEODEV | sed 's,.*/[^0-9]*\([0-9]*\),\1,g'`
if [ "$CAMERA_MIN" != "$VIDEODEV_MIN" ]; then
	echo "[!] Bad minor number $VIDEODEV_MIN in $VIDEODEV_REAL, should be $CAMERA_MIN."
	echo "To correct this problem, remove the bad file $VIDEODEV_REAL and"
	echo "recreate it with mknod-command (read man mknod). Example:"
	echo "	rm -f $VIDEODEV"
	echo "	mknod $VIDEODEV c 81 $CAMERA_MIN"
	echo "	chmod a+rw $VIDEODEV"
	askreturnfail
fi
# }}}
# {{{ [fold] Final test
echo ""
echo "Right now driver is loaded and should be ready to run."
echo "Let's test if user applications can see it, starting with qcset."
askreturn
./qcset "$VIDEODEV" -i | head -n 1 | grep 'Logitech QuickCam USB'
if [ $? != 0 ]; then
	echo "[!] qcset did not found the QuickCam camera"
	askreturnfail
fi
echo "If you like, you can quit now and start using the camera -"
echo "you have good chances that it works, if no problems were detected."
echo "If you have X Window System running and xawtv installed,"
echo "I can now run it automatically for you."
echo "You will then also have opportunity to install the driver permanently."
askreturn
xdpyinfo 2>&1 >/dev/null
if [ $? != 0 ]; then
	echo "[!] Looks like you don't have X Window System running."
	echo "It is necessary for testing the camera."
	askreturnfail
fi
echo "Launching xawtv (press q on xawtv window to quit it)..."
echo "If the image is not sharp, try focusing it by turning the"
echo "wheel around the camera lens."
echo "	xawtv -noscale -noxv -c \"$VIDEODEV\""
xawtv -noscale -noxv -c "$VIDEODEV"
# }}}
# {{{ [fold] Final install
echo ""
echo "Well, did it work, did you get a picture?"
echo "If you did, you might now want to install the driver"
echo "permanently. Just proceed to do that..."
askreturn
echo "Just an extra warning: the driver ($MODULE_NAME) and"
echo "the utility (qcset) will be now copied into system"
echo "directories. If you have already other versions,"
echo "they will be overwritten. Verify by giving root password."
QCINSTCMD="inst" su -p -c "$0"
if [ ! -f "$INSTALL_DIR/misc/$MODULE_NAME" ]; then
	echo "[!] Module install failed to $INSTALL_DIR/misc/$MODULE_NAME"
	askreturnfail
fi
echo "Hopefully the driver is now installed and can be loaded"
echo "with command"
echo "	modprobe quickcam"
echo "as root. You can put this command into some startup"
echo "script to do it always automatically at boot."
echo "The exact location depends on distribution, and this"
echo "script is yet too dumb to do this automatically."
askreturn
# }}}

echo "Goodbye..."
exit 0

