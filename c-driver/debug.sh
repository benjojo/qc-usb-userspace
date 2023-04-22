#!/bin/sh
/etc/init.d/klogd stop
modprobe usb || modprobe usb-uhci || modprobe usb-ohci
modprobe modprobe uhci-hcd || modprobe ohci-hcd
modprobe videodev
sync
tee debug.log < /proc/kmsg
