#!/bin/bash
set -e

BUILD_CONCURRENCY=${BUILD_CONCURRENCY:-6}

# Helper script to initialize first build on container
if [ "$1" = "--manual-build-and-wait" ]; then
    # Custom paths when running from docker-compose
    export CI_PROJECT_DIR=/app
    export WORKDIR=/build

    # Starting build in background job, leaving this script idling
    $0 &
    while true; do
        sleep 1
    done
    exit 0
elif [ -z "${CI_PROJECT_DIR}" ]; then
    echo "Only expected to run from GitLab CI, or manually using $0 --manual-build-and-wait"
    exit 1
else
    set -x
    # On Gitlab we want to build under project dir to be able to upload resulting artifacts,
    # but on local build we want to build ouf of src dir (which is volume-mounted) from host.
    export WORKDIR=${WORKDIR:-${CI_PROJECT_DIR}/build}

    # Buildvars
    export DEBEMAIL="${GITLAB_USER_EMAIL:-manual@example.com}"
    export DEBFULLNAME="${GITLAB_USER_NAME:-manual run}"

    # Let's build inside ./bin, then .deb files will be placed in ../ aka WORKDIR
    mkdir -p ${WORKDIR}/bin
    cd ${WORKDIR}/bin

    # Copy debian control files from src
    cp -r ${CI_PROJECT_DIR}/debian ${WORKDIR}/bin/

    # Set build version
    SRC_DIR="${CI_PROJECT_DIR}" ./debian/version-increment.sh

    # Manually run CMake targeting our source dir
    cmake ${CI_PROJECT_DIR} -DCMAKE_BUILD_TYPE=Debug

    # -i    ???
    # -nc   do not clean before build
    # -us   Do not sign the source package.
    # -uc   Do not sign the .changes file.
    # -b    binary build
    debuild -i -nc -us -uc -b -j${BUILD_CONCURRENCY} ${DEBUILD_EXTRA_ARGS}

    echo "Build finished"
fi
