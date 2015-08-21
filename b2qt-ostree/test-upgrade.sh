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
set -x

ROOT=$PWD

touch tree/usr/lib/yo.so

cd ${ROOT}/tree
COMMIT_SUBJECT=$(date +"%m-%d-%Y %T")
ostree --repo=${ROOT}/ostree-repo commit -b qt/b2qt -s "Generated commit subject: ${COMMIT_SUBJECT}"
ostree --repo=${ROOT}/ostree-repo log qt/b2qt

# FOR LOCAL UPDATES
# cd ${ROOT}
# export OSTREE_SYSROOT=sysroot
# ostree admin upgrade --os=b2qt




