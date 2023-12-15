# NDT Client Engine

[![GitHub license](https://img.shields.io/github/license/m-lab/ndt7-client-cc.svg)](https://raw.githubusercontent.com/m-lab/ndt7-client-cc/main/LICENSE) [![Github Releases](https://img.shields.io/github/release/m-lab/ndt7-client-cc.svg)](https://github.com/m-lab/ndt7-client-cc/releases) [![Build Status](https://img.shields.io/travis/m-lab/ndt7-client-cc/main.svg?label=travis)](https://travis-ci.org/m-lab/ndt7-client-cc) [![codecov](https://codecov.io/gh/m-lab/ndt7-client-cc/branch/main/graph/badge.svg)](https://codecov.io/gh/m-lab/ndt7-client-cc) [![Build status](https://img.shields.io/appveyor/ci/bassosimone/ndt7-client-cc/main.svg?label=appveyor)](https://ci.appveyor.com/project/bassosimone/ndt7-client-cc/branch/main) [![Documentation](https://codedocs.xyz/m-lab/ndt7-client-cc.svg)](https://codedocs.xyz/m-lab/ndt7-client-cc/)

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

This library implements all flavours of NDT. The code implementing the
legacy NDT protocol (i.e., no JSON, no WebSocket, no TLS, no ndt7) is
the most stable, tested, and peer reviewed code. The JSON, WebSocket, and
TLS flavoured NDT code is in beta stage. Ndt7 code is in alpha stage.

## Getting started

Libndt depends on OpenSSL (for TLS support and in the future for
WebSocket support) and cURL (to autodiscover servers).

Download [single_include/libndt.hpp](
https://github.com/measurement-kit/libndt/blob/master/single_include/libndt.hpp) and
put it in the current working directory.

This example runs a NDT download-only nettest with a nearby server. Create
a file named `main.cpp` with this content.

```C++
#include "libndt.hpp"

int main() {
  using namespace measurement_kit;
  libndt::Client client;
  client.run();
}
```

Compile with `g++ -std=c++11 -Wall -Wextra -I. -o main main.cpp`.

See [codedocs.xyz/measurement-kit/libndt](
https://codedocs.xyz/measurement-kit/libndt/) for API documentation;
[include/libndt/libndt.hpp](include/libndt/libndt.hpp) for the full API.

See [libndt-client.cpp](libndt-client.cpp) for a comprehensive usage example.

## Cloning the repository

To develop libndt or run tests, you need a clone of the repository.

```
git clone https://github.com/measurement-kit/libndt
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
