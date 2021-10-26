#!/bin/bash

set -e

usbpath=$1

if [ -z "$usbpath" ]
then
    exit -1
fi

systemctl --no-block "USBCodeUpdate@$usbpath.service"

