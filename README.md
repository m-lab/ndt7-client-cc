# NDT Client Engine

[![GitHub license](https://img.shields.io/github/license/measurement-kit/libndt.svg)](https://raw.githubusercontent.com/measurement-kit/libndt/master/LICENSE) [![Github Releases](https://img.shields.io/github/release/measurement-kit/libndt.svg)](https://github.com/measurement-kit/libndt/releases) [![Build Status](https://img.shields.io/travis/measurement-kit/libndt/master.svg?label=travis)](https://travis-ci.org/measurement-kit/libndt) [![codecov](https://codecov.io/gh/measurement-kit/libndt/branch/master/graph/badge.svg)](https://codecov.io/gh/measurement-kit/libndt) [![Build status](https://img.shields.io/appveyor/ci/bassosimone/libndt/master.svg?label=appveyor)](https://ci.appveyor.com/project/bassosimone/libndt/branch/master) [![Documentation](https://codedocs.xyz/measurement-kit/libndt.svg)](https://codedocs.xyz/measurement-kit/libndt/)

**Note**: this project is currently unmaintained due to lack of time. Maybe
one day we'll find a fix for this issue and resume development...

![tardis](docs/tardis.gif
  "It's not supposed to make that noise. You leave the brakes on.")

Libndt is a [Network-Diagnostic-Tool](
https://github.com/ndt-project/ndt/wiki/NDTProtocol) (NDT) single-include
C++11 client library. NDT is a widely used network performance test that
measures the download and upload speed, and complements these measurements
with kernel-level measurements. NDT is the most popular network performance
test hosted by [Measurement Lab](https://www.measurementlab.net/).

This library implements the ndt7 protocol only. The ndt7 code is in alpha stage.

## Getting started

Libndt depends on OpenSSL (for TLS support and in the future for
WebSocket support) and cURL (to autodiscover servers).

This example runs a NDT download-only nettest with a nearby server. Create
a file named `main.cpp` with this content.

```C++
#include "third_party/github.com/nlohmann/json/json.hpp"
#include "include/libndt/libndt.hpp"

int main() {
  using namespace measurement_kit;
  libndt::Client client;
  client.run();
}
```

Compile with `g++ -std=c++11 -Wall -Wextra -I. -o main main.cpp`.

TODO(soltesz): update API documentation.
See [codedocs.xyz/measurement-kit/libndt](
https://codedocs.xyz/measurement-kit/libndt/) for API documentation;
[include/libndt/libndt.hpp](include/libndt/libndt.hpp) for the full API.

See [libndt-client.cpp](libndt-client.cpp) for a comprehensive usage example.

## Cloning the repository

To develop libndt or run tests, you need a clone of the repository.

```
git clone https://github.com/m-lab/ndt7-client-cc
```

## Building and testing

Build and run tests with:

```
cmake .
cmake --build .
ctest -a --output-on-failure .
```

## Command line client

Building with CMake also builds a simple command line client. Get usage info
by running:

```
./libndt-client -help
```

## Updating dependencies

Vendored dependencies are in `third_party`. We include the complete path to
where they can be found such that updating is obvious.
