#############################################################################
##
## Copyright (C) 2016 The Qt Company Ltd.
## Contact: http://www.qt.io/licensing/
##
## This file is part of the Qt Enterprise Embedded Scripts of the Qt
## framework.
##
## $QT_BEGIN_LICENSE:COMM$
##
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see http://www.qt.io/terms-conditions. For further
## information use the contact form at http://www.qt.io/contact-us.
##
## $QT_END_LICENSE$
##
#############################################################################

. /lib/dracut-lib.sh

if $(getargbool recovery) ; then
    warn "Doing actual recovery"
    root_partition=$(getarg root)
    boot_partition=${root_partition::-1}$((${root_partition: -1} -1))

    e2fsck -p "${boot_partition}"
    e2fsck -p "${root_partition}"
    # 'reboot' and 'systemctl reboot' are shutting down the system but do not
    # trigger a reset after halting.
    # WARNING: Make sure to sync and unmount all mounted disks before restarting in this way.
    echo b >/proc/sysrq-trigger
else
    warn "Not doing recovery"
fi
