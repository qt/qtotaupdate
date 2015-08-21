#! /bin/sh
# mkcard.sh v0.5
# (c) Copyright 2009 Graeme Gregory <dp@xora.org.uk>
# Licensed under terms of GPLv2
#
# Parts of the procudure base on the work of Denys Dmytriyenko
# http://wiki.omap.com/index.php/MMC_Boot_Format

set -e

export LC_ALL=C

if [ $# -ne 1 ]; then
    echo "Usage: $0 <drive>"
    exit 1
fi

if ! which mkfs.ext2 >/dev/null ; then
    echo "Tool mkfs.ext2 needs to be installed"
    exit 1
fi

DRIVE=$1

dd if=/dev/zero of=$DRIVE bs=1024 count=1024

SIZE=`fdisk -l $DRIVE | grep Disk | grep bytes | awk '{print $5}'`

echo DISK SIZE - $SIZE bytes

SFDISK_VERSION=`sfdisk --version|awk '{print $4}'`
SFDISK_NEW_VERSION="2.26"
SFDISK_MIN_VERSION=`{
echo $SFDISK_VERSION
echo $SFDISK_NEW_VERSION
} | sort -V | head -n1`

echo "Using sfdisk unnamed-fields format for vesion $SFDISK_MIN_VERSION"

if [ "$SFDISK_MIN_VERSION" = "$SFDISK_NEW_VERSION" ]; then
    {
    echo ,64MiB,,*
    echo ,,,-
    } | sfdisk $DRIVE
else
    CYLINDERS=`echo $SIZE/255/63/512 | bc`
    echo CYLINDERS - $CYLINDERS
    {
    echo 1,9,,*
    echo 10,,,-
    } | sfdisk -D -H 255 -S 63 -C $CYLINDERS $DRIVE
fi

sleep 1

# handle various device names.
# note something like fdisk -l /dev/loop0 | egrep -E '^/dev' | cut -d' ' -f1
# won't work due to https://bugzilla.redhat.com/show_bug.cgi?id=649572

PARTITION1=${DRIVE}1
if [ ! -b ${PARTITION1} ]; then
    PARTITION1=${DRIVE}p1
fi

PARTITION2=${DRIVE}2
if [ ! -b ${PARTITION2} ]; then
    PARTITION2=${DRIVE}p2
fi

# now make partitions.
if [ -b ${PARTITION1} ]; then
    umount ${PARTITION1} || true
    mkfs.ext2 -L "boot" ${PARTITION1}
else
    echo "Cant find boot partition in /dev"
    exit 1
fi

if [ -b ${PARITION2} ]; then
    umount ${PARTITION2} || true
    mke2fs -t ext3 -j -L "rootfs" ${PARTITION2}
else
    echo "Cant find rootfs partition in /dev"
    exit 1
fi
