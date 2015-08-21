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

if [ ! -x "$(which ostree)" ]
then
    echo "Needed command 'ostree' not found in PATH."
    exit 1
fi

usage()
{
    echo ""
    echo "Usage: $0 OPTIONS"

    echo ""
    echo "--sysroot-image-path                  A path to b2qt sysroot [rootfs,b2qt,boot].tar.gz images."
    echo "--initramfs                           OSTree boot compatible initramfs."
    echo "--uboot-env-file                      OSTree boot compatible u-boot environment file."
    echo "--ostree-repo                         Commits the generated tree into this repository. If the repository does not exist, "
    echo "                                      one is created in the specified location. If this argument is not provided, "
    echo "                                      a default repository is created in the same directory as this script."
    echo "--ostree-branch os/branch-name        Commits the generated update in the specified OSTree branch. A default branch is qt/b2qt."
    echo "--ostree-commit-subject \"Subject\"     Commits the generated update with the specified commit subject. A default commit subject is the"
    echo "                                      date and time when the update was committed."
    echo "--create-initial-deployment           Generates Over-The-Air Update ready sysroot in the 'sysroot' directory."

    echo ""
}

ROOT=$(dirname $(readlink -f $0))

OSTREE_REPO=${ROOT}/ostree-repo
GENERATED_TREE=${ROOT}/tree
UBOOT_ENV_FILE=""
SERVER_ROOT=${ROOT}/httpd
OTA_SYSROOT=false
MISSING_ARGS=false
OSTREE_BRANCH=qt/b2qt
OSTREE_COMMIT_SUBJECT=""

# Private variables.
DO_B2QT_DEPLOY=true

missing_arg()
{
    if [ $MISSING_ARGS = false ] ; then
        echo ""
    fi
    echo ${1} " is not set!"
    MISSING_ARGS=true
}

while [ $# -gt 0 ] ; do
    case "${1}" in
      --create-initial-deployment)
          OTA_SYSROOT=true
          ;;
      --ostree-repo)
          OSTREE_REPO=${2}
          shift 1
          ;;
      --uboot-env-file)
          UBOOT_ENV_FILE=${2}
          shift 1
          ;;
      --ostree-branch)
          OSTREE_BRANCH=${2}
          shift 1
          ;;
      --ostree-commit-subject)
          OSTREE_COMMIT_SUBJECT=${2}
          shift 1
          ;;
      --sysroot-image-path)
          SYSROOT_IMAGE_PATH=${2}
          shift 1
          ;;
      --initramfs)
          INITRAMFS=${2}
          shift 1
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
    esac
    shift 1
done

if [ -z "${SYSROOT_IMAGE_PATH}" ] ; then missing_arg "--sysroot-image-path"; fi
if [ -z "${INITRAMFS}" ] ; then missing_arg "--initramfs"; fi
if [ $MISSING_ARGS = true ] ; then
    echo ""
    usage
    exit 1
fi

rm -rf ${GENERATED_TREE}
mkdir ${GENERATED_TREE}
tar -C ${GENERATED_TREE} -xvf ${SYSROOT_IMAGE_PATH}/rootfs.tar.gz
tar -C ${GENERATED_TREE} -xvf ${SYSROOT_IMAGE_PATH}/b2qt.tar.gz
BOOT_FILE_PATH=${GENERATED_TREE}/usr/system/boot
mkdir -p ${BOOT_FILE_PATH}/
tar -C ${BOOT_FILE_PATH}/ -xvf ${SYSROOT_IMAGE_PATH}/boot.tar.gz

