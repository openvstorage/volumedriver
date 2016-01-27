Open vStorage VolumeDriver
==========================
The Open vStorage Volumedriver is the core of the Open vStorage solution: a high performance distributed block layer. It is the technology that converts block storage into object (Storage Container Objects) which can be stored on the different supported backends. To minimize the latency and boost the performance, it offers read and write acceleration on SSDs or PCI-e flash cards.

The Volumedriver implements functionality such as zero-copy snapshots, thin-cloning, scrubbing of data out of the retention, thin provisioning and a distributed transaction log.

The Volumedriver currently supports KVM (.raw) and VMware (.vmdk) as hypervisor.

The Volumedriver consists out of 3 modules:
* The Virtual File System Router: a layer which routes IO to the correct Volume Router.
* The Volume Router: conversion from block into objects (Storage Container Objects), caching and scrubber functionality
* The Storage Router: spreading the SCOs across the storage backend.

The Open vStorage Volumedriver is written in C++.

License
-------
Apache 2.0 (see LICENSE) unless noted otherwise in the upstream components that
are part of this source tree (cppzmq, etcdcpp, msgpack-c, procon, pstreams,
xmlrpc++).

Prerequisites
-------------
[volumedriver-buildtools](https://github.com/openvstorage/volumedriver-buildtools) and its prerequisites need to be installed. In addition to
that, the following tools and libraries (their development versions) need to be
present:

* arakoon
* libloki
* liblttng-ust
* libpython2.7
* librdmacm
* liburcu
* lttng-tools
* protobuf
* python-protobuf
* python-nose
* omniorb
* rpm
* zeromq 3.2.x

Build Instructions
------------------
VolumeDriver uses autotools.

There are some scripts available under src/buildscripts to help setting up the build
correctly. By way of example:

(1) Create a custom build configuration, e.g.:
    #! /bin/bash
    set -eux
    # test .. this file should be in `pwd`!!!

    BUILDTOOLS_TO_USE=$(realpath ~/Projects/openvstorage/volumedriver-buildtools/rtchecked)
    VOLUMEDRIVER_DIR=$(realpath $1)
    BUILDER=$(realpath ${VOLUMEDRIVER_DIR}/src/buildscripts/builder.sh)
    BUILD_DIR=${VOLUMEDRIVER_DIR}/target/rtchecked

    export RUN_TESTS=no
    export COVERAGE=no
    export BUILD_NUM_PROCESSES=2
    export BUILD_DEBIAN_PACKAGES=${BUILD_DEBIAN_PACKAGES:-no}
    export CLEAN_BUILD=${CLEAN_BUILD:-"no"}
    export CXX_WARNINGS="-Wall -Wextra -Wno-unknown-pragmas -Wsign-promo -Woverloaded-virtual -Wnon-virtual-dtor"
    export SUPPRESS_WARNINGS=no

    mkdir -p ${BUILD_DIR}
    ln -sf ${BUILDER} $BUILD_DIR

    pushd $BUILD_DIR

    set +e
    ${BUILDER} $BUILDTOOLS_TO_USE $VOLUMEDRIVER_DIR
    ret=$?
    set -e

    popd

    exit ${ret}

(2) Run the build script:
    src/buildscripts/rtchecked.sh .
