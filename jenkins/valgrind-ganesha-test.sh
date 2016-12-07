#!/bin/bash

# this is ugly, but apparently the valgrind plugin does not export the WORKSPACE env var
# (or the PATH env var for that matter) to us.
# we know this script is under volumedriver-core in the jenkins subdir, so just strip $0 to get VDC_DIR (volumdriver-core directory)

export VDC_DIR="${0%/jenkins/*.sh}"

BUILDTOOLS_TO_USE=${BUILDTOOLS_TO_USE:-$(realpath ${VDC_DIR}/../BUILDS/volumedriver-buildtools-5.0/rtchecked)}

export TEMP=${TEMP:-${VDC_DIR}/tmp}
export PATH=${VDC_DIR}/build/bin:${PATH}

export WORKSPACE
if [ -z "${WORKSPACE}" ]
then
  WORKSPACE="$(realpath ${VDC_DIR%/volumedriver-core})"
fi

GTEST_REPORT_DIR=${VDC_DIR}/build/tests
GTEST_REPORT=${GTEST_REPORT_DIR}/volumedriverfs_ganesha_test.report.xml

volumedriverfs_ganesha_test \
    --ganesha-binary-path=${BUILDTOOLS_TO_USE}/bin/ganesha.nfsd \
    --volumedriver-fsal-path=${VDC_DIR}/build/lib/ganesha \
    --disable-logging \
    --gtest_output=xml:${GTEST_REPORT} \

exit 0
