#! /bin/bash
set -ex
set -o pipefail
# We should use [[ ]] everywhere for test constructs here
# http://mywiki.wooledge.org/BashFAQ/031
#configurables
BUILDTOOLS="${1-/home/immanuel/workspace/volumedriver-buildtools-root-release}"

if [ -d $BUILDTOOLS ]
then
    echo "Building with buildtools at $BUILDTOOLS"
else
    echo "Couldn't find $BUILDTOOLS, exiting"
    exit
fi

VOLUMEDRIVER_DIR=$(realpath ${2-../../volumedriver-core})

if [ -d $VOLUMEDRIVER_DIR ]
then
    echo "Building volumedriver at $VOLUMEDRIVER_DIR"
else
    echo "Couldn't find $VOLUMEDRIVER_DIR, exiting"
    exit
fi

PREFIX=$(pwd)

#number of parallel makes to run
PALLALLELLIZATION=${BUILD_NUM_PROCESSES:-6}

# less configurables
COMPONENTS="youtils backend persistent_cache volumedriver xmlrpc++0.7 VolumeTester kernel daemon"

export PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig:$BUILDTOOLS/lib/pkgconfig

if [ "x${USE_RTAGS}" == "xyes" ]
then
    echo 'not yet supported'; exit 1
    CXX=${BUILDTOOLS?}/bin/rtags-g++
    CC=${BUILDTOOLS?}/bin/rtags-gcc
    CPP=${BUILDTOOLS?}/bin/rtags-cpp
elif [ "x${USE_CLANG}" == "xyes" ]
then
    CXX=/usr/bin/clang++
    CC=/usr/bin/clang
    #LDFLAGS="-static-libgcc"

    CLANG_MAJOR=$(echo __clang_major__ | ${CC} -E -x c - | tail -n 1)
    CLANG_MINOR=$(echo __clang_minor__ | ${CC} -E -x c - | tail -n 1)

    if [ ! -x ${CC} -o ! -x ${CXX} ]
    then
      echo "${CC} or ${CXX} not executable!"
      exit 1
    fi

    if [ ${CLANG_MAJOR} -lt 3 ] || [ ${CLANG_MAJOR} -eq 3 -a ${CLANG_MINOR} -lt 5 ]
    then
      echo  "${CC} too old, need at least 3.5 installed"
      exit 1
    fi

    CLANG_MAJOR=$(echo __clang_major__ | ${CXX} -E -x c - | tail -n 1)
    CLANG_MINOR=$(echo __clang_minor__ | ${CXX} -E -x c - | tail -n 1)

    if [ ${CLANG_MAJOR} -lt 3 ] || [ ${CLANG_MAJOR} -eq 3 -a ${CLANG_MINOR} -lt 5 ]
    then
      echo  "${CXX} too old, need at least 3.5 installed"
      exit 1
    fi
elif [ "x${USE_CTAGS}" == "xyes" ]
then
    echo 'not yet supported'; exit 1
    CXX=${BUILDTOOLS?}/bin/tag-compiler.sh
    CC=${BUILDTOOLS?}/bin/vd-gcc
    CPP=${BUILDTOOLS?}/bin/vd-cpp
else
    CXX=/usr/bin/g++-4.9
    CC=/usr/bin/gcc-4.9
    CPP=/usr/bin/cpp-4.9

    if [ ! -x ${CC} ]
    then
      GCC_MAJOR=$(echo __GNUC__ | gcc -E -x c - | tail -n 1)
      GCC_MINOR=$(echo __GNUC_MINOR__ | gcc -E -x c - | tail -n 1)
      if [ ${GCC_MAJOR} -gt 4 ] || [ ${GCC_MAJOR} -eq 4 -a ${GCC_MINOR} -ge 9 ]
      then
        CC=/usr/bin/gcc
        CPP=/usr/bin/cpp
      else
        echo 'gcc compiler too old, need at least 4.9+ installed'
        exit 1
      fi
    fi

    if [ ! -x ${CXX} ]
    then
      GCC_MAJOR=$(echo __GNUC__ | g++ -E -x c - | tail -n 1)
      GCC_MINOR=$(echo __GNUC_MINOR__ | g++ -E -x c - | tail -n 1)
      if [ ${GCC_MAJOR} -gt 4 ] || [ ${GCC_MAJOR} -eq 4 -a ${GCC_MINOR} -ge 9 ]
      then
        CXX=/usr/bin/g++
      else
        echo 'g++ compiler too old, need at least 4.9+ installed'
        exit 1
      fi
    fi
