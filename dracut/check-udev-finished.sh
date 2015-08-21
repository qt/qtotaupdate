if [ -b /dev/disk/by-label/boot ] ; then
    exit 0
fi

exit 1
