#!/bin/sh

set -e

CMAKE_CURRENT_BINARY_DIR=$(pwd)

# If executed from non-interactive terminal (default when gitlab CI builds)
# we have none or limited PATH. Make sure we get one, or we cannot find pytest
[ -z "$(which pytest)" ] && export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
[ -z "$(which pytest)" ] && exit "pytest binary not found"

# Do not write bytecode, creates problem if running local dev and in-container dev from same source
export PYTHONDONTWRITEBYTECODE=x

# Ensure we can find our python library
export PYTHONPATH=$(dirname ${CMAKE_CURRENT_BINARY_DIR})/python/

# If there where no args, auto-add python/ dir of this scripts dir, normally in source tree.
# Assumes full path was given to script
ARGS="$*"
[ -z "$ARGS" ] && ARGS="-v $(dirname $0)/python"

# Use explicit cachedir in $(pwd), which should be the build-dir when executed from CMake
exec pytest -o cache_dir=$(pwd)/.pytest_cache $ARGS