fi

if [[ "x${USE_INCREMENTAL_LINKING}" == "xyes" ]]
then
    echo "Dont use incremental linking yet, it doesn't work"
    exit 1
    LDFLAGS=${LDFLAGS:-" -Wl,--incremental"}
else
    LDFLAGS=${LDFLAGS:-""}
fi

LIBS=${LIBS:-""}

# while we're transitioning away from buildtools there's no point in adding -lrdmacm to
# buildtools only to drop it again
LIBS="-lrdmacm ${LIBS}"

# here for ganesha modules but that reminds me that we don't really support c in the source tree.
CFLAGS=${CFLAGS:-"-ggdb3 -gdwarf-3 -O0 -Wall"}
CFLAGS="${CFLAGS} -fPIC"

CPPFLAGS=${CPPFLAGS:-""}
CPPFLAGS="${CPPFLAGS} -isystem ${BUILDTOOLS}/include"

CXX_INCLUDES=${CXX_INCLUDES:-""}
CXX_WARNINGS=${CXX_WARNINGS:-"-Wall -Wextra -Wno-unknown-pragmas -Wctor-dtor-privacy -Wsign-promo -Woverloaded-virtual -Wnon-virtual-dtor"}
CXX_SETTINGS=${CXX_SETTINGS:-"-std=gnu++14 -fPIC"}
CXX_DEFINES=${CXX_DEFINES:-"-DBOOST_FILESYSTEM_VERSION=3 -DBOOST_ASIO_HAS_STD_CHRONO"}
CXX_OPTIMIZE_FLAGS=${CXX_OPTIMIZE_FLAGS:-"-ggdb3 -gdwarf-3 -O0"}
CXX_COVERAGE_FLAGS=${CXX_COVERAGE_FLAGS:-"-fprofile-arcs -ftest-coverage"}
CXX_SANITIZE_FLAGS=${CXX_SANITIZE_FLAGS:-""}
CXXFLAGS="$CXX_INCLUDES $CXX_WARNINGS $CXX_SETTINGS $CXX_DEFINES $CXX_OPTIMIZE_FLAGS $CXX_SANITIZE_FLAGS"

if [ "x${COVERAGE}" == "xyes" ]
then
    CXXFLAGS="${CXXFLAGS} ${CXX_COVERAGE_FLAGS}"
    LIBS="${LIBS} -lgcov"
fi

#XXX: thread this through autoconf/make
if [ "x${SUPPRESS_WARNINGS}" == "xyes" ]
then
    CXXFLAGS="${CXXFLAGS} -DSUPPRESS_WARNINGS"
fi


BUILD_DIR_NAME=build
COVERAGE_DIR_NAME=coverage
SCAN_BUILD_DIR_NAME=scan-build

GCOV=/usr/bin/gcov
LCOV=/usr/bin/lcov
GENHTML=/usr/bin/genhtml
CLANG=/usr/bin/clang
SCAN_BUILD=/usr/bin/scan-build

if [ "x${USE_CLANG_ANALYZER}" == "xyes" ]
then
    echo "Enabling static code analysis with clang analyzer"
    SCAN_BUILD_CMDLINE="${SCAN_BUILD} --use-c++=${CXX} --use-cc=${CC} --use-analyzer=${CLANG} -o ${SCAN_BUILD_DIR_NAME}"
else
    echo "Disabling static code analysis with clang analyzer"
    SCAN_BUILD_CMDLINE=""
fi

# function install_thrift_python {
#     mkdir -p ${PREFIX}/lib/python2.7/dist-packages
#     cp -R ${BUILDTOOLS}/lib/python2.7/site-packages/thrift ${PREFIX}/lib/python2.7/dist-packages
# }

