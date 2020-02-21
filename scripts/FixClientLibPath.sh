#!/bin/bash

# Usage: FixClientLibPath.sh [path to client.so]

# Purpose: for some reason ld writes full absolute path to .so to the resulting ELF binary.
# That causes dynamic linker to look for that file in the location it was when compiling
# instead of the game directory.
#
# Example:
# $ ldd client.so
# ...
# /home/tmp64/projects/HL_Dev/BugfixedHL/lib/public/libtier0.so (0xf7900000)
# libtier0.so => not found
# ...
# See that full path?
# This script replaces it with just lib_name.so (libtier0.so in that case)

LIB=$1

if [ -z "$LIB" ]; then
    echo "Usage: FixClientLibPath.sh [path to client.so]"
    echo "Purpose: replaces original path to vgui.so (e.g. ../lib/public/vgui.so) with generic 'vgui.so'.";
    echo "Requires patchelf to be installed."
    exit 1
fi

if [ -z "$PATCHELF" ]; then
    PATCHELF=$(which patchelf)

    if [ -z "$PATCHELF" ]; then
        echo "Error: 'patchelf' utility is not installed."
        echo "Set PATCHELF envvar to the correct path or"
        echo "use you default package manager to install it."
        exit 1
    fi
fi

if [ ! -f "$LIB" ]; then
    echo "${LIB}: no such file";
    exit 1
fi

# Fix vgui.so
ORIGINAL=$(ldd ${LIB} | grep -m 1 vgui.so | awk -F" " '{print $1}')
if [ ! -z "$ORIGINAL" ]; then
    REPLACEMENT="vgui.so"
    echo "[${LIB}] ${ORIGINAL} -> ${REPLACEMENT}"
    $PATCHELF --replace-needed ${ORIGINAL} ${REPLACEMENT} ${LIB}
fi

# Fix libtier0.so
ORIGINAL=$(ldd ${LIB} | grep -m 1 libtier0.so | awk -F" " '{print $1}')
if [ ! -z "$ORIGINAL" ]; then
    REPLACEMENT="libtier0.so"
    echo "[${LIB}] ${ORIGINAL} -> ${REPLACEMENT}"
    $PATCHELF --replace-needed ${ORIGINAL} ${REPLACEMENT} ${LIB}
fi

# Fix libvstdlib.so
ORIGINAL=$(ldd ${LIB} | grep -m 1 libvstdlib.so | awk -F" " '{print $1}')
if [ ! -z "$ORIGINAL" ]; then
    REPLACEMENT="libvstdlib.so"
    echo "[${LIB}] ${ORIGINAL} -> ${REPLACEMENT}"
    $PATCHELF --replace-needed ${ORIGINAL} ${REPLACEMENT} ${LIB}
fi

