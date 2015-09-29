#! /bin/bash
#! /bin/bash
set -ex
set -o pipefail

#configurables
BUILDTOOLS="${1?}"

if [ -d $BUILDTOOLS ]
then
    echo "Building with buildtools at $BUILDTOOLS"
else
    echo "Couldn't find $BUILDTOOLS, exiting"
    exit
fi

VOLUMEDRIVER_DIR=$(realpath ${2?})

if [ -d $VOLUMEDRIVER_DIR ]
then
    echo "Building volumedriver at $VOLUMEDRIVER_DIR"
else
    echo "Couldn't find $VOLUMEDRIVER_DIR, exiting"
    exit
fi

VOLUMEDRIVER_BUILD_DIR=$(realpath ${3?})
if [ -d $VOLUMEDRIVER_BUILD_DIR ]
then
    echo "volumedriver build dir at $VOLUMEDRIVER_BUILD_DIR"
else
    echo "Couldn't find $VOLUMEDRIVER_DIR, exiting"
    exit
fi


VOLUMEDRIVER_DATA_DIR=$(realpath ${4?})

if [ -d $VOLUMEDRIVER_DATA_DIR ]
then
    echo "volumedriver data dir at $VOLUMEDRIVER_DATA_DIR"
else
    echo "Couldn't find $VOLUMEDRIVER_DATA_DIR, exiting"
    exit
fi

#number of parallel makes to run
PALLALLELLIZATION=${BUILD_NUM_PROCESSES:-6}


# less configurables
PREFIX=$(pwd)
COMPONENTS="youtils backend persistent_cache volumedriver xmlrpc++0.7 VolumeTester kernel daemon"

export PKG_CONFIG_PATH=$BUILDTOOLS/lib/pkgconfig:$PREFIX/lib/pkgconfig

CXX=${BUILDTOOLS?}/bin/vd-g++
CC=${BUILDTOOLS?}/bin/vd-gcc

LDFLAGS=${LDFLAGS:-""}
CFLAGS=${CFLAGS:-""}

CXX_INCLUDES=${CXX_INCLUDES:-""}
CXX_WARNINGS=${CXX_WARNINGS:-"-Wall -Wextra -Wno-unknown-pragmas -Wctor-dtor-privacy -Wsign-promo -Woverloaded-virtual -Wnon-virtual-dtor"}
CXX_SETTINGS=${CXX_SETTINGS:-"-std=gnu++0x -fPIC"}
CXX_DEFINES=${CXX_DEFINES:-"-DBOOST_FILESYSTEM_VERSION=3"}
CXX_OPTIMIZE_FLAGS=${CXX_OPTIMIZE_FLAGS:-"-ggdb3 -gdwarf-3 -Og"}

CXXFLAGS="$CXX_INCLUDES $CXX_WARNINGS $CXX_SETTINGS $CXX_DEFINES $CXX_OPTIMIZE_FLAGS"

echo "Configuring the build"

autoreconf -isv .
VOLUMEDRIVER_BUILD_DIR=${VOLUMEDRIVER_BUILD_DIR?} \
    VOLUMEDRIVER_DIR=${VOLUMEDRIVER_DIR?} \
    VOLUMEDRIVER_DATA_DIR=${VOLUMEDRIVER_DATA_DIR?} \
    CXX=${CXX?} CC=${CC?} CFLAGS=${CFLAGS?} \
    CXXFLAGS=${CXXFLAGS?} LDFLAGS=${LDFLAGS} ./configure --config-cache --prefix=$PREFIX
## Local Variables: **
## compile-command: "time ./builder.sh" **
## End: **
