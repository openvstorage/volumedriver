#!/bin/bash
set -eux

export TEMP=${TEMP:-/tmp}

find ${TEMP}/arakoon -name nohup.pid  -print | xargs -I % bash -c 'kill -15 `cat %`'
find ${TEMP}/alba -name nohup.pid  -print | xargs -I % bash -c 'kill -15 `cat %`'

rm -rf ${TEMP}/alba
rm -rf ${TEMP}/arakoon
