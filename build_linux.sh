#!/bin/bash
# libVideoCodec的linux构建脚本
# 版权所有 (c) 华为技术有限公司 2021-2021

cur_file_path=$(cd $(dirname "${0}");pwd)

error()
{
    echo -e "\033[1;31m${*}\033[0m"
}
info()
{
    echo -e "\033[1;36m${*}\033[0m"
}

so_list=(
    video_codec/libVideoCodec.so
)

export PATH=/usr/bin:/bin:${PATH}

package()
{
    output_dir=${MODULE_OUTPUT_DIR}
    [ -z "${output_dir}" ] && output_dir=${cur_file_path}/output/linux && rm -rf ${output_dir} && mkdir -p ${output_dir}
    for so_name in ${so_list[@]}
    do
        source_path=${cur_file_path}/build/${so_name}
        target_path=${output_dir}
        [ ! -d "${target_path}" ] && mkdir -p ${target_path}
        cp -d ${source_path} ${target_path}
        [ ${?} != 0 ] && error "failed to copy ${source_path} to ${target_path}"
    done
    if [ -z "${MODULE_OUTPUT_DIR}" ];then
        cd ${output_dir}
        tar zcvf ../libVideoCodec_linux.tar.gz *
        cd -
    fi
}

inc()
{
    info "begin incremental compile"
    cd ${cur_file_path}
    mkdir -p build/
    cd build
    cmake ..
    make -j16
    [ ${?} != 0 ] && error "failed to incremental compile" && return -1
    cd -
    package
    info "incremental compile success"
}

clean()
{
    if [ -d "${cur_file_path}/build" ];then
        rm -rf ${cur_file_path}/build
    fi
    if [ -z "${MODULE_OUTPUT_DIR}" ];then
        output_dir=${cur_file_path}/output
        if [ -f "${output_dir}/libVideoCodec_linux.tar.gz" ];then
            rm -rf ${output_dir}/libVideoCodec_linux.tar.gz
        fi
        if [ -d "${output_dir}/linux" ];then
            rm -rf ${output_dir}/linux
        fi
        if [ -f "${output_dir}" ];then
            rm -rf ${output_dir}
        fi
    fi
}

build()
{
    info "begin build"
    clean
    [ ${?} != 0 ] && error "failed to clean" && return -1
    inc
    [ ${?} != 0 ] && error "failed to build" && return -1
    info "build success"
}

ACTION=$1;
case "$ACTION" in
    build) build "$@";;
    clean) clean "$@";;
    inc) inc "$@";;
    *) error "input command[${ACTION}] not support";;
esac
