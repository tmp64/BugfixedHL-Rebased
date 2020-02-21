#!/bin/bash
# Usage: PublishLibs.sh [path list file] [files to copy...]

PATHS=$1

if [ ! -f "$PATHS" ]; then
    echo "No deployment path specified. Create file ${PATHS} with paths on separate lines for auto deployment."
    exit 0
fi

while IFS="" read -r p || [ -n "$f" ]
do
    echo "Deploying to: ${p}"

    FIRST=0
    mkdir -p "${p}"

    for file in "$@"
    do
        if [ "$FIRST" -eq 0 ]; then
            # Skip first argument which is list file name
            # I am bad in bash
            FIRST=1
            continue
        fi
        cp "${file}" "${p}"
    done

done < "$PATHS"
