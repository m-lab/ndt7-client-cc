# NDT7 Library and Command line Client

[![GitHub license](https://img.shields.io/github/license/m-lab/ndt7-client-cc.svg)](https://raw.githubusercontent.com/m-lab/ndt7-client-cc/main/LICENSE) [![Github Releases](https://img.shields.io/github/release/m-lab/ndt7-client-cc.svg)](https://github.com/m-lab/ndt7-client-cc/releases) [![Build Status](https://app.travis-ci.com/m-lab/ndt7-client-cc.svg?branch=main)](https://app.travis-ci.com/m-lab/ndt7-client-cc) [![codecov](https://codecov.io/gh/m-lab/ndt7-client-cc/branch/main/graph/badge.svg)](https://codecov.io/gh/m-lab/ndt7-client-cc)

**Note**: This project is community supported and provided as-is for users and
client integrators. Contributions are welcome (PRs and issues).

ndt7-client-cc is a [Network-Diagnostic-Tool](
https://github.com/ndt-project/ndt/wiki/NDTProtocol) (NDT) single-include C++11
client library and command line client. NDT is a widely used network performance
test that measures the download and upload speed, and complements these
measurements with kernel-level measurements. NDT is the most popular network
performance test hosted by [Measurement Lab](https://www.measurementlab.net/).

This library implements the ndt7 protocol. The libndt7 code should be considered
an alpha release.

We forked this library from [measurement-kit/libndt@6a9040c21fc](
https://github.com/measurement-kit/libndt/commit/6a9040c21fcf43a40eb8e0d139be0d6b2a493b0a).

## Getting started

libndt7 depends on OpenSSL (for TLS support and in the future for WebSocket
support) and cURL (to autodiscover servers).

Download [single_include/libndt7.hpp](
https://github.com/m-lab/ndt7-client-cc/blob/main/single_include/libndt7.hpp) and
put it in the current working directory.

This example runs a download-only ndt7 test with a nearby, healthy server.
Create a file named `main.cpp` with this content:

```C++
#include "libndt7.hpp"

int main() {
  using namespace measurementlab;
  libndt7::Settings settings;
  std::unique_ptr<libndt7::Client>  client;
  settings.metadata["client_name"] = CLIENT_NAME;
  settings.metadata["client_version"] = CLIENT_VERSION;
  client.reset(new libndt7::Client{settings});
  client->run();
}
```

Compile your client with a unique name using:

```sh
g++ -std=c++11 -Wall -Wextra -I. \
  -DCLIENT_NAME=\"my-ndt7-client\" \
  -DCLIENT_VERSION=\"v0.1.0\" \
  -o main main.cpp
```

For API documentation, see
[include/libndt7/libndt7.hpp](include/libndt7/libndt7.hpp) for the full API.

See [ndt7-client-cc.cpp](ndt7-client-cc.cpp) for a comprehensive, reference client.

## Cloning the repository

To develop libndt7 or run tests, you need a clone of the repository.

```sh
git clone https://github.com/m-lab/ndt7-client-cc
```

## Building and testing

Build and run tests with:

```sh
cmake .
cmake --build .
ctest -a --output-on-failure .
```

## Command line client

Building with CMake also builds a simple command line client. Get usage info
by running:

```sh
./ndt7-client-cc -help
```

## Updating dependencies

Vendored dependencies are in `third_party`. We include the complete path to
where they can be found such that updating is obvious.
