// Part of Measurement Kit <https://measurement-kit.github.io/>.
// Measurement Kit is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.

#include "third_party/github.com/nlohmann/json/json.hpp"

#include "libndt/libndt.hpp"  // not standalone

#include <stdlib.h>

#include <iostream>
#include <sstream>
#include <memory>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#endif  // __clang__
#include "third_party/github.com/adishavit/argh/argh.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif  // __clang__

using namespace measurement_kit;

// BatchClient only prints JSON messages on stdout.
class BatchClient : public libndt::Client {
  public:
    using libndt::Client::Client;
    void on_result(std::string, std::string, std::string value) noexcept override;
    void on_performance(libndt::NettestFlags, uint8_t, double, double,
                        double) noexcept override;
    void summary() noexcept override;
};

// on_result is overridden to only print the JSON value on stdout.
void BatchClient::on_result(std::string, std::string, std::string value) noexcept {
  std::cout << value << std::endl;
}
// on_performance is overridded to hide the user-friendly output messages.
void BatchClient::on_performance(libndt::NettestFlags tid, uint8_t nflows,
                            double measured_bytes,
                            double elapsed_time, double) noexcept {
  nlohmann::json performance;
  performance["ElapsedTime"] = elapsed_time;
  performance["NumFlows"] = nflows;
  performance["TestId"] = (int)tid;
  performance["Speed"] = libndt::format_speed_from_kbits(measured_bytes,
                                                         elapsed_time);
  std::cout << performance.dump() << std::endl;
}

// summary is overridden to print a JSON summary.
void BatchClient::summary() noexcept {
  nlohmann::json summary;

  if (summary_.download_speed != 0.0) {
    nlohmann::json download;
    download["Speed"] = summary_.download_speed;
    download["Retransmission"] = summary_.download_retrans;

    if (web100 != nullptr) {
      download["Web100"] = web100;
    }

    if (measurement_ != nullptr) {
      download["ConnectionInfo"] = connection_info_;
      download["LastMeasurement"] = measurement_;
    }

    summary["Download"] = download;
    summary["Latency"] = summary_.min_rtt;
  }

  if (summary_.upload_speed != 0.0) {
    nlohmann::json upload;
    upload["Speed"] = summary_.upload_speed;
    upload["Retransmission"] = summary_.upload_retrans;
    summary["Upload"] = upload;
  }

  std::cout << summary.dump() << std::endl;
}

static void usage() {
  // clang-format off
  std::clog << R"(Usage: libndt-client <-upload|-download> [options]

You MUST specify what subtest to enable:
 * `-download` enables the download subtest
 * `-upload` enables the upload subtest

By default, libndt-client uses M-Lab's unregistered Locate API to find a
suitable target server. For registered clients, you may specify an API key for
the Locate API using:
* `-key=<key>`

Instead of the Locate API, you may specify a specific server using a combination
of the flags:
 * `-port=<port>`
 * `-scheme=<ws>`
 * `-hostname=<hostname>`

The default mode is wss (TLS).
 * `-scheme=wss` (default)
 * `-insecure` allows connecting to servers with self-signed or invalid certs.
 * `-ca-bundle-path=<path>` allows specifying an alternate CA bundle.

You may control information output using a combination of the following flags:
 * `-batch` outputs JSON results to STDOUT.
 * `-summary` only prints a summary at the end of the test.
 * `-verbose` prints additional debug information.

In combination, -batch and -summary produce a final summary in JSON.

The `-socks5h <port>` flag causes this tool to use the specified SOCKS5h
proxy to contact Locate API and for running the selected subtests.

The `-version` shows the version number and exits.)" << std::endl;
  // clang-format on
}

