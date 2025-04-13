#!/bin/bash
set -euo pipefail 

# parameters
build_type="Debug"
curr_dir=$(pwd)
jobs=$(nproc)       
build_dir="${curr_dir}/build"

red='\033[0;31m'
green='\033[0;32m'
nc='\033[0m'


build_func() {
    echo -e "${green}curr_dir: ${curr_dir}${nc}"
    rm -rf $build_dir && mkdir -p $build_dir && cd $build_dir
    cmake -G Ninja -DCMAKE_BUILD_TYPE=$build_type -DUNIT_TESTING=ON ..

    if ninja -j$jobs; then
        echo -e "${green}build success!${nc}"
    else
        echo -e "${red}build failed!${nc}" >&2
        exit 1
    fi
}

show_help() {
    echo -e "${green}用法：${nc}"
    echo -e "${green}可用命令："
    echo -e "\tbuild   - 编译项目"
    echo -e "\ttest    - 运行测试"
    echo -e "\tall     - 构建并测试"
    echo -e "${nc}"
}

run_tests() {
    cd $build_dir && ninja && ctest --output-on-failure -V
    cd $curr_dir
}

entry() {
    if [ $# -eq 0 ]; then
        show_help
        exit 1
    fi
    case "$1" in
        build)
        build_func
        ;;
        test)
        run_tests
        ;;
        all)
        build_func && run_tests
        ;;
        *)
        echo "Usage: $0 {build|test|all}"
        exit 1
    esac
}

entry "$@"
