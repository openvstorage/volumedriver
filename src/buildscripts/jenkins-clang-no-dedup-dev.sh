#! /bin/bash
set -eux
# test .. this file should be in `pwd`!!!


BUILDTOOLS_TO_USE=$(realpath ${WORKSPACE}/BUILDS/volumedriver-buildtools-5.0/rtchecked)

VOLUMEDRIVER_DIR=$(realpath $1)
. ${VOLUMEDRIVER_DIR}/src/buildscripts/get_revision.sh

BUILD_DIR=${VOLUMEDRIVER_DIR}/build
export RUN_TESTS=${RUN_TESTS:-"yes"}
export USE_MD5_HASH=no
export CLEAN_BUILD=yes
export RECONFIGURE_BUILD=yes
export TEMP=${TEMP:-${VOLUMEDRIVER_DIR}/tmp}
export FOC_PORT_BASE=${FOC_PORT_BASE:-19250}
export BUILD_DEBIAN_PACKAGES=no
export BUILD_DOCS=no
export SKIP_TESTS=
export SKIP_BUILD=no
export FAILOVERCACHE_TEST_PORT=${FOC_PORT_BASE}
export COVERAGE=nyet
export ARAKOON_BINARY=/usr/bin/arakoon
export ARAKOON_PORT_BASE=${ARAKOON_PORT_BASE:-$((FOC_PORT_BASE + 10))}
export VFS_PORT_BASE=${VFS_PORT_BASE:-$((FOC_PORT_BASE + 20))}
export USE_CLANG=yes
export VD_EXTRA_VERSION=`get_debug_extra_version $VOLUMEDRIVER_DIR`
export VOLUMEDRIVERFS_TEST_EXTRA_ARGS="--gtest_filter=-*chown*"

# adds
# * -Wno-mismatched-tags to disable "class X was previously declared as struct"
#   This is a pretty harmless (and actually perfectly valid) thing AFAICT.
# * -Wno-deprecated-register to disable "warning: 'register' storage class specifier
#    is deprecated"
#   We get a lot of these through boost and don't use it ourselves.
export CXX_WARNINGS="-Wall -Wextra -Wno-unknown-pragmas -Wctor-dtor-privacy -Wsign-promo -Woverloaded-virtual -Wnon-virtual-dtor -Wno-mismatched-tags -Wno-deprecated-register -Wno-unused-local-typedef -Wno-unused-parameter"

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
