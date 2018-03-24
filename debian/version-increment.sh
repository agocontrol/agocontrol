#!/bin/bash
ORIG_PWD=$(pwd)
if [ ! -z "$SRC_DIR" ]; then
    cd "$SRC_DIR"
fi
NUMCOMMIT=$(git rev-list --count HEAD)
GITSHORT=$(git rev-parse --short HEAD)

cd $ORIG_PWD

dch -v 2.0.${NUMCOMMIT}.${GITSHORT} "new package from ${1}"
dch -r "unstable"
VERSION=$(dpkg-parsechangelog  | grep ^Version | sed 's/^Version: //g')
echo "#define AGOCONTROL_VERSION \"${VERSION}\"" > version.h
