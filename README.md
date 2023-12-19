# NDT Client Engine

[![GitHub license](https://img.shields.io/github/license/m-lab/ndt7-client-cc.svg)](https://raw.githubusercontent.com/m-lab/ndt7-client-cc/main/LICENSE) [![Github Releases](https://img.shields.io/github/release/m-lab/ndt7-client-cc.svg)](https://github.com/m-lab/ndt7-client-cc/releases) [![Build Status](https://img.shields.io/travis/m-lab/ndt7-client-cc/main.svg?label=travis)](https://travis-ci.org/m-lab/ndt7-client-cc) [![codecov](https://codecov.io/gh/m-lab/ndt7-client-cc/branch/main/graph/badge.svg)](https://codecov.io/gh/m-lab/ndt7-client-cc) [![Build status](https://img.shields.io/appveyor/ci/bassosimone/ndt7-client-cc/main.svg?label=appveyor)](https://ci.appveyor.com/project/bassosimone/ndt7-client-cc/branch/main) [![Documentation](https://codedocs.xyz/m-lab/ndt7-client-cc.svg)](https://codedocs.xyz/m-lab/ndt7-client-cc/)

**Note**: this project is currently unmaintained due to lack of time. Maybe
one day we'll find a fix for this issue and resume development...

![tardis](docs/tardis.gif
  "It's not supposed to make that noise. You leave the brakes on.")

ndt7-client-cc is a [Network-Diagnostic-Tool](
https://github.com/ndt-project/ndt/wiki/NDTProtocol) (NDT) single-include C++11
client library and command line client. NDT is a widely used network performance
test that measures the download and upload speed, and complements these
measurements with kernel-level measurements. NDT is the most popular network
performance test hosted by [Measurement Lab](https://www.measurementlab.net/).

This library implements the ndt7 protocol. Ndt7 code is in alpha stage.

## Getting started

libndt7 depends on OpenSSL (for TLS support and in the future for
WebSocket support) and cURL (to autodiscover servers).

Download [single_include/libndt7.hpp](
https://github.com/m-lab/ndt7-client-cc/blob/main/single_include/libndt7.hpp) and
put it in the current working directory.

This example runs a NDT download-only nettest with a nearby server. Create
a file named `main.cpp` with this content.

```C++
#include "libndt7.hpp"

int main() {
  using namespace measurement_kit;
  libndt::Settings settings;
  std::unique_ptr<libndt::Client>  client;
  settings.metadata["client_name"] = CLIENT_NAME;
  settings.metadata["client_version"] = CLIENT_VERSION;
  client.reset(new libndt::Client{settings});
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

See [codedocs.xyz/m-lab/ndt7-client-cc](
https://codedocs.xyz/m-lab/ndt7-client-cc/) for API documentation;
[include/libndt7/libndt7.hpp](include/libndt7/libndt7.hpp) for the full API.

See [ndt7-client-cc.cpp](ndt7-client-cc.cpp) for a comprehensive usage example.

## Cloning the repository

To develop libndt7 or run tests, you need a clone of the repository.

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
./ndt7-client-cc -help
```

## Updating dependencies

Vendored dependencies are in `third_party`. We include the complete path to
where they can be found such that updating is obvious.
