#!/bin/bash

set -eu

# script found here
# https://github.com/openbmc/pldm/blob/master/tools/fw-update/pldm_fwup_pkg_creator.py

export IMAGE=miniblob.bin
export PACKAGE=updatepackage.bin

# creating random blob

dd if=/dev/urandom of=${IMAGE} bs=1M count=1

# wrapping blob in pldm fw update package

./pldm_fwup_pkg_creator.py ${PACKAGE} metadata-example.json ${IMAGE} ${IMAGE} ${IMAGE}