int main(int, char **argv) {
  libndt::Settings settings;
  settings.verbosity = libndt::verbosity_info;
  // You need to enable tests explicitly by passing command line flags.
  settings.nettest_flags = libndt::NettestFlags{0};
  bool batch_mode = false;
  bool summary = false;

  {
    argh::parser cmdline;
    cmdline.add_param("ca-bundle-path");
    cmdline.add_param("lookup-policy");
    cmdline.add_param("socks5h");
    cmdline.add_param("key");
    cmdline.add_param("port");
    cmdline.add_param("scheme");
    cmdline.add_param("hostname");
    cmdline.parse(argv);
    for (auto &flag : cmdline.flags()) {
      if (flag == "download") {
        settings.nettest_flags |= libndt::nettest_flag_download;
        std::clog << "will run the download sub-test" << std::endl;
      } else if (flag == "upload") {
        settings.nettest_flags |= libndt::nettest_flag_upload;
        std::clog << "will run the upload sub-test" << std::endl;
      } else if (flag == "help") {
        usage();
        exit(EXIT_SUCCESS);
      } else if (flag == "insecure") {
        settings.tls_verify_peer = false;
        std::clog << "WILL NOT verify the TLS peer (INSECURE!)" << std::endl;
      } else if (flag == "verbose") {
        settings.verbosity = libndt::verbosity_debug;
        std::clog << "will be verbose" << std::endl;
      } else if (flag == "version") {
        std::cout << libndt::version_major << "." << libndt::version_minor
                  << "." << libndt::version_patch << std::endl;
        exit(EXIT_SUCCESS);
      } else if (flag == "batch") {
        batch_mode = true;
        std::clog << "will run in batch mode" << std::endl;
      } else if (flag == "summary") {
        summary = true;
        std::clog << "will only display summary" << std::endl;
      } else {
        std::clog << "fatal: unrecognized flag: " << flag << std::endl;
        usage();
        exit(EXIT_FAILURE);
      }
    }
    for (auto &param : cmdline.params()) {
      if (param.first == "ca-bundle-path") {
        settings.ca_bundle_path = param.second;
        std::clog << "will use this CA bundle: " << param.second << std::endl;
      } else if (param.first == "key") {
        settings.metadata["key"] = param.second;
        std::clog << "will use this key: " << param.second << std::endl;
      } else if (param.first == "port") {
        settings.port = param.second;
        std::clog << "will use this port: " << param.second << std::endl;
      } else if (param.first == "scheme") {
        settings.scheme = param.second;
        std::clog << "will use this scheme: " << param.second << std::endl;
      } else if (param.first == "hostname") {
        settings.hostname = param.second;
        std::clog << "will use this hostname: " << param.second << std::endl;
      } else if (param.first == "socks5h") {
        settings.socks5h_port = param.second;
        std::clog << "will use the socks5h proxy at: 127.0.0.1:" << param.second << std::endl;
      } else {
        std::clog << "fatal: unrecognized param: " << param.first << std::endl;
        usage();
        exit(EXIT_FAILURE);
      }
    }
    if (settings.scheme != "ws" && settings.scheme != "wss" ) {
      std::clog << "fatal: invalid scheme: " << settings.scheme << std::endl;
      usage();
      exit(EXIT_FAILURE);
    }
    if (settings.scheme == "wss") {
      settings.protocol_flags |= libndt::protocol_flag_tls;
      std::clog << "will secure communications using TLS" << std::endl;
    }
    auto sz = cmdline.pos_args().size();
    if (sz != 1 && sz != 2) {
      usage();
      exit(EXIT_FAILURE);
    }
    if (!settings.hostname.empty()) {
      std::clog << "will use this static NDT server: " << \
        settings.scheme << "://" << settings.hostname << ":" << settings.port << std::endl;
    } else {
      std::clog << "will auto-select a suitable server" << std::endl;
    }
  }

  if (settings.nettest_flags == 0) {
    std::clog << "FATAL: No test selected" << std::endl;
    std::clog << "Run `libndt-client --help` for more help" << std::endl;
    exit(EXIT_FAILURE);
  }

  settings.summary_only = summary;
  std::unique_ptr<libndt::Client>  client;
  if (batch_mode) {
    client.reset(new BatchClient{settings});
  } else {
    client.reset(new libndt::Client{settings});
  }
  bool rv = client->run();
  if (rv ) {
    client->summary();
  }
  return (rv) ? EXIT_SUCCESS : EXIT_FAILURE;
}
