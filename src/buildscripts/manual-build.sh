#!/bin/bash 

#########################################################################################################
# various build settings; you may want to make changes

export BUILD_TYPE=release             # set to 'rtchecked' for debug builds
export RUN_TESTS=no                   # set to "yes" to run included test suite (needs running rpcbind,
                                      # redis & omniNames + installed arakoon & alba; see docs!)
export BUILD_DEBIAN_PACKAGES=yes      # both for deb & rpm; change to "no" to skip creating packages
export CLEAN_BUILD=no                 # set to "yes" to clean build env (force complete rebuild)
export RECONFIGURE_BUILD=yes          # do reconfigure the code
export USE_MD5_HASH=yes               # set to "no" to disable deduping
export BUILD_NUM_PROCESSES=2          # number of concurrent build processes (make -j)
export SUPPRESS_WARNINGS=no
export COVERAGE=no

export CXX_WARNINGS="-Wall -Wextra -Wno-unknown-pragmas -Wsign-promo -Woverloaded-virtual -Wnon-virtual-dtor"

if [ "${BUILD_TYPE}" != "rtchecked" ]
then
  export CXX_OPTIMIZE_FLAGS="-ggdb3 -O2"
  export CXX_DEFINES="-DNDEBUG -DBOOST_FILESYSTEM_VERSION=3"
fi

## settings for the included testsuite; only needed if RUN_TESTS=yes
export ARAKOON_BINARY=/usr/bin/arakoon
export FOC_PORT_BASE=19100
export FAILOVERCACHE_TEST_PORT=${FOC_PORT_BASE}
export ARAKOON_PORT_BASE=$((FOC_PORT_BASE + 10))
export VFS_PORT_BASE=$((FOC_PORT_BASE + 20))
export MDS_PORT_BASE=$((VFS_PORT_BASE + 20))

#########################################################################################################
# locations of required libs & scripts; these should be ok

THIS_SCRIPT="$(readlink -f $0)"
TOPSRCDIR=${THIS_SCRIPT%/volumedriver/src/*}
BUILDTOOLS_TO_USE="${TOPSRCDIR}/BUILDS/volumedriver-buildtools/${BUILD_TYPE}"
VOLUMEDRIVER_DIR="${TOPSRCDIR}/volumedriver"  # must be a full path, not a relative one!

BUILD_DIR="${TOPSRCDIR}/BUILDS/volumedriver/${BUILD_TYPE}"
BUILDER="${VOLUMEDRIVER_DIR}/src/buildscripts/builder.sh"

## quick checks to make sure everything is in place
if [ ! -d "${BUILDTOOLS_TO_USE}" ]
then
  echo "FATAL: The directory ${BUILDTOOLS_TO_USE} with required libs does not exist;"
  echo "Did you build the volumedriver-buildtools?"
  exit 1
fi

if [ ! -x "${BUILDER}" ]
then 
  echo "FATAL: buildscript ${BUILDER} not found or not executable; cannot continue..."
  exit 1
fi

#########################################################################################################
## finally: building things...

set -eux 

mkdir -p ${BUILD_DIR}
ln -sf ${BUILDER} ${BUILD_DIR}

pushd ${BUILD_DIR}

set +e
${BUILDER} ${BUILDTOOLS_TO_USE} ${VOLUMEDRIVER_DIR}
ret=$?
set -e

popd

exit ${ret}
