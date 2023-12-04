#!/bin/bash

set -e

CUR_DIR="containers/linux"

IMAGE_TAG="bhl-build-bhl"
TEMP_CONT="${IMAGE_TAG}-temp"
DOCKERFILE="Dockerfile.linux"
OUT_DIR="$CUR_DIR/out/bhl"

cd ../..
mkdir -p $OUT_DIR

docker buildx build -f $DOCKERFILE -t $IMAGE_TAG --build-arg BHL_VER_TAG .
docker container create --name $TEMP_CONT $IMAGE_TAG
docker container cp ${TEMP_CONT}:/build/bhl/_build_out_client/. $OUT_DIR
docker container cp ${TEMP_CONT}:/build/bhl/_build_out_server/. $OUT_DIR
docker container rm ${TEMP_CONT}
