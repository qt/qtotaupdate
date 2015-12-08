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

if [ ! -x "$(which ostree)" ]
then
    echo "error: Needed command 'ostree' not found in PATH."
    exit 1
fi

ROOT=$(dirname $(readlink -f $0))
WORKDIR=$PWD

GENERATED_TREE=${WORKDIR}/tree
BOOT_FILE_PATH=${GENERATED_TREE}/boot
UBOOT_ENV_FILE=""
SERVER_ROOT=${WORKDIR}/httpd
OTA_SYSROOT=false
INVALID_ARGS=false
BINARY_IMAGE=false
VERBOSE=""
# REPO
OSTREE_REPO=${WORKDIR}/ostree-repo
OSTREE_BRANCH=qt/b2qt
OSTREE_COMMIT_SUBJECT=""
# TLS
USE_CLIENT_TLS=false
SERVER_CERT=""
CLIENT_CERT=""
CLIENT_KEY=""
TLS_CERT_PATH=${GENERATED_TREE}/usr/share/ostree/certs/
# GPG
USE_GPG=false
GPG_KEY=""
GPG_HOMEDIR=""
GPG_TRUSTED_KEYRING=""
GPG_KEYS_PATH=${GENERATED_TREE}/usr/share/ostree/trusted.gpg.d/
# PRIVATE
DO_B2QT_DEPLOY=true

