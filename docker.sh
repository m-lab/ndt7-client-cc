#!/bin/sh -e
# Autogenerated by 'mkbuild'; DO NOT EDIT!

USAGE="Usage: $0 asan|clang|coverage|ubsan|vanilla"

if [ $# -eq 1 ]; then
  INTERNAL=0
  BUILD_TYPE="$1"
elif [ $# -eq 2 -a "$1" = "-internal" ]; then
  INTERNAL=1
  BUILD_TYPE="$2"
else
  echo "$USAGE" 1>&2
  exit 1
fi

if [ "$CODECOV_TOKEN" = "" ]; then
  echo "WARNING: CODECOV_TOKEN is not set" 1>&2
fi
if [ "$TRAVIS_BRANCH" = "" ]; then
  echo "WARNING: TRAVIS_BRANCH is not set" 1>&2
fi

set -x

if [ $INTERNAL -eq 0 ]; then
  if [ "`docker images -q local/debian-testing 2> /dev/null`" = "" ]; then
    # Create image for running tests.
    docker build -t local/debian-testing .
  fi
  ci_env=`curl -s https://codecov.io/env | bash`
  exec docker run --cap-add=NET_ADMIN \
                  --cap-add=SYS_PTRACE \
                  -e CODECOV_TOKEN=$CODECOV_TOKEN \
                  -e TRAVIS_BRANCH=$TRAVIS_BRANCH \
                  -e CI=true \
                  $ci_env \
                  -v "$(pwd):/workdir" \
                  --workdir /workdir \
                  -t local/debian-testing \
                  ./docker.sh -internal "$1"
fi

env | grep -v TOKEN | sort

# Select the proper build flags depending on the build type
if [ "$BUILD_TYPE" = "asan" ]; then
  export CFLAGS="-fsanitize=address -O1 -fno-omit-frame-pointer"
  export CXXFLAGS="-fsanitize=address -O1 -fno-omit-frame-pointer"
  export LDFLAGS="-fsanitize=address -fno-omit-frame-pointer"
  export CMAKE_BUILD_TYPE="Debug"

elif [ "$BUILD_TYPE" = "clang" ]; then
  export CMAKE_BUILD_TYPE="Release"
  export CXXFLAGS="-stdlib=libc++"
  export CC=clang
  export CXX=clang++

elif [ "$BUILD_TYPE" = "coverage" ]; then
  export CFLAGS="-O0 -g -fprofile-arcs -ftest-coverage"
  export CMAKE_BUILD_TYPE="Debug"
  export CXXFLAGS="-O0 -g -fprofile-arcs -ftest-coverage"
  export LDFLAGS="-lgcov"

elif [ "$BUILD_TYPE" = "ubsan" ]; then
  export CFLAGS="-fsanitize=undefined -fno-sanitize-recover"
  export CXXFLAGS="-fsanitize=undefined -fno-sanitize-recover"
  export LDFLAGS="-fsanitize=undefined"
  export CMAKE_BUILD_TYPE="Debug"

elif [ "$BUILD_TYPE" = "vanilla" ]; then
  export CMAKE_BUILD_TYPE="Release"

else
  echo "$0: BUILD_TYPE not in: asan, clang, coverage, ubsan, vanilla" 1>&2
  exit 1
fi

# Configure and make equivalent
mkdir -p build/$BUILD_TYPE
cd build/$BUILD_TYPE
cmake -GNinja -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE ../../
cmake --build . -- -v

# Make sure we don't consume too much resources by bumping latency. Not all
# repositories need this feature. For them the code is commented out.
#tc qdisc add dev eth0 root netem delay 200ms 10ms

# Make check equivalent
ctest --output-on-failure -a -j8

# Stop adding latency. Commented out if we don't need it.
#tc qdisc del dev eth0 root

# Measure and possibly report the test coverage
if [ "$BUILD_TYPE" = "coverage" ]; then
  lcov --directory . --capture -o lcov.info
  if [ "$CODECOV_TOKEN" != "" ]; then
    curl -fsSL -o codecov.sh https://codecov.io/bash
    bash codecov.sh -X gcov -Z -f lcov.info
  fi
fi
