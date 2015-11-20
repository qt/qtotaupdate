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

rm -f initramfs.img ota-initramfs.ub

output=$(adb shell ls /usr/sbin/ostree-prepare-root)
contains="No such file or directory"
if [[ "$output" == *"$contains"* ]] ; then
    echo "error: Failed to find the required binary /usr/sbin/ostree-prepare-root on a device."
    exit 1
fi

MODULE_PATH=/usr/lib/dracut/modules.d/90ostree/

adb shell mkdir -p $MODULE_PATH
adb push module-setup.sh $MODULE_PATH
adb push prepare-root.sh $MODULE_PATH
adb push check-udev-finished.sh $MODULE_PATH

echo "Generating initramfs image ..."
adb shell dracut /boot/initramfs.img --host-only --omit systemd --add "ostree" --persistent-policy by-label --force --stdlog 3
adb pull /boot/initramfs.img

mkimage -A arm -O linux -T ramdisk -a 0 -e 0 -d initramfs.img ota-initramfs.ub

adb shell rm /boot/initramfs.img
rm -f initramfs.img
echo
echo "Done. Generated OSTree boot compatible initramfs - ota-initramfs.ub."
echo
