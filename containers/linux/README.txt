Build order:
0. rm -rf out
1. create-bhl-build.sh
2. build-zlib.sh
3. build-libzip.sh
4. build-gnutls.sh
4. build-curl.sh
5. build-bhl.sh

To open bash
  docker run --rm -it --entrypoint bash bhl-build