# Keep everything what is needed for booting in one place.
if [ -d ${GENERATED_TREE}/boot ] && [ "$(ls -A ${GENERATED_TREE}/boot)" ] ; then
    cp -r ${GENERATED_TREE}/boot/* ${BOOT_FILE_PATH}/
    rm -rf ${GENERATED_TREE}/boot/
fi
if [ -n "${UBOOT_ENV_FILE}" ] ; then
    cd ${ROOT}
    cp ${UBOOT_ENV_FILE} ${BOOT_FILE_PATH}/

    file=$(basename ${UBOOT_ENV_FILE})
    if [ "$file" != "uEnv.txt" ] ; then
        # must be a boot script then, check
        # if its compiled or plain text
        if [ ${file: -4} == ".txt" ] ; then
            echo "Compiling u-boot boot script..."
            mkimage -A arm -O linux -T script -C none -a 0 -e 0 -n "boot script" -d ${BOOT_FILE_PATH}/${file} ${BOOT_FILE_PATH}/${file%.*}
        fi
    fi
fi

find_and_rename_kernel()
{
    cd ${BOOT_FILE_PATH}/
    for file in *; do
        if file -b $file | grep -qi "kernel"; then
            echo "Found kernel image ${PWD}/${file}"
            cp ${file} ${GENERATED_TREE}/boot/vmlinuz
            return 0
        fi
    done
    echo "Failed to find kernel image in ${PWD} !"
    exit 1
}

# Only initramfs and kernel image files are required
# in /boot for OSTree tool to recognize this as a valid tree.
mkdir -p ${GENERATED_TREE}/boot/
cp ${INITRAMFS} ${GENERATED_TREE}/boot/initramfs
find_and_rename_kernel
cd ${GENERATED_TREE}/boot/
bootcsum=$(cat vmlinuz initramfs | sha256sum | cut -f 1 -d ' ')
mv vmlinuz vmlinuz-${bootcsum}
mv initramfs initramfs-${bootcsum}

# OSTree requires /etc/os-release file. This data is show in multi-boot
# setup, but U-boot does not have multi-boot menu on system startup so it
# doesn't really matter what we put here.
cat > ${GENERATED_TREE}/etc/os-release <<EOF
PRETTY_NAME="Boot 2 Qt Reference Image"
EOF

# Adjust rootfs according to OSTree guidelines.
cd ${GENERATED_TREE}
mkdir -p usr/system/
mv bin/ lib/ sbin/ data/ usr/system/
mkdir -p sysroot/ostree/
ln -s sysroot/ostree/ ostree
ln -s usr/system/bin bin
ln -s usr/system/lib lib
ln -s usr/system/sbin sbin
ln -s usr/system/data data
mv home/ var/
ln -s var/home home

# Commit the generated tree into OSTree repository.
mkdir -p ${OSTREE_REPO}
if [ ! -d ${OSTREE_REPO}/objects ] ; then
    echo "Initializing new OSTree repository in ${OSTREE_REPO}"
    ostree --repo=${OSTREE_REPO} init --mode=archive-z2
fi

if [ -z "${OSTREE_COMMIT_SUBJECT}" ] ; then
    current_time=$(date +"%m-%d-%Y %T")
    OSTREE_COMMIT_SUBJECT="Generated commit subject: ${current_time}"
fi

cd ${GENERATED_TREE}
echo "Committing the generated tree into a repository at ${OSTREE_REPO} ..."
ostree --repo=${OSTREE_REPO} commit -b ${OSTREE_BRANCH} -s "${OSTREE_COMMIT_SUBJECT}"
ostree --repo=${OSTREE_REPO} fsck

# Prepare sysroot. This is what goes on a device when
# preparing a device for a first time. Subsequent sysroot
# changes to a device should happen via OTA update.
if [ $OTA_SYSROOT = true ] ; then
    echo "Preparing initial deployment..."
    cd ${ROOT}
    rm -rf sysroot

    mkdir -p sysroot/sysroot/
    export OSTREE_SYSROOT=sysroot
    ostree admin init-fs sysroot
    ostree admin os-init b2qt

    # Tell OSTree that it should use U-Boot handler.
    mkdir -p sysroot/boot/loader.0/
    ln -s loader.0 sysroot/boot/loader
    touch sysroot/boot/loader/uEnv.txt

    # Currently there is no built-in support in OSTree for updating DTBs (and other
    # files that might be stored on boot partition, like second stage bootloader and
    # etc.) *so we just copy those files directly to /boot.*
    # TODO - find some way to workaround this limitation.
    cp -r ${BOOT_FILE_PATH}/* sysroot/boot/
    ln -s ../usr/system/boot/ sysroot/boot/boot-update

    ostree --repo=sysroot/ostree/repo pull-local --remote=b2qt ${OSTREE_REPO} ${OSTREE_BRANCH}
    ostree admin deploy --os=b2qt b2qt:${OSTREE_BRANCH}
    ostree admin status | tee status.txt

    # OSTree does not touch the contents of /var,
    # /var needs to be dynamically managed at boot time.
    # TODO - handle this at boot time!
    rm -rf sysroot/ostree/deploy/b2qt/var/
    cp -R ${GENERATED_TREE}/var/ sysroot/ostree/deploy/b2qt/

    # Repack.
    if [ $DO_B2QT_DEPLOY = true ] ; then
        rm -rf ota-deploy
        mkdir ota-deploy/
        cd ota-deploy/
        tar -pczvf boot.tar.gz -C ../sysroot/boot/ .
        mv ../sysroot/boot/ ../boot/
        tar -pczvf rootfs.tar.gz -C ../sysroot/ .
        mv ../boot/ ../sysroot/boot/
    fi
fi

# TODO - Improve this
if [ ! -d ${SERVER_ROOT} ] ; then
    mkdir ${SERVER_ROOT}
    cd ${SERVER_ROOT}
    ln -s ${OSTREE_REPO} ostree
    ostree trivial-httpd --autoexit --daemonize -p ${SERVER_ROOT}/httpd-port
    PORT=$(cat ${SERVER_ROOT}/httpd-port)
    echo "http://127.0.0.1:${PORT}" > ${SERVER_ROOT}/httpd-address
fi

