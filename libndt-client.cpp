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
  std::clog << R"(Usage: libndt-client [options] [<hostname>]

Options can start either with a single dash (i.e. -option) or with
a double dash (i.e. --option).

If an hostname is not specified, we use M-Lab's name service to
lookup a suitable M-Lab server to run the test with. You can use
the `-lookup-policy <policy>` flag to choose the policy to discover
M-Lab servers. The available policies are: `closest`, `random`,
and `geo-options`. The `closest` policy requests the hostname of a
closest nearby server. The `random` policy requests the hostname
of a random server. The `geo-options` policy returns a list of
nearby servers. The default policy is `geo-options`. The deprecated
`-random` flag is an alias for `-lookup-policy random`.

You MUST specify what subtest to enable. The `-download` flag enables the
download subtest. The `-upload` flag enables the upload subtest.

The `-port <port>` flag specifies what port to use. The default is to
use the correct port depending on the selected NDT protocol (see below).

By default, we use the ndt7 protocol. Adding the `-tls` flag causes NDT to use
a TLS connection rather than a TCP connection. When using `-tls`, you may also
want to use `-insecure` to allow connecting to servers with self-signed or
otherwise invalid TLS certificate. With `-tls`, you can also use the
`-ca-bundle-path <path>` to use a specific CA bundle path. Adding the
`-websocket` flag will cause NDT to wrap its messages (possibly already wrapped
by JSON) into WebSocket messages. Finally, adding the `-ndt7` flag turns on
version 7 of the NDT protocol, which is not backwards compatible. Since `-ndt7`
uses TLS, both `-ca-bundle-path <path>` and `-insecure` work also in combination
with the `-ndt7` flag. When using `-ndt7`, `-batch` can be specified so that the
only output on STDOUT will be the JSON test results.  To further reduce the
amount of output, you can use the `-summary` flag, which only prints a summary
at the end of the tests. If used with `-batch`, the generated summary will be
JSON.

In practice, these are the flags you want to use for ndt7:

1. none, to use the plain ndt7 protocol (ws://);

2. `-tls` to use the ndt7 protocol over TLS (wss://);

When running, this client emits messages. You can use `-verbose` to cause
it to emit even more messages.

The `-socks5h <port>` flag causes this tool to use the specified SOCKS5h
proxy to contact mlab-ns and for running the selected subtests.

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
    cmdline.add_param("port");
    cmdline.add_param("socks5h");
    cmdline.parse(argv);
    for (auto &flag : cmdline.flags()) {
      if (flag == "download") {
        settings.nettest_flags |= libndt::nettest_flag_download;
        std::clog << "will run the download sub-test" << std::endl;
      } else if (flag == "help") {
        usage();
        exit(EXIT_SUCCESS);
      } else if (flag == "insecure") {
        settings.tls_verify_peer = false;
        std::clog << "WILL NOT verify the TLS peer (INSECURE!)" << std::endl;
      } else if (flag == "ndt7") {
        settings.protocol_flags |= libndt::protocol_flag_ndt7;
        std::clog << "will use the ndt7 protocol" << std::endl;
      } else if (flag == "random") {
        std::clog << "WARNING: the `-random` flag is deprecated" << std::endl;
        std::clog << "HINT: replace with `-lookup-policy random`" << std::endl;
        settings.mlabns_policy = libndt::mlabns_policy_random;
        std::clog << "will auto-select a random server" << std::endl;
      } else if (flag == "tls") {
        settings.protocol_flags |= libndt::protocol_flag_tls;
        std::clog << "will secure communications using TLS" << std::endl;
      } else if (flag == "upload") {
        settings.nettest_flags |= libndt::nettest_flag_upload;
        std::clog << "will run the upload sub-test" << std::endl;
      } else if (flag == "verbose") {
        settings.verbosity = libndt::verbosity_debug;
        std::clog << "will be verbose" << std::endl;
      } else if (flag == "version") {
        std::cout << libndt::version_major << "." << libndt::version_minor
                  << "." << libndt::version_patch << std::endl;
        exit(EXIT_SUCCESS);
      } else if (flag == "websocket") {
        settings.protocol_flags |= libndt::protocol_flag_websocket;
        std::clog << "will use the NDT-over-WebSocket protocol" << std::endl;
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
      } else if (param.first == "lookup-policy") {
        if (param.second == "closest") {
          settings.mlabns_policy = libndt::mlabns_policy_closest;
        } else if (param.second == "random") {
          settings.mlabns_policy = libndt::mlabns_policy_random;
        } else if (param.second == "geo-options") {
          settings.mlabns_policy = libndt::mlabns_policy_geo_options;
        } else {
          std::clog << "fatal: unrecognized -lookup-policy: " << param.second
                    << std::endl << std::endl;
          usage();
          exit(EXIT_FAILURE);
        }
      } else if (param.first == "port") {
        settings.port = param.second;
        std::clog << "will use this port: " << param.second << std::endl;
      } else if (param.first == "socks5h") {
        settings.socks5h_port = param.second;
        std::clog << "will use the socks5h proxy at: 127.0.0.1:" << param.second << std::endl;
      } else {
        std::clog << "fatal: unrecognized param: " << param.first << std::endl;
        usage();
        exit(EXIT_FAILURE);
      }
    }
    auto sz = cmdline.pos_args().size();
    if (sz != 1 && sz != 2) {
      usage();
      exit(EXIT_FAILURE);
    }
    if (sz == 2) {
      settings.hostname = cmdline.pos_args()[1];
      std::clog << "will use this NDT server: " << cmdline.pos_args()[1] << std::endl;
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