function generate_coverage_info {

    COVERAGE_DIR=$(pwd)/${COVERAGE_DIR_NAME}
    echo "Generating coverage info in ${COVERAGE_DIR}"
    BUILD_DIR=$(pwd)/${BUILD_DIR_NAME}

    mkdir -p ${COVERAGE_DIR}
    pushd ${COVERAGE_DIR}

    ${LCOV} --capture --compat-libtool --directory ${BUILD_DIR} --output-file coverage.info --gcov-tool=${GCOV}
    ${GENHTML} coverage.info
    popd

    echo "Done generating coverage info in ${COVERAGE_DIR}"
}

function reset_coverage_info {
    COVERAGE_DIR=$(pwd)/${COVERAGE_DIR_NAME}
    echo "Resetting coverage info in ${COVERAGE_DIR}"
    BUILD_DIR=$(pwd)/${BUILD_DIR_NAME}

    mkdir -p ${COVERAGE_DIR}
    pushd ${COVERAGE_DIR}

    ${LCOV} --zerocounters --compat-libtool --directory ${BUILD_DIR} --gcov-tool=${GCOV}
    popd

    echo "Done resetting coverage info in ${COVERAGE_DIR}"
}

function clean_build {
    echo "CLEANING up the previous build"
    if [ -f $BUILD_DIR_NAME/Makefile ]
    then
	pushd $BUILD_DIR_NAME
	make uninstall || true
	make distclean || true
	popd
    else
	echo "Not cleaning up since there was no preconfigured build here"
    fi
    rm -rf ${BUILD_DIR_NAME?} ${COVERAGE_DIR_NAME?} bin debian opt lib tests || true
    # clean up coverage files
    find . -name \*.gcda -exec rm -f {} \;
    find . -name \*.gcno -exec rm -f {} \;
}


