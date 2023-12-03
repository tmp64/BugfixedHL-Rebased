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

# Fix MbedTLS linking
sed -i 's\/opt/bhl/prefix/lib/libmbedtls.a\MbedTLS::mbedtls\g' $OUT_DIR/lib/cmake/CURL/CURLTargets.cmake
sed -i 's\/opt/bhl/prefix/lib/libmbedx509.a\MbedTLS::mbedx509\g' $OUT_DIR/lib/cmake/CURL/CURLTargets.cmake
sed -i 's\/opt/bhl/prefix/lib/libmbedcrypto.a\MbedTLS::mbedcrypto\g' $OUT_DIR/lib/cmake/CURL/CURLTargets.cmake
