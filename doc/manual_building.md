## Preface

The _volumedriver_ code builds upon quite a number of (external) tools and libraries some of which are either not available, too old or require fixes on the current linux distro of choice (our default platform is Ubuntu 14.04 LTS). For these, we have a separate _volumedriver-buildtools_ repo that includes all requirements. 

To build the _volumedriver_ you'll need to:

1. install all prerequisites
2. build the _volumedriver-buildtools_
3. build the _volumedriver_ itself

The code can either be built for debugging purposes (_rtchecked_ build) or as code to be run in production (_release_ build). In this document we mainly concentrate on the _release_ build process, but we'll hint at the changes needed for debug (_rtchecked_) builds.

Note: If you want a simpler, more automated way of building, do have a look at the [Building with docker](doc/build_with_docker.md) document.

## 1. Installing all prerequisites

  - on __Ubuntu 14.04__ 

          apt-get install -y software-properties-common
          add-apt-repository -y ppa:ubuntu-toolchain-r/test
          add-apt-repository -y ppa:afrank/boost
          apt-get update
          apt-get install -y gcc-4.9 g++-4.9 libstdc++-4.9-dev clang-3.5 \
                             libboost1.57-all-dev \
                             build-essential \
                             flex bison gawk check pkg-config \
                             autoconf libtool realpath bc gettext lcov \
                             unzip doxygen dkms debhelper pylint git cmake \
                             wget libssl-dev libpython2.7-dev libxml2-dev \
                             libcurl4-openssl-dev libc6-dbg \
                             librabbitmq-dev libaio-dev libkrb5-dev libc-ares-dev
      
          update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.9 99
          update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-4.9 99
          update-alternatives --install /usr/bin/clang clang /usr/bin/clang-3.5 99
          update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-3.5 99
          update-alternatives --install /usr/bin/scan-build scan-build /usr/bin/scan-build-3.5 99
          
          apt-get install -y mercurial rpm pkg-create-dbgsym elfutils \
                             libloki-dev libprotobuf-dev liblttng-ust-dev libzmq3-dev \
                             libtokyocabinet-dev libbz2-dev protobuf-compiler \
                             libgflags-dev libsnappy-dev \
                             redis-server libhiredis-dev libhiredis-dbg \
                             libomniorb4-dev omniidl omniidl-python omniorb-nameserver \
                             librdmacm-dev libibverbs-dev python-nose fuse \
                             python-protobuf \
                             rpcbind
                             
  - on __CentOS 7__
  
    Note: some of the required tools/libs are available from our own [repository](http://yum.openvstorage.org/CentOS/7/x86_64/dists/unstable/upstream/), so we'll add this to allow installing from there.
    
          yum -y install epel-release
          echo -e '[ovs]\nname=ovs\nbaseurl=http://yum.openvstorage.org/CentOS/7/x86_64/dists/unstable\nenabled=1\ngpgcheck=0' > /etc/yum.repos.d/ovs.repo
          yum -y update
          yum -y install sudo wget iproute \
                         gcc gcc-c++ clang boost-devel make \
                         flex bison gawk check pkgconfig autoconf libtool bc gettext \
                         unzip doxygen dkms pylint git mercurial cmake lcov \
                         openssl-devel python-devel libxml2-devel librabbitmq-devel  \
                         libaio-devel krb5-devel c-ares-devel check-devel valgrind-devel \
                         librdmacm-devel loki-lib-devel protobuf-devel lttng-ust-devel \
                         cppzmq-devel tokyocabinet-devel bzip2-devel protobuf-compiler \
                         gflags-devel snappy-devel omniORB-devel omniORB-servers python-omniORB \
                         redis hiredis-devel \
                         python-nose libcurl-devel \
                         rpm rpm-build fuse protobuf-python fakeroot \
                         rpcbind

## 2. Building _volumedriver-buildtools_

  - check out the source
  
        git clone https://github.com/openvstorage/volumedriver-buildtools

  - create a configuration file that points to the desired install location and a path where downloaded sources will be stored
    This file needs to define:

      - INSTALL_DIR = location where build artifacts will be stored
      - SOURCES_DIR = location where downloaded external sources will be stored (if not already present)
      - PREFIX = normally subdir under INSTALL_DIR to keep _rtchecked_ and _release_ builds apart
      - BUILD_NUM_PROCESSES = the number of concurrent build processes (make -j)
      
    For example, create _my-release-build.cfg_:
    
        cat >my-release-build.cfg <<_EOF_
        WORKSPACE="${PWD}"
        INSTALL_DIR="\${WORKSPACE}/BUILDS/volumedriver-buildtools"
        SOURCES_DIR="\${WORKSPACE}/BUILDS/volumedriver-buildtools/sources"
        PREFIX="\${INSTALL_DIR}/release"
        BUILD_NUM_PROCESSES=4
        _EOF_

    Note: use _PREFIX="${INSTALL_DIR}/rtchecked"_ and _my-rtchecked-build.cfg_ for debug builds
    
  - set the VOLUMEDRIVER_BUILD_CONFIGURATION environment variable to this config file and run __build.sh__ to compile and install the artifacts:
  
        export VOLUMEDRIVER_BUILD_CONFIGURATION="${PWD}/my-release-build.cfg"
        (cd volumedriver-buildtools/src/release; ./build.sh)

    Note: use _my-rtchecked-build.cfg_ and _cd volumedriver-buildtools/src/rtchecked_ for debug builds
    
    \[TODO: change code of build.sh to get rid of the required cd\]
    
## 3. Build the _volumedriver_ itself

  - if you want to run the included testsuite, first read [running the included testsuite](running_included_testsuite.md) for extra requirements!
  - check out the source
  
        git clone https://github.com/openvstorage/volumedriver 

  - create a custom build script that contains the location of the volumedriver-buildtools built in the previous step plus a bunch of other settings, for example:
  
        cat >bld-voldrvr-release.sh <<_EOF_
        #!/bin/bash 
        
        set -eux 
        
        BUILDTOOLS_TO_USE="${PWD}/BUILDS/volumedriver-buildtools/release"
        
        VOLUMEDRIVER_DIR="${PWD}/volumedriver"  # must be a full path, not a relative one!
        BUILD_DIR="${PWD}/BUILDS/volumedriver/release"
        
        BUILDER="\${VOLUMEDRIVER_DIR}/src/buildscripts/builder.sh"
        
        export RUN_TESTS=no                   # set to "yes" to run included test suite (needs running rpcbind, redis & omniNames + installed arakoon & alba; see docs!)
        export BUILD_DEBIAN_PACKAGES=yes      # both for deb & rpm; change to "no" to skip creating packages
        export CLEAN_BUILD=no                 # set to "yes" to clean build env (force complete rebuild)
        export RECONFIGURE_BUILD=yes          # do reconfigure the code
        export USE_MD5_HASH=yes               # set to "no" to disable deduping
        export BUILD_NUM_PROCESSES=2          # number of concurrent build processes (make -j)
        export SUPPRESS_WARNINGS=no
        export COVERAGE=no
        
        export CXX_WARNINGS="-Wall -Wextra -Wno-unknown-pragmas -Wsign-promo -Woverloaded-virtual -Wnon-virtual-dtor"
        export CXX_OPTIMIZE_FLAGS="-ggdb3 -O2"
        export CXX_DEFINES="-DNDEBUG -DBOOST_FILESYSTEM_VERSION=3"
        
        ## settings for the included testsuite
        export ARAKOON_BINARY=/usr/bin/arakoon
        export FOC_PORT_BASE=19100
        export FAILOVERCACHE_TEST_PORT=\${FOC_PORT_BASE}
        export ARAKOON_PORT_BASE=\$((FOC_PORT_BASE + 10))
        export VFS_PORT_BASE=\$((FOC_PORT_BASE + 20))
        export MDS_PORT_BASE=\$((VFS_PORT_BASE + 20))

        mkdir -p \${BUILD_DIR}
        ln -sf \${BUILDER} \${BUILD_DIR}
        
        pushd \${BUILD_DIR}
        
        set +e
        \${BUILDER} \${BUILDTOOLS_TO_USE} \${VOLUMEDRIVER_DIR}
        ret=\$?
        set -e
        
        popd
        
        exit \${ret}
        _EOF_
        
        chmod +x bld-voldrvr-release.sh
        
  - build the volumedriver
  
        ./bld-voldrvr-release.sh

The resulting packages will be either under **${BUILD_DIR}/debian** or **${BUILD_DIR}/rpm**.
If you ran the including testsuite, take a look at ${BUILD_DIR}/build/test.log for the results.
