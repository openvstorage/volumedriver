#! /bin/bash
set -eux
# test .. this file should be in `pwd`!!!

# script to get a volumdriverfs binary as quick as possible, excludes
# building tests altogether.
# If reconfiguration is necessary, use the jenkins-release-perf.sh (temporarily) 

VOLUMEDRIVER_DIR=$1
BUILD_DIR=${VOLUMEDRIVER_DIR}/build


pushd $BUILD_DIR
make -j4 -C build/youtils libyoutils.la
make -j4 -C build/backend libbackend.la
make -j4 -C build/volumedriver libvolumedriver.la
make -j4 -C build/filesystem volumedriver_fs
popd
