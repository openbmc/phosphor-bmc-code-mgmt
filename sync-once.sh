#!/bin/bash

# Sync the files/dirs specified in synclist once
# Usually the sync-manager could sync the file once before it starts, so that
# it makes sure the synclist is always synced when the sync-manager is running.

SYNCLIST=/etc/synclist
DEST_DIR=/run/media/rwfs-alt/cow

while read l; do
    echo rsync -a -R "${l}" "${DEST_DIR}"
    rsync -a -R "${l}" "${DEST_DIR}"
done < ${SYNCLIST}
