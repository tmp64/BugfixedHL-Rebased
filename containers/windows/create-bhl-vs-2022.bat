@echo off
docker build ^
    -t bhl-vs-2022 ^
    -f .\Dockerfile.bhl-vs-2022 ^
    -m 4GB ^
    .
