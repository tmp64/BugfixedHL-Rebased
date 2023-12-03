@echo off
docker build ^
    -t bhl-build ^
    -f .\Dockerfile.bhl-build ^
    .
