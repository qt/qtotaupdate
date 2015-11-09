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
    echo "error: Needed command 'ostree' not found in PATH."
    exit 1
fi

ROOT=$(dirname $(readlink -f $0))

if [ "${PWD}" != "${ROOT}" ] ; then
    echo "error: The script must be executed from the directory containing the 'prepare-update.sh' script: ${ROOT}"
    exit 1
fi

GENERATED_TREE=${ROOT}/tree
BOOT_FILE_PATH=${GENERATED_TREE}/boot
UBOOT_ENV_FILE=""
SERVER_ROOT=${ROOT}/httpd
OTA_SYSROOT=false
INVALID_ARGS=false
VERBOSE=""
# REPO
OSTREE_REPO=${ROOT}/ostree-repo
OSTREE_BRANCH=qt/b2qt
OSTREE_COMMIT_SUBJECT=""
# GPG
USE_GPG=false
GPG_KEY=""
GPG_HOMEDIR=""
GPG_TRUSTED_KEYRING=""
GPG_KEYS_PATH=${GENERATED_TREE}/usr/share/ostree/trusted.gpg.d/
# Private variables.
DO_B2QT_DEPLOY=true

usage()
{
    echo
    echo "Usage: $0 OPTIONS"

    echo
    echo "--sysroot-image-path DIR              A path to b2qt sysroot [rootfs,b2qt,boot].tar.gz images."
    echo "--initramfs FILE                      OSTree boot compatible initramfs."
    echo "--uboot-env-file FILE                 OSTree boot compatible u-boot environment file."
    echo
    echo "--ostree-repo DIR                     Commits the generated tree into this repository. If the repository does not exist, "
    echo "                                      one is created in the specified location. If this argument is not provided, "
    echo "                                      a default repository is created in the same directory as this script."
    echo "--ostree-branch os/branch-name        Commits the generated update in the specified OSTree branch. A default branch is qt/b2qt."
    echo "--ostree-commit-subject \"SUBJECT\"     Commits the generated update with the specified commit subject. A default commit subject is the"
    echo "                                      date and time when the update was committed."
    echo
    echo "--create-initial-deployment           Generates Over-The-Air Update ready sysroot in the 'sysroot' directory."
    echo "--verbose                             Prints more information to the console."
    echo
    echo "--gpg-sign KEY-ID                     GPG Key ID to use for signing the commit."
    echo "--gpg-homedir DIR                     GPG home directory to use when looking for keyrings."
    echo "--gpg-trusted-keyring FILE            Adds the provided keyring file to the generated sysroot (see --create-initial-deployment) or to"
    echo "                                      the generated update. When providing a new keyring via update, the new keyring will be used for"
    echo "                                      signature verification only after this update has been applied."
    echo
}

update_invalid_args()
{
    if [ $INVALID_ARGS = false ] ; then
        echo
        echo "Following errors occurred:"
        echo
    fi
    INVALID_ARGS=true
}

