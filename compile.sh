#!/bin/bash

rm -rf build
mkdir build && cd build 
cmake .. -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DWITH_GLOG=ON  -DCMAKE_INSTALL_PREFIX=/home/zizorw/Workspace/lib/brpc -DCMAKE_PREFIX_PATH="/home/zizorw/git_repo/CppPlugins/third_party_libs/gflags_2.2.2/dynamic;/home/zizorw/git_repo/CppPlugins/third_party_libs/glog_0_6_0/dynamic" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_UNIT_TESTS=ON  && make -j16
# make test
# make install
# cd ..

# -DCMAKE_TOOLCHAIN_FILE=../FindGcc.cmake