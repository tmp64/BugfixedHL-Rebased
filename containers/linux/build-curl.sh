#!/bin/bash

set -e

IMAGE_TAG="bhl-build-curl"
TEMP_CONT="${IMAGE_TAG}-temp"
DOCKERFILE="Dockerfile.$IMAGE_TAG"
OUT_DIR="out/curl"

mkdir -p $OUT_DIR

docker buildx build -f $DOCKERFILE -t $IMAGE_TAG .
docker container create --name $TEMP_CONT $IMAGE_TAG
docker container cp ${TEMP_CONT}:/opt/bhl/prefix-out/. $OUT_DIR
docker container rm ${TEMP_CONT}

# Remove unnecessary stuff
rm -rf "$OUT_DIR/bin"
rm -rf "$OUT_DIR/share"

# Fix MbedTLS linking

# /opt/bhl/prefix/lib/libgnutls.a;/opt/bhl/prefix/lib32/libnettle.a
sed -i 's\/opt/bhl/prefix/lib/libgnutls.a\GnuTLS::GnuTLS\g' $OUT_DIR/lib/cmake/CURL/CURLTargets.cmake
sed -i 's\/opt/bhl/prefix/lib32/libnettle.a\\g' $OUT_DIR/lib/cmake/CURL/CURLTargets.cmake
