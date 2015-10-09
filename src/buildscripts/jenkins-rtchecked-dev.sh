#! /bin/bash
set -eux
# test .. this file should be in `pwd`!!!

export CXX_SETTINGS="-std=gnu++14 -fPIC -fprofile-arcs -ftest-coverage"
export LIBS="-lgcov"

BUILDTOOLS_TO_USE=$(realpath ${WORKSPACE}/BUILDS/volumedriver-buildtools-5.0/rtchecked)

VOLUMEDRIVER_DIR=$1

. ${VOLUMEDRIVER_DIR}/src/buildscripts/get_revision.sh

BUILD_DIR=${VOLUMEDRIVER_DIR}/build
export RUN_TESTS=yes
export CLEAN_BUILD=yes
export RECONFIGURE_BUILD=yes
export TEMP=${TEMP:-${VOLUMEDRIVER_DIR}/tmp}
export FOC_PORT_BASE=${FOC_PORT_BASE:-19200}
export BUILD_DEBIAN_PACKAGES=no
export BUILD_DOCS=no
export SKIP_BUILD=no
export SKIP_TESTS="pylint"
export FAILOVERCACHE_TEST_PORT=${FOC_PORT_BASE}
export COVERAGE=yes
export ARAKOON_BINARY=/usr/bin/arakoon
export ARAKOON_PORT_BASE=${ARAKOON_PORT_BASE:-$((FOC_PORT_BASE + 10))}
export VFS_PORT_BASE=${VFS_PORT_BASE:-$((FOC_PORT_BASE + 20))}
export MDS_PORT_BASE=${MDS_PORT_BASE:-$((VFS_PORT_BASE + 20))}
export CXX_WARNINGS="-Wall -Wextra -Wno-unknown-pragmas -Wsign-promo -Woverloaded-virtual -Wnon-virtual-dtor"
export VD_EXTRA_VERSION=`get_debug_extra_version $VOLUMEDRIVER_DIR`

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
