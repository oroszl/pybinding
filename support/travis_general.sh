#!/usr/bin/env sh

if [ "$TRAVIS_OS_NAME" = "linux" ]; then
    export CXX=g++-4.8 CC=gcc-4.8
elif [ "$TRAVIS_OS_NAME" = "osx" ]; then
    export CXX=clang++ CC=clang
fi

export PATH=$TRAVIS_BUILD_DIR/build/cmake/cppcore/tests/:$PATH
export PB_WERROR=ON  # make warnings into errors
export PB_TESTS=ON  # generate cpp tests
export PB_NATIVE_SIMD=OFF  # only generate generic SIMD for deployment
export MAKEFLAGS=-j2

$CXX --version
