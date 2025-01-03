#!/bin/bash
set -eo pipefail

# Check the /sys/class/watchdog/watchdog1/access_cs0 and tell if it's running on the primary or the secondary flash.

ACCESS_CS0="/sys/class/watchdog/watchdog1/access_cs0"
SLOT_FILE="/run/media/slot"

# Create directory if not exist
mkdir -p "$(dirname "${SLOT_FILE}")"

# Write slot info
if [ -f ${ACCESS_CS0} ]; then
    echo "1" > ${SLOT_FILE}
else
    echo "0" > ${SLOT_FILE}
fi
