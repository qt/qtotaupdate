/usr/sbin/ostree-prepare-root /sysroot

# HACK !? https://mail.gnome.org/archives/ostree-list/2015-July/msg00015.html
# ostree binary expects to find bootloader configs in /boot/ but at this point
# the mounted /boot contains what we committed in the "tree" (vmlinuz-sha256
# and initramfs-sha256). We mount our real boot partition on that path.
mount -t ext2 /dev/disk/by-label/boot /sysroot/boot
