#! /bin/bash
set -eux
# test .. this file should be in `pwd`!!!

# no valgrind invocations here - the jenkins valgrind plugin does that.

BUILDTOOLS_TO_USE=$(realpath ${WORKSPACE}/BUILDS/volumedriver-buildtools-5.0/rtchecked)

VOLUMEDRIVER_DIR=$1

BUILD_DIR=${VOLUMEDRIVER_DIR}/build
export RUN_TESTS=no
export USE_MD5_HASH=no
export TEMP=${TEMP:-${VOLUMEDRIVER_DIR}/tmp}
export FOC_PORT_BASE=${FOC_PORT_BASE:-19000}
export VFS_PORT_BASE=${VFS_PORT_BASE:-$((FOC_PORT_BASE + 20))}

export CLEAN_BUILD=yes
export RECONFIGURE_BUILD=yes
export BUILD_DEBIAN_PACKAGES=no
export BUILD_DOCS=no
export SKIP_BUILD=no
export REPORTS=${WORKSPACE}/report
export VD_EXTRA_VERSION=valgrind

rm -rf ${TEMP}
mkdir -p ${TEMP}

rm -rf ${REPORTS}
mkdir -p ${REPORTS}

mkdir -p ${BUILD_DIR}

ln -sf $VOLUMEDRIVER_DIR/src/buildscripts/builder.sh $BUILD_DIR

pushd $BUILD_DIR
./builder.sh $BUILDTOOLS_TO_USE $VOLUMEDRIVER_DIR

popd
