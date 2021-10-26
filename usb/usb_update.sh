#!/bin/bash

usb_dir="/run/media/usb/$1"
src_dir="/dev/$1"

if [ ! -d "${usb_dir}" ]; then
    mkdir -p "${usb_dir}"
fi

mount "${src_dir}" "${usb_dir}"

systemctl start "usb-code-update@$1.service"
