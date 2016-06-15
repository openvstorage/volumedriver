#! /bin/bash
set -eux
# test .. this file should be in `pwd`!!!

export CXX_SETTINGS="-std=gnu++14 -fPIC -fprofile-arcs -ftest-coverage"
export LIBS="-lgcov"

BUILDTOOLS_TO_USE=$(realpath ${WORKSPACE}/BUILDS/volumedriver-buildtools-5.0/rtchecked)

VOLUMEDRIVER_DIR=$1

BUILD_DIR=${VOLUMEDRIVER_DIR}/build
export RUN_TESTS=nope
export USE_MD5_HASH=yes
export CLEAN_BUILD=yes
export RECONFIGURE_BUILD=yes
export TEMP=${TEMP:-${VOLUMEDRIVER_DIR}/tmp}
export FOC_PORT_BASE=${FOC_PORT_BASE:-19050}
export BUILD_DEBIAN_PACKAGES=nah
export BUILD_DOCS=no
export SKIP_BUILD=no
export SKIP_TESTS="c++ python"
export FAILOVERCACHE_TEST_PORT=32322
export COVERAGE=nyet
export ARAKOON_BINARY=/usr/bin/arakoon
export ARAKOON_PORT_BASE=${ARAKOON_PORT_BASE:-$((FOC_PORT_BASE + 10))}
export VFS_PORT_BASE=${VFS_PORT_BASE:-$((FOC_PORT_BASE + 20))}
export USE_CLANG_ANALYZER=yes
export VD_EXTRA_VERSION=clang-analyzer
export SUPPRESS_WARNINGS=yes

rm -rf ${TEMP}
mkdir -p ${TEMP}
mkdir -p ${BUILD_DIR}

ln -sf $VOLUMEDRIVER_DIR/src/buildscripts/builder.sh $BUILD_DIR

pushd $BUILD_DIR

# temporarily switch off error checking as we need to process the pylint output
# (see below)
set +e
./builder.sh $BUILDTOOLS_TO_USE $VOLUMEDRIVER_DIR
ret=$?
set -e

# the violations plugin wants a relative path - fix up the pylint output
find . -name \*_pylint.out -exec sed -i "s#${VOLUMEDRIVER_DIR}/##" {} \;

popd

exit ${ret}
