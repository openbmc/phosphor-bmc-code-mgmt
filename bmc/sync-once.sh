#!/bin/bash

# Sync the files/dirs specified in synclist once
# Usually the sync-manager could sync the file once before it starts, so that
# it makes sure the synclist is always synced when the sync-manager is running.

SYNCLIST=/etc/synclist
DEST_DIR=/run/media/rwfs-alt/cow

while read -r l; do

    # if the sync entry is not present in the source, remove it from the destination
    if [ -n "${l}" ] && [ ! -e "${l}" ] && [ -e "${DEST_DIR}/${l}" ]; then
        echo "Removing ${DEST_DIR}/${l}"
        rm -rf "${DEST_DIR:?}/${l:?}"
        continue
    fi

    echo rsync -a -R --delete "${l}" "${DEST_DIR}"
    rsync -a -R --delete "${l}" "${DEST_DIR}"
done < ${SYNCLIST}
