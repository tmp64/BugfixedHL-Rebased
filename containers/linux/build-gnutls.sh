#!/bin/bash

set -e

IMAGE_TAG="bhl-build-gnutls"
TEMP_CONT="${IMAGE_TAG}-temp"
DOCKERFILE="Dockerfile.$IMAGE_TAG"
OUT_DIR="out/gnutls"

mkdir -p "$OUT_DIR"

docker buildx build -f $DOCKERFILE -t $IMAGE_TAG .
docker container create --name $TEMP_CONT $IMAGE_TAG
docker container cp ${TEMP_CONT}:/opt/bhl/prefix-out/. $OUT_DIR
docker container rm ${TEMP_CONT}

# Remove symlinks
# find "$OUT_DIR" -type l | xargs rm --

# Remove binaries
rm -rf "$OUT_DIR/bin"
