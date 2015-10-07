#! /bin/bash

function get_revision() {
    pushd $1 > /dev/null
    local extra=`git describe --always --dirty`
    popd > /dev/null
    echo "$extra"
}

function get_time() {
    local d=`date --utc "+%Y%m%d%H%M"`
    echo "$d"
}

function get_debug_extra_version() {
    pushd $1 > /dev/null
    local r=`git rev-parse --short HEAD`
    local d=`get_time`
    popd > /dev/null
    echo "dev.$d.$r"
}
