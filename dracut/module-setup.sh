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
check() {
    if [ -x /usr/sbin/ostree-prepare-root ]; then
       return 255
    fi

    return 1
}

depends() {
    return 0
}

install() {
    inst_hook cleanup 20 "$moddir/prepare-root.sh"
    inst_hook initqueue/finished 99 "$moddir/check-udev-finished.sh"
    inst_multiple ostree-prepare-root
}
