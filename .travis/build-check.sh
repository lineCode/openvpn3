#!/bin/sh
set -eux

if [ "${TRAVIS_OS_NAME}" = "linux" ]; then
	export LD_LIBRARY_PATH="${PREFIX}/lib:${LD_LIBRARY_PATH:-}"
fi

if [ "${TRAVIS_OS_NAME}" = "osx"   ]; then
	export DYLD_LIBRARY_PATH="${PREFIX}/lib:${DYLD_LIBRARY_PATH:-}"
fi

cd test/ovpncli

CXXFLAGS="-fwhole-program -O3 -Wall -Wno-sign-compare"
CXXFLAGS="${CXXFLAGS} -Wno-unused-parameter -std=c++14 -flto=4"
CXXFLAGS="${CXXFLAGS} -Wno-unused-local-typedefs -Wno-unused-variable"
CXXFLAGS="${CXXFLAGS} -Wno-shift-count-overflow -pthread"
CXXFLAGS="${CXXFLAGS} -DOPENVPN_SHOW_SESSION_TOKEN -DUSE_MBEDTLS -DHAVE_LZ4"
CXXFLAGS="${CXXFLAGS} -DUSE_ASIO -DASIO_STANDALONE -DASIO_NO_DEPRECATED"
INCLUDEDIRS="-I/home/ordex/exp/asio//asio/include"
INCLUDEDIRS="${INCLUDEDIRS} -I../../"
LDFLAGS="-Wl,--no-as-needed -lmbedtls -lmbedx509 -lmbedcrypto -llz4"


g++ ${CXXFLAGS} ${INCLUDEDIRS} ${LDFLAGS} cli.cpp -o cli