usage()
{
    echo
    echo "Usage: $0 OPTIONS"

    echo
    echo "--sysroot-image-path DIR              A path to b2qt sysroot *.tar.gz or *.img image files."
    echo "--initramfs FILE                      OSTree boot compatible initramfs."
    echo "--uboot-env-file FILE                 OSTree boot compatible u-boot environment file."
    echo
    echo "--ostree-repo DIR                     Commits the generated tree into this repository. If the repository does not exist, "
    echo "                                      one is created in the specified location. If this argument is not provided, "
    echo "                                      a default repository is created in the working directory."
    echo "--ostree-branch os/branch-name        Commits the generated update in the specified OSTree branch. A default branch is qt/b2qt."
    echo "--ostree-commit-subject \"SUBJECT\"     Commits the generated update with the specified commit subject. A default commit subject is the"
    echo "                                      date and time when the update was committed."
    echo
    echo "--create-initial-deployment           Generates Over-The-Air Update ready sysroot."
    echo "--verbose                             Prints more information to the console."
    echo
    echo "--tls-ca-path FILE                    Path to file containing trusted anchors instead of the system CA database. Pins the certificate and"
    echo "                                      uses it for servers authentication."
    echo "--tls-client-cert-path FILE           Path to file for client-side certificate, to present when making requests to a remote repository."
    echo "--tls-client-key-path FILE            Path to file containing client-side certificate key, to present when making requests to a remote repository."
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
                echo "${arg} requires a directory path, but ${value} was provided."
                ;;
            f)
                echo "${arg} requires a path to a file, but ${value} was provided."
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
        --sysroot-image-path)
            count=$(ls ${value}/*.img 2> /dev/null | wc -l)
            if [ ${count} -gt 1 ] ; then
                valid=false
                error="Found ${count} *.img files in --sysroot-image-path ${value}/, but expected to find 1 *.img file."
            elif [ ${count} = 1 ] ; then
                BINARY_IMAGE=true
            else
                count=$(ls ${value}/*.tar.gz 2> /dev/null | wc -l)
                if [ ${count} = 0 ] ; then
                    valid=false
                    error="--sysroot-image-path ${value}/ must contain *.img or *.tar.gz sysroot image files."
                fi
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
              OSTREE_REPO=$(readlink -f ${2})
              shift 1
              ;;
          --uboot-env-file)
              UBOOT_ENV_FILE=$(readlink -f ${2})
              shift 1
              ;;
          --tls-ca-path)
              SERVER_CERT=$(readlink -f ${2})
              shift 1
              ;;
          --tls-client-cert-path)
              CLIENT_CERT=$(readlink -f ${2})
              USE_CLIENT_TLS=true
              shift 1
              ;;
          --tls-client-key-path)
              CLIENT_KEY=$(readlink -f ${2})
              USE_CLIENT_TLS=true
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
              SYSROOT_IMAGE_PATH=$(readlink -f ${2})
              shift 1
              ;;
          --initramfs)
              INITRAMFS=$(readlink -f ${2})
              shift 1
              ;;
          --gpg-sign)
              GPG_KEY=${2}
              USE_GPG=true
              shift 1
              ;;
          --gpg-homedir)
              GPG_HOMEDIR=$(readlink -f ${2})
              USE_GPG=true
              shift 1
              ;;
          --gpg-trusted-keyring)
              GPG_TRUSTED_KEYRING=$(readlink -f ${2})
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
    validate_arg "--tls-ca-path" "${SERVER_CERT}" false f
    validate_arg "--tls-client-cert-path" "${CLIENT_CERT}" false f
    validate_arg "--tls-client-key-path" "${CLIENT_KEY}" false f

    if [[ $USE_GPG = true && ( -z "${GPG_KEY}" || -z "${GPG_HOMEDIR}" ) ]] ; then
        update_invalid_args
        # Note: --gpg-homedir is not required when keyring is stored in a standard path,
        # but just to me sure that a commit won't fail, we require both of these args.
        echo "Must specify both --gpg-sign and --gpg-homedir for GPG signing feature."
    fi
    if [[ $USE_CLIENT_TLS = true && ( -z "${CLIENT_CERT}" || -z "${CLIENT_KEY}" ) ]] ; then
        update_invalid_args
        echo "Must specify both --tls-client-cert-path and --tls-client-key-path for TLS client authentication feature."
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
            return 0
        fi
    done
    echo "error: Failed to find kernel image in ${BOOT_FILE_PATH}."
    exit 1
}

organize_boot_files()
{
    cp ${INITRAMFS} ${BOOT_FILE_PATH}/initramfs
    find_and_rename_kernel
    if [ -n "${UBOOT_ENV_FILE}" ] ; then
        cp ${UBOOT_ENV_FILE} ${BOOT_FILE_PATH}/
        name=$(basename ${UBOOT_ENV_FILE})
        if [ "${name}" != "uEnv.txt" ] ; then
            cd ${BOOT_FILE_PATH}
            # Must be a boot script then.
            if file -b $name | grep -qi "ASCII text"; then
                echo "Compiling u-boot boot script: ${BOOT_FILE_PATH}/${name} ..."
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
    if [ ! -e ${GENERATED_TREE}/etc/os-release ] ; then
        echo "PRETTY_NAME=\"Boot 2 Qt\"" > ${GENERATED_TREE}/etc/os-release
    fi

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

    systemd=false
    if [[ -e lib/systemd/systemd ]] ; then
        systemd=true
        echo "Detected systemd support on the image."
    fi
    if [ ${systemd} = false ] ; then
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
    fi

    # Add trusted GPG keyring file.
    if [ -n "${GPG_TRUSTED_KEYRING}" ] ; then
        cp ${GPG_TRUSTED_KEYRING} ${GPG_KEYS_PATH}
    fi

    # Enable TLS support.
    mkdir -p ${TLS_CERT_PATH}
    if [ -n "${SERVER_CERT}" ] ; then
        cp ${SERVER_CERT} ${TLS_CERT_PATH}
    fi
    if [ $USE_CLIENT_TLS = true ] ; then
        cp ${CLIENT_CERT} ${CLIENT_KEY} ${TLS_CERT_PATH}
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
    echo "Committing the generated tree into a repository at ${OSTREE_REPO} ..."
    if [ $USE_GPG = true ] ; then
        ostree --repo=${OSTREE_REPO} commit -b ${OSTREE_BRANCH} -s "${OSTREE_COMMIT_SUBJECT}" --gpg-sign="${GPG_KEY}" --gpg-homedir="${GPG_HOMEDIR}"
    else
        ostree --repo=${OSTREE_REPO} commit -b ${OSTREE_BRANCH} -s "${OSTREE_COMMIT_SUBJECT}"
    fi

    ostree --repo=${OSTREE_REPO} fsck
}

create_initial_deployment()
{
    echo "Preparing initial deployment in ${WORKDIR}/sysroot/ ..."
    cd ${WORKDIR}
    rm -rf sysroot

    mkdir -p sysroot/sysroot/
    export OSTREE_SYSROOT=sysroot
    ostree admin init-fs sysroot
    ostree admin os-init b2qt
    # Tell OSTree to use U-Boot boot loader backend.
    mkdir -p sysroot/boot/loader.0/
    ln -s loader.0 sysroot/boot/loader
    touch sysroot/boot/loader/uEnv.txt
    ln -s loader/uEnv.txt sysroot/boot/uEnv.txt
    # Add convenience symlinks, for details see ostree/src/libostree/ostree-bootloader-uboot.c
    for file in ${BOOT_FILE_PATH}/* ; do
        name=$(basename $file)
        if [[ ! -f $file || $name == *.dtb || $name == initramfs-* || $name == vmlinuz-* ]] ; then
            continue
        fi
        ln -sf loader/${name} sysroot/boot/${name}
    done

    ostree --repo=sysroot/ostree/repo pull-local --remote=b2qt ${OSTREE_REPO} ${OSTREE_BRANCH}
    ostree admin deploy --os=b2qt b2qt:${OSTREE_BRANCH}
    ostree admin status

    # OSTree does not touch the contents of /var
    rm -rf sysroot/ostree/deploy/b2qt/var/
    cp -R ${GENERATED_TREE}/var/ sysroot/ostree/deploy/b2qt/
    # Pack the OTA ready sysroot.
    if [ $DO_B2QT_DEPLOY = true ] ; then
        echo "Packing initial deployment ..."
        rm -rf boot.tar.gz rootfs.tar.gz
        tar -pcz${VERBOSE}f boot.tar.gz -C sysroot/boot/ .
        mv sysroot/boot/ .
        tar -pcz${VERBOSE}f rootfs.tar.gz -C sysroot/ .
        mv boot/ sysroot/
        if [ "${ROOT}" != "${WORKDIR}" ] ; then
            cp ${ROOT}/deploy.sh ${ROOT}/mkcard.sh ${WORKDIR}/
        fi
    fi
}

extract_sysroot()
{
    rm -rf ${GENERATED_TREE}
    mkdir ${GENERATED_TREE}/
    mkdir ${BOOT_FILE_PATH}/

    if [ $BINARY_IMAGE = true ] ; then
        # Extract binary image.
        image=$(ls ${SYSROOT_IMAGE_PATH}/*.img)
        echo "Extracting ${image} ..."
        units=$(fdisk -l ${image} | grep Units | awk '{print $(NF-1)}')
        # The boot partition not always is marked properly.
        boot_start=$(fdisk -l ${image} | grep ${image}1 | awk '{print $2}')
        if [ "${boot_start}" == "*" ] ; then
            boot_start=$(fdisk -l ${image} | grep ${image}1 | awk '{print $3}')
        fi
        rootfs_start=$(fdisk -l ${image} | grep ${image}2 | awk '{print $2}')
        boot_offset=$(expr ${units} \* ${boot_start})
        rootfs_offset=$(expr ${units} \* ${rootfs_start})

        cd ${WORKDIR}/
        if [ -d boot-mount ] ; then
            # Ignore return code.
            sudo umount boot-mount || /bin/true
            rm -rf boot-mount
        fi
        if [ -d rootfs-mount ] ; then
            # Ignore return code.
            sudo umount rootfs-mount || /bin/true
            rm -rf rootfs-mount
        fi

        mkdir boot-mount rootfs-mount
        sudo mount -o loop,offset=${boot_offset} ${image} boot-mount/
        sudo mount -o loop,offset=${rootfs_offset} ${image} rootfs-mount/
        sudo cp -r boot-mount/* ${BOOT_FILE_PATH}
        sudo cp -r rootfs-mount/* ${GENERATED_TREE}
        sudo umount boot-mount
        sudo umount rootfs-mount
        sudo chown -R $(id -u):$(id -g) ${GENERATED_TREE}
        rm -rf boot-mount rootfs-mount
    else
        # Extract *.tar.gz image files.
        for image in ${SYSROOT_IMAGE_PATH}/*.tar.gz ; do
            echo "Extracting ${image} ..."
            if [[ $(basename ${image}) == *boot* ]] ; then
                tar -C ${BOOT_FILE_PATH} -x${VERBOSE}f ${image}
            else
                tar -C ${GENERATED_TREE} -x${VERBOSE}f ${image}
            fi
        done
    fi
}

start_httpd_server()
{
    # Start a trivial httpd server on localhost.
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
    parse_args "$@"

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

main "$@"
