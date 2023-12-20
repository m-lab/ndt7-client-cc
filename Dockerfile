FROM debian:testing-slim AS build
RUN apt-get update
RUN apt-get install -y autoconf automake clang cmake curl g++ git iproute2     \
      lcov libc++-dev libc++abi-dev libc-ares-dev libcurl4-openssl-dev         \
      libevent-dev libmaxminddb-dev libssl-dev libtool make ninja-build        \
      libgeoip-dev
