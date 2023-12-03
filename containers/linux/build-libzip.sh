#!/bin/bash

set -e

IMAGE_TAG="bhl-build-libzip"
TEMP_CONT="${IMAGE_TAG}-temp"
DOCKERFILE="Dockerfile.$IMAGE_TAG"
OUT_DIR="out/libzip"

mkdir -p $OUT_DIR

docker buildx build -f $DOCKERFILE -t $IMAGE_TAG .
docker container create --name $TEMP_CONT $IMAGE_TAG
docker container cp ${TEMP_CONT}:/opt/bhl/prefix-out/. $OUT_DIR
docker container rm ${TEMP_CONT}
