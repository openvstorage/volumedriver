#! /bin/bash
set -eux
# test .. this file should be in `pwd`!!!

export CXX_OPTIMIZE_FLAGS="-ggdb3 -O2"
export CXX_DEFINES="-DNDEBUG -DBOOST_FILESYSTEM_VERSION=3"

BUILDTOOLS_TO_USE=$(realpath ${WORKSPACE}/BUILDS/volumedriver-buildtools-5.0/release)

VOLUMEDRIVER_DIR=$1

. ${VOLUMEDRIVER_DIR}/src/buildscripts/get_revision.sh
BUILD_DIR=${VOLUMEDRIVER_DIR}/build
export RUN_TESTS=yes
export CLEAN_BUILD=yes
export RECONFIGURE_BUILD=yes
export TEMP=${TEMP:-${VOLUMEDRIVER_DIR}/tmp}
export FOC_PORT_BASE=${FOC_PORT_BASE:-19100}
export BUILD_DEBIAN_PACKAGES=yes
export BUILD_QSHELL_PYTHON=no
export BUILD_DOCS=no
export SKIP_BUILD=no
export SKIP_TESTS=
export FAILOVERCACHE_TEST_PORT=${FOC_PORT_BASE}
export WITH_ARAKOON_METADATA_STORE=no
export ARAKOON_BINARY=/usr/bin/arakoon
export ARAKOON_PORT_BASE=${ARAKOON_PORT_BASE:-$((FOC_PORT_BASE + 10))}
export ARAKOON_VERSION=$(${ARAKOON_BINARY} --version | grep version | cut -f 2 -d ' ')
export VFS_PORT_BASE=${VFS_PORT_BASE:-$((FOC_PORT_BASE + 20))}
export MDS_PORT_BASE=${MDS_PORT_BASE:-$((VFS_PORT_BASE + 20))}
export CXX_WARNINGS="-Wall -Wextra -Wno-unknown-pragmas -Wsign-promo -Woverloaded-virtual -Wnon-virtual-dtor"
export SUPPRESS_WARNINGS=yes
export VD_EXTRA_VERSION=`get_release_extra_version $VOLUMEDRIVER_DIR`

#oooh this is dangerous...
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