function configure_build {
    echo "Configuring the build"
    SOURCE_DIR=$VOLUMEDRIVER_DIR/src

    mkdir -p ${BUILD_DIR_NAME?}
    pushd ${BUILD_DIR_NAME?}
    mkdir -p ${SCAN_BUILD_DIR_NAME?}
    autoreconf -isv ${SOURCE_DIR?}
    VD_VERSION_STR=$(git describe --abbrev=0)
    VD_VERSION=(${VD_VERSION_STR//./ })
    CXX=${CXX?} \
	CC=${CC?} \
	CPP=${CPP} \
	CFLAGS=${CFLAGS?} \
	CPPFLAGS=${CPPFLAGS?} \
	CXXFLAGS=${CXXFLAGS?} \
	LDFLAGS=${LDFLAGS} \
	LIBS=${LIBS} \
	VD_MAJOR_VERSION=${VD_VERSION[0]} \
	VD_MINOR_VERSION=${VD_VERSION[1]} \
	VD_PATCH_VERSION=${VD_VERSION[2]} \
	VD_EXTRA_VERSION=${VD_EXTRA_VERSION?} \
	${SCAN_BUILD_CMDLINE} \
	${SOURCE_DIR}/configure \
	--with-buildtoolsdir=${BUILDTOOLS?} \
  --with-omniidl=/usr/bin/omniidl \
	--with-capnpc=${BUILDTOOLS?}/bin/capnpc \
	--with-protoc=/usr/bin/protoc \
	--with-fio=${BUILDTOOLS?}/bin/fio \
	--with-lttng-gen-tp=/usr/bin/lttng-gen-tp \
	--config-cache \
	--with-ganesha=${BUILDTOOLS}/include/ganesha \
	BUILDTOOLS=${BUILDTOOLS?} \
	--prefix=$PREFIX "$@" 2>&1 | tee build.log
    popd
}

function make_build {
    echo "Making the build"
    pushd ${BUILD_DIR_NAME?}
    if [ "x$DONT_REMOVE_BUILDINFO_CPP" != "xyes" ]
    then
	echo "Removing $VOLUMEDRIVER_DIR/src/youtils/Buildinfo.cpp"
	rm -f $VOLUMEDRIVER_DIR/src/youtils/BuildInfo.cpp
    else
	echo "Not removing $VOLUMEDRIVER_DIR/src/youtils/BuildInfo.cpp cause you asked me nicely"
    fi
#    install_thrift_python

    if [ "x$RECONFIGURE_BUILD" = "xyes" ]
    then
	${SCAN_BUILD_CMDLINE} make -j ${PALLALLELLIZATION} install 2>&1 | tee -a build.log
    else
	${SCAN_BUILD_CMDLINE} make -j ${PALLALLELLIZATION} install 2>&1 | tee build.log
    fi
    popd
}

function check_build {
    echo "Checking the build"
    if [ "x$RUN_TESTS" = "xyes" ]
    then
	pushd ${BUILD_DIR_NAME?}
	make -j ${PALLALLELLIZATION} check 2>&1 | tee test.log
	popd
    else
	echo "Not running the tests, set RUN_TESTS to yes to do this"
    fi
}

function build_debian_packages {
    echo "BUILDING debian packages"
    pushd ${BUILD_DIR_NAME?}/packaging
    make -j ${PALLALLELLIZATION} install 2>&1 | tee packaging.log
    popd
}

function tag_repo {
    echo "TAGGING"
    pushd ${SOURCE_DIR}
    # Getting the current revision
    local rev=`git rev-parse HEAD`
    # Update the code (make sure we don't have merge stuff to do later on)
    git pull --rebase
    # Tag the previously selected revision
    git tag -r ${VD_TAG} ${rev}
    # Push the tag
    git push origin ${VD_TAG}
    popd
}

function build_docs {
    pushd ${BUILD_DIR_NAME?}
    if which doxygen &> /dev/null; then
        echo "Generating documentation..."
        make docs
        if [[ "x${DOCS_UPLOAD}" = "xyes" ]]
        then
            echo "Uploading..."
            ssh ${DOCS_SERVER} "mkdir -p ${DOCS_PATH}"
            ssh ${DOCS_SERVER} "rm -rf ${DOCS_PATH}*"
            pushd docs/html
            tar -cf - . | ssh ${DOCS_SERVER} "cd ${DOCS_PATH}; tar xf -"
            popd
            ssh ${DOCS_SERVER} "chown -R www-data:www-data ${DOCS_PATH}"
        fi
        echo "Done"
    else
        echo "No doxygen installed"
    fi
    popd
}

if [ "x$ONLY_BUILD_QSHELL_PYTHON" = "xyes" ]
then
    build_qshell
    exit
fi

if [ "x${CLEAN_BUILD}" = "xyes" ]
then
    clean_build
fi

if  [ ! -f ${BUILD_DIR_NAME?}/Makefile ]
then
    echo "Makefile does not exist - configuring the build"
    RECONFIGURE_BUILD=yes
fi

if [ "x$RECONFIGURE_BUILD" = "xyes" ]
then
    echo "Reconfiguring the build"
    configure_build
else
    echo "Not reconfiguring the build, set RECONFIGURE_BUILD to yes to do this"
fi

if [ "x${COVERAGE}" == "xyes" ]
then
    reset_coverage_info
fi

if [[ "x${SKIP_BUILD}" = "xyes" ]]
then
    echo "Skipping build. set SKIP_BUILD to something different than yes to make and clean the build"
else
    make_build
    check_build
fi

if [ "x${COVERAGE}" == "xyes" ]
then
    generate_coverage_info
fi

if [ "x$BUILD_DEBIAN_PACKAGES" = "xyes" ]
then
    build_debian_packages
else
    echo "Not building the debian packages set BUILD_DEBIAN_PACKAGES to yes to do this"
fi

if [ "x${VD_TAG}" = "x" ]
then
    echo "No VD_TAG set"
else
    tag_repo
fi

if [[ "x${BUILD_DOCS}" = "xyes" ]]
then
    build_docs
else
    echo "No documentation generated. Set BUILD_DOCS to yes to do this"
fi

## Local Variables: **
## compile-command: "time ./builder.sh" **
## End: **
