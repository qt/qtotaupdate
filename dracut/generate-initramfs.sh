#!/bin/bash
#############################################################################
##
## Copyright (C) 2015 Digia Plc and/or its subsidiary(-ies).
##
## This file is part of the Qt Enterprise Embedded Scripts of the Qt
## framework.
##
## $QT_BEGIN_LICENSE$
## Commercial License Usage Only
## Licensees holding valid commercial Qt license agreements with Digia
## with an appropriate addendum covering the Qt Enterprise Embedded Scripts,
## may use this file in accordance with the terms contained in said license
## agreement.
##
## For further information use the contact form at
## http://www.qt.io/contact-us.
##
##
## $QT_END_LICENSE$
##
#############################################################################
#set -x
set -e

if [ ! -x "$(which adb)" ] ; then
    echo "Needed command 'adb' not found in PATH."
    exit 1
fi

ROOT=$(dirname $(readlink -f $0))

output=$(adb shell ls /usr/sbin/ostree-prepare-root)
substring="No such file or directory"
if [[ $output == *${substring}* ]] ; then
    echo "error: Failed to find the required binary /usr/sbin/ostree-prepare-root on a device."
    exit 1
fi

output=$(adb shell ls /lib/systemd/systemd)
systemd=false
if [[ $output != *${substring}* ]] ; then
    systemd=true
fi

options='/boot/initramfs.img
        --host-only
        --add ostree
        --omit i18n
        --stdlog 3
        --force'

if [ ${systemd} = true ] ; then
    # OSTree ships with a dracut module for systemd based images.
    echo "Generating initramfs for systemd based image ..."
    adb push ${ROOT}/systemd/01-b2qt.conf /etc/dracut.conf.d/
    custom_options='--add systemd'
else
    # Deploy our custom dracut module for systemd-less images.
    echo "Generating initramfs for system v init based image ..."
    MODULE_PATH=/usr/lib/dracut/modules.d/98ostree/
    adb shell mkdir -p $MODULE_PATH
    adb push ${ROOT}/systemv/module-setup.sh $MODULE_PATH
    adb push ${ROOT}/systemv/prepare-root.sh $MODULE_PATH
    custom_options='--omit systemd'
fi

# Terminate when the explicitly required modules could not be found or installed.
adb shell dracut ${options} ${custom_options} | tee dracut.log
errors=$(cat dracut.log | grep -i "cannot be found or installed" | wc -l)
rm dracut.log
if [ ${errors} -gt 0 ] ; then
    echo "error: Failed to include the required modules into the initramfs image."
    exit 1
fi

rm -f initramfs.img
adb pull /boot/initramfs.img
device=$(adb shell uname -n | tr -d '\r')
release=$(adb shell uname -r | tr -d '\r' | cut -d'-' -f1)
initramfs=initramfs-${device}-${release}
rm -rf ${initramfs}
mkimage -A arm -O linux -T ramdisk -a 0 -e 0 -d initramfs.img ${initramfs}
rm -f initramfs.img

echo
echo "Done, generated OSTree boot compatible initramfs:"
echo
echo ${initramfs}
echo