validate_arg()
{
    arg=${1}
    value=${2}
    required=${3}
    flag=${4}

    if [ -z "${value}" ] ; then
        if [ ${required} = true ] ; then
            update_invalid_args
            echo ${arg} "is not set."
        fi
        return 0
    fi

    if [ ! -${flag} ${value} ] ; then
        update_invalid_args
        case ${flag} in
            d)
                echo "${arg} requires a valid path. The provided ${value} path does not exist."
                ;;
            f)
                echo "${arg} requires a path to an existing file. The provided ${value} file does not exist."
                ;;
        esac
        return 0
    fi

    valid=true
    case "${arg}" in
        --gpg-trusted-keyring)
            if [[ ${value} != *.gpg ]] ; then
                valid=false
                error="--gpg-trusted-keyring expects gpg keyring file with the .gpg extension, but ${value} was provided."
            fi
            ;;
        --gpg-homedir)
            if [[ ${value} != /* ]] ; then
                valid=false
                error="--gpg-homedir must be an absolute path, but ${value} was provided."
            fi
            ;;
    esac

    if [ $valid = false ] ; then
        update_invalid_args
        echo ${error}
        return 0
    fi
}

parse_args()
{
    while [ $# -gt 0 ] ; do
        case "${1}" in
          --create-initial-deployment)
              OTA_SYSROOT=true
              ;;
          --verbose)
              VERBOSE=v
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
          --gpg-sign)
              GPG_KEY=${2}
              USE_GPG=true
              shift 1
              ;;
          --gpg-homedir)
              GPG_HOMEDIR=${2}
              USE_GPG=true
              shift 1
              ;;
          --gpg-trusted-keyring)
              GPG_TRUSTED_KEYRING=${2}
              shift 1
              ;;
          -h | -help | --help)
              usage
              exit 0
              ;;
          -*)
              echo
              echo "Unknown parameter: ${1}"
              usage
              exit 1
              ;;
        esac
        shift 1
    done

    validate_arg "--sysroot-image-path" "${SYSROOT_IMAGE_PATH}" true d
    validate_arg "--initramfs" "${INITRAMFS}" true f
    validate_arg "--uboot-env-file" "${UBOOT_ENV_FILE}" false f
    validate_arg "--gpg-homedir" "${GPG_HOMEDIR}" false d
    validate_arg "--gpg-trusted-keyring" "${GPG_TRUSTED_KEYRING}" false f

    if [[ $USE_GPG = true && ( -z "${GPG_KEY}" || -z "${GPG_HOMEDIR}") ]] ; then
        update_invalid_args
        echo "Both --gpg-sign and --gpg-homedir must be provided when using the gpg signing feature."
    fi

    if [ $INVALID_ARGS = true ] ; then
        usage
        exit 1
    fi
}

find_and_rename_kernel()
{
    cd ${BOOT_FILE_PATH}/
    for file in *; do
        if file -b $file | grep -qi "kernel"; then
            echo "Found kernel image ${BOOT_FILE_PATH}/${file}"
            mv ${file} vmlinuz
            cd - &> /dev/null
            return 0
        fi
    done
    echo "error: Failed to find kernel image in ${BOOT_FILE_PATH}."
    exit 1
}

organize_boot_files()
{
    cd ${ROOT}
    cp ${INITRAMFS} ${BOOT_FILE_PATH}/initramfs
    find_and_rename_kernel
    if [ -n "${UBOOT_ENV_FILE}" ] ; then
        cp ${UBOOT_ENV_FILE} ${BOOT_FILE_PATH}/
        name=$(basename ${UBOOT_ENV_FILE})
        if [ "${name}" != "uEnv.txt" ] ; then
            cd ${BOOT_FILE_PATH}
            # Must be a boot script then.
            if file -b $name | grep -qi "ASCII text"; then
                echo "Compiling u-boot boot script: ${BOOT_FILE_PATH}/${name}..."
                mv ${name} ${name}-tmp
                mkimage -A arm -O linux -T script -C none -a 0 -e 0 -n "boot script" -d ${name}-tmp ${name}
                rm ${name}-tmp
            fi
        fi
    fi
}

convert_b2qt_to_ostree()
{
    cd ${GENERATED_TREE}
    if [[ ! -e usr/bin/ostree || ! -e usr/sbin/ostree-remount ]] ; then
        echo "error: The provided sysroot does not contain required binaries."
        exit 1
    fi

    cd ${BOOT_FILE_PATH}
    bootcsum=$(cat vmlinuz initramfs | sha256sum | cut -f 1 -d ' ')
    mv vmlinuz vmlinuz-${bootcsum}
    mv initramfs initramfs-${bootcsum}

    # OSTree requires /etc/os-release file (see The Boot Loader Specification).
    # U-boot does not have multi-boot menu so it doesn't really matter what we put here.
    echo "PRETTY_NAME=\"Boot 2 Qt\"" > ${GENERATED_TREE}/etc/os-release

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

    # Run ostree-remount on startup. This makes sure that
    # rw/ro mounts are set correctly for ostree to work.
    cp ${ROOT}/ostree-remount.sh etc/init.d/
    # System V init services are started in alphanumeric order. We want to remount
    # things as early as possible so we prepend 'a' in S01[a]ostree-remount.sh
    ln -fs ../init.d/ostree-remount.sh etc/rc1.d/S01aostree-remount.sh
    ln -fs ../init.d/ostree-remount.sh etc/rc2.d/S01aostree-remount.sh
    ln -fs ../init.d/ostree-remount.sh etc/rc3.d/S01aostree-remount.sh
    ln -fs ../init.d/ostree-remount.sh etc/rc4.d/S01aostree-remount.sh
    ln -fs ../init.d/ostree-remount.sh etc/rc5.d/S01aostree-remount.sh

    # Add trusted GPG keyring file
    if [ -n "${GPG_TRUSTED_KEYRING}" ] ; then
        cd ${ROOT}
        cp ${GPG_TRUSTED_KEYRING} ${GPG_KEYS_PATH}
    fi
}

commit_generated_tree()
{
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
    echo "Committing the generated tree into a repository at ${OSTREE_REPO}..."
    if [ $USE_GPG = true ] ; then
        ostree --repo=${OSTREE_REPO} commit -b ${OSTREE_BRANCH} -s "${OSTREE_COMMIT_SUBJECT}" --gpg-sign="${GPG_KEY}" --gpg-homedir="${GPG_HOMEDIR}"
    else
        ostree --repo=${OSTREE_REPO} commit -b ${OSTREE_BRANCH} -s "${OSTREE_COMMIT_SUBJECT}"
    fi

    ostree --repo=${OSTREE_REPO} fsck
}

create_initial_deployment()
{
    echo "Preparing initial deployment..."
    cd ${ROOT}
    rm -rf sysroot status.txt

    mkdir -p sysroot/sysroot/
    export OSTREE_SYSROOT=sysroot
    ostree admin init-fs sysroot
    ostree admin os-init b2qt

    # Tell OSTree to use U-Boot boot loader backend.
    mkdir -p sysroot/boot/loader.0/
    ln -s loader.0 sysroot/boot/loader
    touch sysroot/boot/loader/uEnv.txt
    ln -s loader/uEnv.txt sysroot/boot/uEnv.txt

    # Add convenience symlinks, for details see
    # ostree/src/libostree/ostree-bootloader-uboot.c
    for file in ${BOOT_FILE_PATH}/* ; do
        name=$(basename $file)
        if [[ ! -f $file || $name == *.dtb || $name == initramfs-* || $name == vmlinuz-* ]] ; then
            continue
        fi
        ln -sf loader/${name} sysroot/boot/${name}
    done

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
        echo "Packing initial deployment..."
        rm -rf ota-deploy
        mkdir ota-deploy/
        cd ota-deploy/
        tar -pcz${VERBOSE}f boot.tar.gz -C ../sysroot/boot/ .
        mv ../sysroot/boot/ ../boot/
        tar -pcz${VERBOSE}f rootfs.tar.gz -C ../sysroot/ .
        mv ../boot/ ../sysroot/boot/
    fi
}

extract_sysroot()
{
    echo "Extracting sysroot..."
    rm -rf ${GENERATED_TREE}
    mkdir ${GENERATED_TREE}
    tar -C ${GENERATED_TREE} -x${VERBOSE}f ${SYSROOT_IMAGE_PATH}/rootfs.tar.gz
    tar -C ${GENERATED_TREE} -x${VERBOSE}f ${SYSROOT_IMAGE_PATH}/b2qt.tar.gz
    mkdir -p ${BOOT_FILE_PATH}/
    tar -C ${BOOT_FILE_PATH}/ -x${VERBOSE}f ${SYSROOT_IMAGE_PATH}/boot.tar.gz
}

start_httpd_server()
{
    # Start a trivial httpd server.
    # TODO - allow starting on existing repo
    if [ ! -d ${SERVER_ROOT} ] ; then
        mkdir ${SERVER_ROOT}
        cd ${SERVER_ROOT}
        ln -s ${OSTREE_REPO} ostree
        ostree trivial-httpd --autoexit --daemonize -p ${SERVER_ROOT}/httpd-port
        PORT=$(cat ${SERVER_ROOT}/httpd-port)
        echo "http://127.0.0.1:${PORT}" > ${SERVER_ROOT}/httpd-address
    fi
}

main()
{
    parse_args $@

    extract_sysroot

    organize_boot_files
    convert_b2qt_to_ostree
    commit_generated_tree

    if [ $OTA_SYSROOT = true ] ; then
        create_initial_deployment
    fi

    start_httpd_server

    echo "Done."
}

main $@

