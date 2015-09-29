#! /bin/bash

function get_revision() {
    pushd $1 > /dev/null
    local extra=`hg id -i`
    popd > /dev/null
    echo "$extra"
}

function get_time() {
    local d=`date --utc "+%Y%m%d%H%M"`
    echo "$d"
}

function get_debug_extra_version() {
    local r=`get_revision $1`
    local d=`get_time`
    echo "dev.$d.$r"
}

function get_release_extra_version() {
    pushd $1 > /dev/null
    local major=`cat src/major.txt`
    local minor=`cat src/minor.txt`
    local patch=`cat src/patch.txt`
    local extra=`hg tags | grep -F "release-$major.$minor.$patch." | sed "s/\s\s*/ /g" | cut -d ' ' -f 1 | cut -d '.' -f 4 | sort -rn | head -n 1`
    ((extra++))
    popd > /dev/null
    echo "$extra"
}

function get_release_tag() {
    pushd $1 > /dev/null
    local major=`cat src/major.txt`
    local minor=`cat src/minor.txt`
    local patch=`cat src/patch.txt`
    local extra=`get_release_extra_version $1`
    popd > /dev/null
    echo "release-$major.$minor.$patch.$extra"
}
