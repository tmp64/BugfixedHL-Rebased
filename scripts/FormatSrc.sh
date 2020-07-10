#!/bin/bash
# This script formats all sources in /src/ directory using clang-format.

# Get repo dir
REPO_ROOT=$(git rev-parse --show-toplevel);

# Format *.c, *.cpp, *.h
find "${REPO_ROOT}/src" -iname *.h -o -iname *.cpp -o -iname *.c | xargs clang-format --style=file -i
