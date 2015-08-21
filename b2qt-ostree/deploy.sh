#!/bin/sh
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
set -e

if [ $(id -u) -ne 0 ]; then
    echo "You need root privileges to run this script"
    exit 1
fi

usage()
{
    echo "Usage: $0 <device>"
}

DEVICE=""
UBOOT="u-boot.imx"

# parse parameters
while [ $# -gt 0 ] ; do
    case "${1}" in
      -y)
          DONTASK=1
          ;;
      --uboot)
          UBOOT="${2}"
          shift 1
          ;;
      --verbose)
          VERBOSE="v"
          ;;
      -h | -help | --help)
          usage
          exit 0
          ;;
      -*)
          echo "Unknown parameter: ${1}"
          usage
          exit 1
          ;;
      *)
          if [ -n "${DEVICE}" ] ; then
              echo "Device given twice: ${DEVICE} and ${1}"
              usage
              exit 1
          else
              DEVICE="${1}"
          fi
          ;;
      esac
      shift 1
done

if [ -z "${DEVICE}" ]; then
    usage
    exit 1
fi

DIR=$(dirname $(readlink -f $0))
DISK=$(readlink -f ${DEVICE})

if [ -n "$(command -v lsblk)" ]; then
    lsblk
fi
fdisk -l ${DISK}
if [ -z "${DONTASK}" ]; then
    read -p "Reformat disk '${DISK}', all files will be lost? (y/N) " format
    if [ "$format" != "y" -a "$format" != "Y" ]; then
        exit 0
    fi
fi

echo "-- STEP -- Formatting memory card ${DEVICE}"
umount ${DISK}* 2> /dev/null || true
${DIR}/mkcard.sh ${DISK}

if [ -b ${DISK}1 ]; then
    BOOT=${DISK}1
    ROOTFS=${DISK}2
elif [ -b ${DISK}p1 ]; then
    BOOT=${DISK}p1
    ROOTFS=${DISK}p2
else
    echo "Unknown device partitions"
    exit 1
fi

echo "-- STEP -- Deploying boot files"
udisks --mount ${BOOT} > /dev/null
MOUNTPOINT=$(grep ${BOOT} /proc/mounts | awk '{print $2}')
tar ${VERBOSE}xzf ${DIR}/ota-deploy/boot.tar.gz -C ${MOUNTPOINT} --owner=root --group=root
if [ -e uEnv.txt ]; then
    cp uEnv.txt ${MOUNTPOINT}
fi
sync
if [ -e ${MOUNTPOINT}/${UBOOT} ]; then
    dd if=${MOUNTPOINT}/${UBOOT} of=${DISK} bs=512 seek=2
fi

udisks --unmount ${BOOT}

echo "-- STEP -- Deploying rootfs, this may take a while"
udisks --mount ${ROOTFS} > /dev/null
MOUNTPOINT=$(grep ${ROOTFS} /proc/mounts | awk '{print $2}')
tar ${VERBOSE}xzf ${DIR}/ota-deploy/rootfs.tar.gz -C ${MOUNTPOINT}
sync
udisks --unmount ${ROOTFS}

echo "-- STEP -- Done"
