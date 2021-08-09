#!/bin/bash
# 版权所有 (c) 华为技术有限公司 2021-2021
set -e
cur_file_path=$(cd $(dirname "${0}");pwd)
export PATH=/usr/bin:/bin:${PATH}

inc()
{
    echo "begin incremental compile VideoCodec"
    cd ${cur_file_path}
    mkdir -p build/
    cd build
    cmake ..
    make -j16
    cd -
    echo "incremental compile VideoCodec success"
}

clean()
{
    echo -e "begin clean"
    rm -rf ./build
    echo "clean success"
}

build()
{
    echo "begin build"
    clean
    inc
    echo "build success"
}

ACTION=$1; 
case "$ACTION" in
    build) build "$@";;
    clean) clean "$@";;
    inc) inc "$@";;
    *) echo "input command[${ACTION}] not support";;
esac
