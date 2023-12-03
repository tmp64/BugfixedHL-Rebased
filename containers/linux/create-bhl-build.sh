#!/bin/bash

set -e

docker buildx build -f Dockerfile.bhl-build -t bhl-build .
