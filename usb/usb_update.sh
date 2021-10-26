#!/bin/bash

set -e

filename=$1

systemctl --no-block start "USBCodeUpdate@$filename.service"
