## Preface

The _volumedriver_ code builds upon quite a number of (external) tools and libraries of which some are either not available, too old or require fixes on the current linux distro of choice (our default platform is Ubuntu 14.04 LTS). For these, we have a separate _volumedriver-buildtools_ repo that includes those items. Building the _volumedriver_ thus first requires building the _volumedriver-buildtools_ after installing its prerequisites.

Note: If you want a simpler, more automated way of building, do have a look at the [Building with docker](doc/build_with_docker.md) document.

## Installing all prerequisites

  - on Ubuntu 14.04 

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
                             
  - on CentOS 7
  
    Note: some of the required tools/libs are available on our own repository, so we'll add this to allow installing from there.
    
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

## Building _volumedriver-buildtools_
