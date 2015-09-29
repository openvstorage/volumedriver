#!/bin/bash

# this is ugly, but apparently the valgrind plugin does not export the WORKSPACE env var
# (or the PATH env var for that matter) to us.
# we know this script is under volumedriver-core in the jenkins subdir, so just strip $0 to get VDC_DIR (volumdriver-core directory)

export VDC_DIR="${0%/jenkins/*.sh}"

export TEMP=${TEMP:-${VDC_DIR}/tmp}
export PATH=${VDC_DIR}/build/bin:${PATH}

export WORKSPACE
if [ -z "${WORKSPACE}" ]
then
  WORKSPACE="$(realpath ${VDC_DIR%/volumedriver-core})"
fi

pushd ${VDC_DIR}/build/bin

GTEST_REPORT_DIR=${VDC_DIR}/build/tests
GTEST_REPORT=${GTEST_REPORT_DIR}/failovercache_test.report.xml

mkdir -p ${GTEST_REPORT_DIR}

./failovercache_test \
    --disable-logging \
    --gtest_filter=-*Stress \
    --gtest_output=xml:${GTEST_REPORT}

popd

exit 0
