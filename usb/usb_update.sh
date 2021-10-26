#!/bin/bash

filename=$1
systemctl --no-block start "USBCodeUpdate@$filename.service"
