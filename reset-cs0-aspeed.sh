#!/bin/sh

# Write 1 to /sys/class/watchdog/watchdog1/access_cs0 to reset the boot code
# selection and reset the chip select, so that the next boot is from the
# primary chip.
# This needs to be done in the shutdown script after umount the rootfs,
# otherwise the filesystem will get error because the content is switched
# back to CS0 chip.

sed -i "s/echo Remaining mounts/echo 1 > \/sys\/class\/watchdog\/watchdog1\/access_cs0\\necho Remaining mounts/" /run/initramfs/shutdown
