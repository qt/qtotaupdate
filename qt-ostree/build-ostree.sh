#!/bin/bash
#############################################################################
##
## Copyright (C) 2016 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is part of the Qt OTA Update module of the Qt Toolkit.
##
## $QT_BEGIN_LICENSE:GPL$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see https://www.qt.io/terms-conditions. For further
## information use the contact form at https://www.qt.io/contact-us.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 3 or (at your option) any later version
## approved by the KDE Free Qt Foundation. The licenses are as published by
## the Free Software Foundation and appearing in the file LICENSE.GPL3
## included in the packaging of this file. Please review the following
## information to ensure the GNU General Public License requirements will
## be met: https://www.gnu.org/licenses/gpl-3.0.html.
##
## $QT_END_LICENSE$
##
#############################################################################
set -e
set -x
# On Ubuntu 14.04 the following build dependecies needs to be installed:
# sudo apt-get install git autoconf gtk-doc-tools libattr1-dev libcap-dev libghc-gio-dev liblzma-dev \
# e2fslibs-dev libgpgme11-dev libsoup2.4-dev libarchive-dev
DIR=$(dirname $(readlink -f $0))
OTA_LIBGSYSTEM_REF="v2015.2"
OTA_OSTREE_REF="v2016.5"
PARALLEL=4

cd "${DIR}"
rm -rf libgsystem-git ostree-git

# Build libgsystem.
# Note: There is an ongoing work in the ostree project to remove this dependency.
git clone https://github.com/GNOME/libgsystem.git libgsystem-git
cd libgsystem-git
git checkout ${OTA_LIBGSYSTEM_REF}
git submodule update --init
./autogen.sh \
    --disable-shared \
    --enable-static \
    CFLAGS='-fPIC'

make V=1 -j ${PARALLEL}
export PKG_CONFIG_PATH=$PWD/src
export LD_LIBRARY_PATH=$PWD/.libs
cd -

# Build OSTree.
git clone https://github.com/ostreedev/ostree.git ostree-git
cd ostree-git
git checkout ${OTA_OSTREE_REF}
git am "${DIR}"/patches/ostree-Fix-enable_rofiles_fuse-no-build.patch
git am "${DIR}"/patches/ostree-Allow-updating-files-in-the-boot-directory.patch
git am "${DIR}"/patches/ostree-u-boot-Merge-ostree-s-and-systems-uEnv.txt.patch
git am "${DIR}"/patches/ostree-Create-firmware-convenience-symlinks.patch
git am "${DIR}"/patches/ostree-Fix-build-for-HAVE_LIBSOUP_CLIENT_CERTS.patch

./autogen.sh \
    --with-soup \
    --enable-rofiles-fuse=no \
    --enable-gtk-doc-html=no \
    --enable-man=no \
    --disable-shared \
    --enable-static \
    CFLAGS='-I../libgsystem-git/src -fPIC' \
    LDFLAGS='-Wl,-rpath,\$$ORIGIN/lib/'

make V=1 -j ${PARALLEL}
unset PKG_CONFIG_PATH
unset LD_LIBRARY_PATH
cd -

mv ostree-git/ostree .
strip ostree
rm -rf libgsystem-git ostree-git
