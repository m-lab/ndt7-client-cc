// Part of Measurement Kit <https://measurement-kit.github.io/>.
// Measurement Kit is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.
#ifndef MEASUREMENT_KIT_LIBNDT_API_HPP
#define MEASUREMENT_KIT_LIBNDT_API_HPP

// TODO(bassosimone): run through cppcheck and attempt to minimize warnings.

/// \file libndt.hpp
///
/// \brief Public header of measurement-kit/libndt. The basic usage is a simple
/// as creating a `libndt::Client c` instance and then calling `c.run()`. More
/// advanced usage may require you to create a subclass of `libndt::Client` and
/// override specific virtual methods to customize the behaviour.
///
/// This implementation provides the C2S and S2C NDT subtests. We implement
/// NDT over TLS and NDT over websocket. For more information on the NDT
/// protocol, \see https://github.com/ndt-project/ndt/wiki/NDTProtocol.
///
/// The NDT protocol described above is version 5 (aka ndt5). The code in this
/// library also implements the ndt7 specification, which is described at
/// \see https://github.com/m-lab/ndt-server/blob/master/spec/ndt7-protocol.md.
///
/// Throughout this file, we'll use NDT to indicate ndt5 and ndt7 explicitly
/// to indicate version 7 of the protocol. Please, use ndt7 in newer code and
/// stick to ndt5 only if backwards compatibility is necessary.
///
/// \remark As a general rule, what is not documented using Doxygen comments
/// inside of this file is considered either internal or experimental. We
/// recommend you to only use documented interfaces.
///
/// Usage example follows. We assume that you have downloaded the single include
/// headers of nlohmann/json >= 3.0.0 and of libndt.
///
/// ```
/// #include "json.hpp"
/// #include "libndt.hpp"
/// measurement_kit::libndt::Client client;
/// client.run();
/// ```
///
/// \warning Not including nlohmann/json before including libndt will cause
/// the build to fail, because libndt uses nlohmann/json symbols.

#ifndef LIBNDT_SINGLE_INCLUDE
#include "libndt/internal/err.hpp"
#include "libndt/internal/sys.hpp"
#include "libndt/internal/curlx.hpp"
#include "libndt/timeout.hpp"
#endif // !LIBNDT_SINGLE_INCLUDE

// Check dependencies
// ``````````````````
#ifndef NLOHMANN_JSON_VERSION_MAJOR
#error "Libndt depends on nlohmann/json. Include nlohmann/json before including libndt."
#endif  // !NLOHMANN_JSON_VERSION_MAJOR
#if NLOHMANN_JSON_VERSION_MAJOR < 3
#error "Libndt requires nlohmann/json >= 3"
#endif

// TODO(bassosimone): these headers should be in impl.hpp and here we
// need to include the bare minimum required by the API

#ifndef _WIN32
#include <sys/socket.h>
#include <arpa/inet.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <assert.h>
#ifndef _WIN32
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <poll.h>
#endif
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

namespace measurement_kit {
namespace libndt {

// Versioning
// ``````````

/// Type containing a version number.
using Version = unsigned int;

/// Major API version number of measurement-kit/libndt.
constexpr Version version_major = Version{0};

/// Minor API version number of measurement-kit/libndt.
constexpr Version version_minor = Version{27};

/// Patch API version number of measurement-kit/libndt.
constexpr Version version_patch = Version{0};

// Flags for selecting subtests
// ````````````````````````````

/// Flags that indicate what subtests to run.
using NettestFlags = unsigned char;

/// Run the upload subtest.
constexpr NettestFlags nettest_flag_upload = NettestFlags{1U << 1};

/// Run the download subtest.
constexpr NettestFlags nettest_flag_download = NettestFlags{1U << 2};

// Verbosity levels
// ````````````````

/// Library's logging verbosity.
using Verbosity = unsigned int;

/// Do not emit any log message.
constexpr Verbosity verbosity_quiet = Verbosity{0};

/// Emit only warning messages.
constexpr Verbosity verbosity_warning = Verbosity{1};

/// Emit warning and informational messages.
constexpr Verbosity verbosity_info = Verbosity{2};

/// Emit all log messages.
constexpr Verbosity verbosity_debug = Verbosity{3};

// Flags for selecting what NDT protocol features to use
// `````````````````````````````````````````````````````

/// Flags to select what protocol should be used.
using ProtocolFlags = unsigned int;

/// When this flag is set we use TLS. This specifically means that we will
/// use TLS channels for the control and the measurement connections.
constexpr ProtocolFlags protocol_flag_tls = ProtocolFlags{1 << 1};

/// When this flag is set we use WebSocket. This specifically means that
/// we use the WebSocket framing to encapsulate NDT messages.
constexpr ProtocolFlags protocol_flag_websocket = ProtocolFlags{1 << 2};

/// When this flag is set, we use ndt7 rather than ndt5. This specifically
/// means that a totally different protocol is used. You can read more on ndt7
/// at https://github.com/m-lab/ndt-server/blob/master/spec/ndt7-protocol.md
constexpr ProtocolFlags protocol_flag_ndt7 = ProtocolFlags{1 << 3};

// Policy for auto-selecting a NDT server
// ``````````````````````````````````````

/// Flags modifying the behavior of mlab-ns. Mlab-ns is the web service used
/// to automatically discover NDT's (and other experiments') servers.
using MlabnsPolicy = unsigned short;

/// Request just the closest NDT server.
constexpr MlabnsPolicy mlabns_policy_closest = MlabnsPolicy{0};

/// Request for a random NDT server.
constexpr MlabnsPolicy mlabns_policy_random = MlabnsPolicy{1};

/// Return a list of nearby NDT servers. When more than one server is returned
/// all the available servers will be tried in case some of them are down.
constexpr MlabnsPolicy mlabns_policy_geo_options = MlabnsPolicy{2};

// EventHandler
// ------------

/// EventHandler handles events.
class EventHandler {
 public:
  /// Called when a warning message is emitted. The default behavior is to write
  /// the warning onto the `std::clog` standard stream. \warning This method
  /// could be called from a different thread context.
  virtual void on_warning(const std::string &s) const noexcept = 0;

  /// Called when an informational message is emitted. The default behavior is
  /// to write the message onto the `std::clog` standard stream. \warning This method
  /// could be called from a different thread context.
  virtual void on_info(const std::string &s) const noexcept = 0;

  /// Called when a debug message is emitted. The default behavior is
  /// to write the message onto the `std::clog` standard stream. \warning This method
  /// could be called from a different thread context.
  virtual void on_debug(const std::string &s) const noexcept = 0;

  /// Called to inform you about the measured speed. The default behavior is
  /// to write the provided information as an info message. @param tid is either
  /// nettest_flag_download or nettest_flag_upload. @param nflows is the number
  /// of used flows. @param measured_bytes is the number of bytes received
  /// or sent since the beginning of the measurement. @param elapsed
  /// is the number of seconds elapsed since the beginning of the nettest.
  /// @param max_runtime is the maximum runtime of this nettest, as copied from
  /// the Settings. @remark By dividing @p elapsed by @p max_runtime, you can
  /// get the percentage of completion of the current nettest. @remark We
  /// provide you with @p tid, so you know whether the nettest is downloading
  /// bytes from the server or uploading bytes to the server. \warning This
  /// method could be called from another thread context.
  virtual void on_performance(NettestFlags tid, uint8_t nflows,
                              double measured_bytes, double elapsed,
                              double max_runtime) noexcept = 0;

  /// Called to provide you with NDT results. The default behavior is
  /// to write the provided information as an info message. @param scope is
  /// "web100", when we're passing you Web 100 variables, "tcp_info" when
  /// we're passing you TCP info variables, "summary" when we're passing you
  /// summary variables, or "ndt7" when we're passing you results returned
  /// by a ndt7 server. @param name is the name of the variable; if @p scope
  /// is "ndt7", then @p name should be "download". @param value is the
  /// variable value; variables are typically int, float, or string when
  /// running ndt5 tests, instead they are serialized JSON returned by the
  /// server when running a ndt7 test. \warning This method could be called
  /// from another thread context.
  virtual void on_result(std::string scope, std::string name,
                         std::string value) noexcept = 0;

  /// Called when the server is busy. The default behavior is to write a
  /// warning message. @param msg is the reason why the server is busy, encoded
  /// according to the NDT protocol. @remark when Settings::hostname is empty,
  /// we will autodiscover one or more servers, depending on the configured
  /// policy; in the event in which we autodiscover more than one server, we
  /// will attempt to use each of them, hence, this method may be called more
  /// than once if some of these servers happen to be busy. \warning This
  /// method could be called from another thread context.
  virtual void on_server_busy(std::string msg) noexcept = 0;

  /// ~EventHandler is the destructor.
  virtual ~EventHandler() noexcept;
};
EventHandler::~EventHandler() noexcept {}

// Settings
// ````````

constexpr const char *ndt_version_compat = "v3.7.0";

/// NDT client settings. If you do not customize the settings when creating
/// a Client, the defaults listed below will be used instead.
class Settings {
 public:
  /// Base URL to be used to query the mlab-ns service. If you specify an
  /// explicit hostname, mlab-ns won't be used. Note that the URL specified
  /// here MUST NOT end with a final slash.
  std::string mlabns_base_url = "https://locate.measurementlab.net";

  /// Flags that modify the behavior of mlabn-ns. By default we use the
  /// geo_options policy that is the most robust to random server failures.
  MlabnsPolicy mlabns_policy = mlabns_policy_geo_options;

  /// Timeout used for I/O operations.
  Timeout timeout = Timeout{7} /* seconds */;

  /// Host name of the NDT server to use. If this is left blank (the default),
  /// we will use mlab-ns to discover a nearby server.
  std::string hostname;

  /// Port of the NDT server to use. If this is not specified, we will use
  /// the most correct port depending on the configuration.
  std::string port;

  /// The tests you want to run with the NDT server. By default we run
  /// a download test, because that is probably the typical usage.
  NettestFlags nettest_flags = nettest_flag_download;

  /// Verbosity of the client. By default no message is emitted. Set to other
  /// values to get more messages (useful when debugging).
  Verbosity verbosity = verbosity_quiet;

  /// Metadata to include in the server side logs. By default we just identify
  /// the NDT version and the application.
  std::map<std::string, std::string> metadata{
      {"client.version", ndt_version_compat},
      {"client.application", "measurement-kit/libndt"},
  };

  /// Type of NDT protocol that you want to use. Selecting the protocol may
  /// cause libndt to use different default settings for the port or for
  /// mlab-ns. Clear text NDT uses port 3001, NDT-over-TLS uses 3010. There
  /// will most likely be servers listening on port 443 in the future, but
  /// they will only support the TLS+WebSocket protocol.
  ProtocolFlags protocol_flags = ProtocolFlags{0};

  /// Maximum time for which a nettest (i.e. download) is allowed to run. After
  /// this time has elapsed, the code will stop downloading (or uploading). It
  /// is meant as a safeguard to prevent the test for running for much more time
  /// than anticipated, due to buffering and/or changing network conditions.
  Timeout max_runtime = Timeout{14} /* seconds */;

  /// SOCKSv5h port to use for tunnelling traffic using, e.g., Tor. If non
  /// empty, all DNS and TCP traffic should be tunnelled over such port.
  std::string socks5h_port;

  /// CA bundle path to be used to verify TLS connections. If you do not
  /// set this variable and you're on Unix, we'll attempt to use some reasonable
  /// default value. Otherwise, the test will fail (unless you set the
  /// tls_verify_peer setting to false, indicating that you do not care about
  /// verifying the peer -- insecure, not recommended).
  std::string ca_bundle_path;

  /// Whether to use the CA bundle and OpenSSL's builtin hostname validation to
  /// make sure we are talking to the correct host. Enabled by default, but it
  /// may be useful sometimes to disable it for testing purposes. You should
  /// not disable this option in general, since doing that is insecure.
  bool tls_verify_peer = true;

  /// Run in "summary only" mode. If this flag is enabled, most log messages are
  /// hidden and the only output on stdout is the test summary.
  bool summary_only = false;
};


// Client
// ``````

/// NDT client. In the typical usage, you just need to construct a Client,
/// optionally providing settings, and to call the run() method. More advanced
/// usage may require you to override methods in a subclass to customize the
/// default behavior. For instance, you may probably want to override the
/// on_result() method that is called when processing NDT results to either
/// show such results to a user or store them on the disk.
class Client : public EventHandler {
 public:
  /// Constructs a Client with default settings.
  Client() noexcept;

  /// Deleted copy constructor.
  Client(const Client &) noexcept = delete;

  /// Deleted copy assignment.
  Client &operator=(const Client &) noexcept = delete;

  /// Deleted move constructor.
  Client(Client &&) noexcept = delete;

  /// Deleted move assignment.
  Client &operator=(Client &&) noexcept = delete;

  /// Constructs a Client with the specified @p settings.
  explicit Client(Settings settings) noexcept;

  /// Destroys a Client.
  virtual ~Client() noexcept;

  /// Runs a NDT test using the configured (or default) settings.
  bool run() noexcept;

  void on_warning(const std::string &s) const noexcept override;

  void on_info(const std::string &s) const noexcept override;

  void on_debug(const std::string &s) const noexcept override;

  void on_performance(NettestFlags tid,
                      uint8_t nflows,
                      double measured_bytes,
                      double elapsed,
                      double max_runtime) noexcept override;

  void on_result(std::string scope, std::string name,
                 std::string value) noexcept override;

  void on_server_busy(std::string msg) noexcept override;

  /*
               _        __             _    _ _                _
   ___ _ _  __| |  ___ / _|  _ __ _  _| |__| (_)__   __ _ _ __(_)
  / -_) ' \/ _` | / _ \  _| | '_ \ || | '_ \ | / _| / _` | '_ \ |
  \___|_||_\__,_| \___/_|   | .__/\_,_|_.__/_|_\__| \__,_| .__/_|
                            |_|                          |_|
  */
  // If you're just interested to use measurement-kit/libndt, you can stop
  // reading right here. All the remainder of this file is not documented on
  // purpose and contains functionality that you'll typically don't care about
  // unless you're looking into heavily customizing this library.
  //
  // SWIG should not see anything below this point otherwise it will attempt
  // to create wrappers for that. TODO(bassosimone): it should be evaluated in
  // the future whether it makes sense enforcing `protected` here. This is
  // certainly feasible but would require some refactoring.
#ifdef SWIG
 private:
#endif

  // High-level API
  virtual void summary() noexcept;
  virtual bool query_mlabns(std::vector<std::string> *) noexcept;

  // ndt7 protocol API
  // `````````````````
  //
  // This API allows you to perform ndt7 tests. The plan is to increasingly
  // use ndt7 code and eventually deprecate and remove NDT.
  //
  // Note that we cannot have ndt7 without OpenSSL.

  // ndt7_download performs a ndt7 download. Returns true if the download
  // succeeds and false in case of failure.
  bool ndt7_download() noexcept;

  // ndt7_upload is like ndt7_download but performs an upload.
  bool ndt7_upload() noexcept;

  // ndt7_connect connects to @p url_path.
  bool ndt7_connect(std::string url_path) noexcept;

  // WebSocket
  // `````````
  //
  // This section contain a WebSocket implementation.

  // Send @p line over @p fd.
  virtual internal::Err ws_sendln(internal::Socket fd, std::string line) noexcept;

  // Receive shorter-than @p maxlen @p *line over @p fd.
  virtual internal::Err ws_recvln(internal::Socket fd, std::string *line, size_t maxlen) noexcept;

  // Perform websocket handshake. @param fd is the socket to use. @param
  // ws_flags specifies what headers to send and to expect (for more information
  // see the ws_f_xxx constants defined below). @param ws_protocol specifies
  // what protocol to specify as Sec-WebSocket-Protocol in the upgrade request.
  // @param port is used to construct the Host header. @param url_path is the
  // URL path to use for performing the websocket upgrade.
  virtual internal::Err ws_handshake(internal::Socket fd, std::string port, uint64_t ws_flags,
                           std::string ws_protocol,
                           std::string url_path) noexcept;

  // Prepare and return a WebSocket frame containing @p first_byte and
  // the content of @p base and @p count as payload. If @p base is nullptr
  // then we'll just not include a body in the prepared frame.
  virtual std::string ws_prepare_frame(uint8_t first_byte, uint8_t *base,
                                       internal::Size count) const noexcept;

  // Send @p count bytes from @p base over @p sock as a frame whose first byte
  // @p first_byte should contain the opcode and possibly the FIN flag.
  virtual internal::Err ws_send_frame(internal::Socket sock, uint8_t first_byte, uint8_t *base,
                            internal::Size count) const noexcept;

  // Receive a frame from @p sock. Puts the opcode in @p *opcode. Puts whether
  // there is a FIN flag in @p *fin. The buffer starts at @p base and it
  // contains @p total bytes. Puts in @p *count the actual number of bytes
  // in the message. @return The error that occurred or Err::none.
  internal::Err ws_recv_any_frame(internal::Socket sock, uint8_t *opcode, bool *fin, uint8_t *base,
                        internal::Size total, internal::Size *count) const noexcept;

  // Receive a frame. Automatically and transparently responds to PING, ignores
  // PONG, and handles CLOSE frames. Arguments like ws_recv_any_frame().
  internal::Err ws_recv_frame(internal::Socket sock, uint8_t *opcode, bool *fin, uint8_t *base,
                    internal::Size total, internal::Size *count) const noexcept;

  // Receive a message consisting of one or more frames. Transparently handles
  // PING and PONG frames. Handles CLOSE frames. @param sock is the socket to
  // use. @param opcode is where the opcode is returned. @param base is the
  // beginning of the buffer. @param total is the size of the buffer. @param
  // count contains the actual message size. @return An error on failure or
  // Err::none in case of success.
  internal::Err ws_recvmsg(internal::Socket sock, uint8_t *opcode, uint8_t *base, internal::Size total,
                 internal::Size *count) const noexcept;

  // Networking layer
  // ````````````````
  //
  // This section contains network functionality used by NDT. The functionality
  // to connect to a remote host is layered to comply with the websocket spec
  // as follows:
  //
  // - netx_maybews_dial() calls netx_maybessl_dial() and, if that succeeds, it
  //   then attempts to negotiate a websocket channel (if enabled);
  //
  // - netx_maybessl_dial() calls netx_maybesocks5h_dial() and, if that
  //   suceeds, it then attempts to establish a TLS connection (if enabled);
  //
  // - netx_maybesocks5h_dial() possibly creates the connection through a
  //   SOCKSv5h proxy (if the proxy is enabled).
  //
  // By default with TLS we use a CA and we perform SNI validation. That can be
  // disabled for debug reasons. Doing that breaks compliancy with the websocket
  // spec. See <https://tools.ietf.org/html/rfc6455#section-4.1>.

  // Connect to @p hostname and @p port possibly using WebSocket,
  // SSL, and SOCKSv5. This depends on the Settings. See the documentation
  // of ws_handshake() for more info on @p ws_flags, @p ws_protocol, and
  // @p url_path.
  virtual internal::Err netx_maybews_dial(const std::string &hostname,
                                const std::string &port, uint64_t ws_flags,
                                std::string ws_protocol, std::string url_path,
                                internal::Socket *sock) noexcept;

  // Connect to @p hostname and @p port possibly using SSL and SOCKSv5. This
  // depends on the Settings you configured.
  virtual internal::Err netx_maybessl_dial(const std::string &hostname,
                                 const std::string &port,
                                 internal::Socket *sock) noexcept;

  // Connect to @p hostname and @port possibly using SOCKSv5. This depends
  // on the Settings you configured.
  virtual internal::Err netx_maybesocks5h_dial(const std::string &hostname,
                                     const std::string &port,
                                     internal::Socket *sock) noexcept;

  // Map errno code into a Err value.
  static internal::Err netx_map_errno(int ec) noexcept;

  // Map getaddrinfo return value into a Err value.
  internal::Err netx_map_eai(int ec) noexcept;

  // Connect to @p hostname and @p port.
  virtual internal::Err netx_dial(const std::string &hostname, const std::string &port,
                        internal::Socket *sock) noexcept;

  // Receive from the network.
  virtual internal::Err netx_recv(internal::Socket fd, void *base, internal::Size count,
                        internal::Size *actual) const noexcept;

  // Receive from the network without blocking.
  virtual internal::Err netx_recv_nonblocking(internal::Socket fd, void *base, internal::Size count,
                                    internal::Size *actual) const noexcept;

  // Receive exactly N bytes from the network.
  virtual internal::Err netx_recvn(internal::Socket fd, void *base, internal::Size count) const noexcept;

  // Send data to the network.
  virtual internal::Err netx_send(internal::Socket fd, const void *base, internal::Size count,
                        internal::Size *actual) const noexcept;

  // Send to the network without blocking.
  virtual internal::Err netx_send_nonblocking(internal::Socket fd, const void *base, internal::Size count,
                                    internal::Size *actual) const noexcept;

  // Send exactly N bytes to the network.
  virtual internal::Err netx_sendn(
    internal::Socket fd, const void *base, internal::Size count) const noexcept;

  // Resolve hostname into a list of IP addresses.
  virtual internal::Err netx_resolve(const std::string &hostname,
                           std::vector<std::string> *addrs) noexcept;

  // Set socket non blocking.
  virtual internal::Err netx_setnonblocking(internal::Socket fd, bool enable) noexcept;

  // Pauses until the socket becomes readable.
  virtual internal::Err netx_wait_readable(internal::Socket, Timeout timeout) const noexcept;

  // Pauses until the socket becomes writeable.
  virtual internal::Err netx_wait_writeable(internal::Socket, Timeout timeout) const noexcept;

  // Main function for dealing with I/O patterned after poll(2).
  virtual internal::Err netx_poll(
    std::vector<pollfd> *fds, int timeout_msec) const noexcept;

  // Shutdown both ends of a socket.
  virtual internal::Err netx_shutdown_both(internal::Socket fd) noexcept;

  // Close a socket.
  virtual internal::Err netx_closesocket(internal::Socket fd) noexcept;

  virtual bool query_mlabns_curl(const std::string &url, long timeout,
                                 std::string *body) noexcept;

  // Other helpers

  Verbosity get_verbosity() const noexcept;

  // Reference to overridable system dependencies
  std::unique_ptr<internal::Sys> sys{new internal::Sys{}};

 protected:
  // SummaryData contains the fields that are needed to generate the summary
  // at the end of the tests.
  struct SummaryData {
      // download speed in kbit/s.
      double download_speed;

      // upload speed in kbit/s.
      double upload_speed;

      // download retransmission rate (bytes_retrans / bytes_sent).
      double download_retrans;

      // upload retransmission rate (bytes_retrans / bytes_sent).
      double upload_retrans;

      // TCPInfo's MinRTT (microseconds).
      uint32_t min_rtt;
  };

  SummaryData summary_;

  // NDT web100 summary data.
  nlohmann::json web100;

  // ndt7 Measurement object.
  nlohmann::json measurement_;

  // ndt7 ConnectionInfo object.
  nlohmann::json connection_info_;

 private:
  class Winsock {
   public:
    Winsock() noexcept;
    Winsock(const Winsock &) = delete;
    Winsock &operator=(const Winsock &) = delete;
    Winsock(Winsock &&) = delete;
    Winsock &operator=(Winsock &&) = delete;
    ~Winsock() noexcept;
  };

  internal::Socket sock_ = (internal::Socket)-1;
  std::vector<NettestFlags> granted_suite_;
  Settings settings_;

  std::map<internal::Socket, SSL *> fd_to_ssl_;
#ifdef _WIN32
  Winsock winsock_;
#endif
};

#ifdef __linux__
#include <linux/tcp.h>
#define NDT7_ENUM_TCP_INFO \
  XX(tcpi_state, TcpiState) \
  XX(tcpi_ca_state, TcpiCaState) \
  XX(tcpi_retransmits, TcpiRetransmits) \
  XX(tcpi_probes, TcpiProbes) \
  XX(tcpi_backoff, TcpiBackoff) \
  XX(tcpi_options, TcpiOptions) \
  XX(tcpi_snd_wscale, TcpiSndWscale) \
  XX(tcpi_rcv_wscale, TcpiRcvWscale) \
  XX(tcpi_delivery_rate_app_limited, TcpiDeliveryRateAppLimited) \
  XX(tcpi_rto, TcpiRto) \
  XX(tcpi_ato, TcpiAto) \
  XX(tcpi_snd_mss, TcpiSndMss) \
  XX(tcpi_rcv_mss, TcpiRcvMss) \
  XX(tcpi_unacked, TcpiUnacked) \
  XX(tcpi_sacked, TcpiSacked) \
  XX(tcpi_lost, TcpiLost) \
  XX(tcpi_retrans, TcpiRetrans) \
  XX(tcpi_fackets, TcpiFackets) \
  XX(tcpi_last_data_sent, TcpiLastDataSent) \
  XX(tcpi_last_ack_sent, TcpiLastAckSent) \
  XX(tcpi_last_data_recv, TcpiLastDataRecv) \
  XX(tcpi_last_ack_recv, TcpiLastAckRecv) \
  XX(tcpi_pmtu, TcpiPmtu) \
  XX(tcpi_rcv_ssthresh, TcpiRcvSsthresh) \
  XX(tcpi_rtt, TcpiRtt) \
  XX(tcpi_rttvar, TcpiRttvar) \
  XX(tcpi_snd_ssthresh, TcpiSndSsthresh) \
  XX(tcpi_snd_cwnd, TcpiSndCwnd) \
  XX(tcpi_advmss, TcpiAdvmss) \
  XX(tcpi_reordering, TcpiReordering) \
  XX(tcpi_rcv_rtt, TcpiRcvRtt) \
  XX(tcpi_rcv_space, TcpiRcvSpace) \
  XX(tcpi_total_retrans, TcpiTotalRetrans) \
  XX(tcpi_pacing_rate, TcpiPacingRate) \
  XX(tcpi_max_pacing_rate, TcpiMaxPacingRate) \
  XX(tcpi_bytes_acked, TcpiBytesAcked) \
  XX(tcpi_bytes_received, TcpiBytesReceived) \
  XX(tcpi_segs_out, TcpiSegsOut) \
  XX(tcpi_segs_in, TcpiSegsIn) \
  XX(tcpi_notsent_bytes, TcpiNotsentBytes) \
  XX(tcpi_min_rtt, TcpiMinRtt) \
  XX(tcpi_data_segs_in, TcpiDataSegsIn) \
  XX(tcpi_data_segs_out, TcpiDataSegsOut) \
  XX(tcpi_delivery_rate, TcpiDeliveryRate) \
  XX(tcpi_busy_time, TcpiBusyTime) \
  XX(tcpi_rwnd_limited, TcpiRwndLimited) \
  XX(tcpi_sndbuf_limited, TcpiSndbufLimited) \
  XX(tcpi_delivered, TcpiDelivered) \
  XX(tcpi_delivered_ce, TcpiDeliveredCe) \
  XX(tcpi_bytes_sent, TcpiBytesSent) \
  XX(tcpi_bytes_retrans, TcpiBytesRetrans) \
  XX(tcpi_dsack_dups, TcpiDsackDups) \
  XX(tcpi_reord_seen, TcpiReordSeen)
#endif // __linux__

// WebSocket constants
// ```````````````````

// Opcodes. See <https://tools.ietf.org/html/rfc6455#section-11.8>.
constexpr uint8_t ws_opcode_continue = 0;
constexpr uint8_t ws_opcode_text = 1;
constexpr uint8_t ws_opcode_binary = 2;
constexpr uint8_t ws_opcode_close = 8;
constexpr uint8_t ws_opcode_ping = 9;
constexpr uint8_t ws_opcode_pong = 10;

// Constants useful to process the first octet of a websocket frame. For more
// info see <https://tools.ietf.org/html/rfc6455#section-5.2>.
constexpr uint8_t ws_fin_flag = 0x80;
constexpr uint8_t ws_reserved_mask = 0x70;
constexpr uint8_t ws_opcode_mask = 0x0f;

// Constants useful to process the second octet of a websocket frame. For more
// info see <https://tools.ietf.org/html/rfc6455#section-5.2>.
constexpr uint8_t ws_mask_flag = 0x80;
constexpr uint8_t ws_len_mask = 0x7f;

// Flags used to specify what HTTP headers are required and present into the
// websocket handshake where we upgrade from HTTP/1.1 to websocket.
constexpr uint64_t ws_f_connection = 1 << 0;
constexpr uint64_t ws_f_sec_ws_accept = 1 << 1;
constexpr uint64_t ws_f_sec_ws_protocol = 1 << 2;
constexpr uint64_t ws_f_upgrade = 1 << 3;

// Values of Sec-WebSocket-Protocol used by ndt-project/ndt.
constexpr const char *ws_proto_control = "ndt";
constexpr const char *ws_proto_c2s = "c2s";
constexpr const char *ws_proto_s2c = "s2c";
constexpr const char *ws_proto_ndt7 = "net.measurementlab.ndt.v7";

// Private constants
// `````````````````

constexpr auto max_loops = 256;
constexpr char msg_kickoff[] = "123456 654321";
constexpr size_t msg_kickoff_size = sizeof(msg_kickoff) - 1;

// Private utils
// `````````````

// Generic macro for emitting logs.
#define LIBNDT_EMIT_LOG_EX(client, level, statements)      \
  do {                                                     \
    if (client->get_verbosity() >= verbosity_##level) {    \
      std::stringstream ss_log_lines;                      \
      ss_log_lines << statements;                          \
      std::string log_line;                                \
      while (std::getline(ss_log_lines, log_line, '\n')) { \
        if (!log_line.empty()) {                           \
          client->on_##level(std::move(log_line));         \
        }                                                  \
      }                                                    \
    }                                                      \
  } while (0)

#define LIBNDT_EMIT_WARNING_EX(clnt, stmnts) LIBNDT_EMIT_LOG_EX(clnt, warning, stmnts)
#define LIBNDT_EMIT_INFO_EX(clnt, stmnts) LIBNDT_EMIT_LOG_EX(clnt, info, stmnts)
#define LIBNDT_EMIT_DEBUG_EX(clnt, stmnts) LIBNDT_EMIT_LOG_EX(clnt, debug, stmnts)

#define LIBNDT_EMIT_WARNING(statements) LIBNDT_EMIT_WARNING_EX(this, statements)
#define LIBNDT_EMIT_INFO(statements) LIBNDT_EMIT_INFO_EX(this, statements)
#define LIBNDT_EMIT_DEBUG(statements) LIBNDT_EMIT_DEBUG_EX(this, statements)

#ifdef _WIN32
#define LIBNDT_OS_SHUT_RDWR SD_BOTH
#else
#define LIBNDT_OS_SHUT_RDWR SHUT_RDWR
#endif

static void random_printable_fill(char *buffer, size_t length) noexcept {
  static const std::string ascii =
      " !\"#$%&\'()*+,-./"          // before numbers
      "0123456789"                  // numbers
      ":;<=>?@"                     // after numbers
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"  // uppercase
      "[\\]^_`"                     // between upper and lower
      "abcdefghijklmnopqrstuvwxyz"  // lowercase
      "{|}~"                        // final
      ;
  // TODO(bassosimone): the random device is not actually random in a
  // mingw environment. Here we should perhaps take advantage of the
  // OpenSSL dependency, when available, and use OpenSSL.
  std::random_device rd;
  std::mt19937 g(rd());
  for (size_t i = 0; i < length; ++i) {
    buffer[i] = ascii[g() % ascii.size()];
  }
}

static double compute_speed_kbits(double data, double elapsed) noexcept {
  return (elapsed > 0.0) ? ((data * 8.0) / 1000.0 / elapsed) : 0.0;
}

// format_speed_from_kbits format the input speed, which must be in kbit/s, to
// a string describing the speed with a measurement unit.
static std::string format_speed_from_kbits(double speed) noexcept {
  std::string unit = "kbit/s";
  if (speed > 1000) {
    unit = "Mbit/s";
    speed /= 1000;
    if (speed > 1000) {
      unit = "Gbit/s";
      speed /= 1000;
    }
  }
  std::stringstream ss;
  ss << std::setprecision(3) << std::setw(6) << std::right
      << speed << " " << unit;
  return ss.str();
}

static std::string format_speed_from_kbits(double data, double elapsed) noexcept {
  return format_speed_from_kbits(compute_speed_kbits(data, elapsed));
}

static std::string represent(std::string message) noexcept {
  bool printable = true;
  for (auto &c : message) {
    if (c < ' ' || c > '~') {
      printable = false;
      break;
    }
  }
  if (printable) {
    return message;
  }
  std::stringstream ss;
  ss << "binary([";
  for (auto &c : message) {
    if (c <= ' ' || c > '~') {
      ss << "<0x" << std::fixed << std::setw(2) << std::setfill('0') << std::hex
         << (unsigned)(uint8_t)c << ">";
    } else {
      ss << (char)c;
    }
  }
  ss << "])";
  return ss.str();
}

// Private classes
// ```````````````

#ifdef _WIN32
// "There must be a call to WSACleanup for each successful call
//  to WSAStartup. Only the final WSACleanup function call performs
//  the actual cleanup. The preceding calls simply decrement
//  an internal reference count in the WS2_32.DLL."

Client::Winsock::Winsock() noexcept {
  WORD requested = MAKEWORD(2, 2);
  WSADATA data;
  if (::WSAStartup(requested, &data) != 0) {
    abort();
  }
}

Client::Winsock::~Winsock() noexcept {
  if (::WSACleanup() != 0) {
    abort();
  }
}
#endif  // _WIN32

class SocketVector {
 public:
  SocketVector(Client *c) noexcept;
  ~SocketVector() noexcept;
  Client *owner = nullptr;
  std::vector<internal::Socket> sockets;
};

SocketVector::SocketVector(Client *c) noexcept : owner{c} {}

SocketVector::~SocketVector() noexcept {
  if (owner != nullptr) {
    for (auto &fd : sockets) {
      owner->netx_closesocket(fd);
    }
  }
}

// Client constructor and destructor
// `````````````````````````````````

Client::Client() noexcept {}

Client::Client(Settings settings) noexcept : Client::Client() {
  std::swap(settings_, settings);
}

Client::~Client() noexcept {
  if (sock_ != -1) {
    netx_closesocket(sock_);
  }
}

// Top-level API
// `````````````

bool Client::run() noexcept {
  std::vector<std::string> fqdns;
  if (!query_mlabns(&fqdns)) {
    return false;
  }
  for (auto &fqdn : fqdns) {
    LIBNDT_EMIT_DEBUG("trying to connect to " << fqdn);
    settings_.hostname = fqdn;
    // TODO(bassosimone): we will eventually want to refactor the code to
    // make ndt7 the default and ndt5 the optional case.
    if ((settings_.protocol_flags & protocol_flag_ndt7) != 0) {
      LIBNDT_EMIT_DEBUG("using the ndt7 protocol");
      if ((settings_.nettest_flags & nettest_flag_download) != 0) {
        // TODO(bassosimone): for now we do not try with more than one host
        // when using ndt7 and there's a failure. We may want to do that.
        if (!ndt7_download()) {
          LIBNDT_EMIT_WARNING("ndt7: download failed");
          // FALLTHROUGH
        }
      }
      if ((settings_.nettest_flags & nettest_flag_upload) != 0) {
        // TODO(bassosimone): same as above.
        if (!ndt7_upload()) {
          LIBNDT_EMIT_WARNING("ndt7: upload failed");
          // FALLTHROUGH
        }
      }
      LIBNDT_EMIT_INFO("ndt7: test complete");
      // TODO(bassosimone): here we may want to warn if the user selects
      // subtests that we actually do not implement.
      return true;
    }
    LIBNDT_EMIT_DEBUG("connection closed");
    return true;
  }
  LIBNDT_EMIT_WARNING("no more hosts to try; failing the test");
  return false;
}

void Client::on_warning(const std::string &msg) const noexcept {
  std::clog << "[!] " << msg << std::endl;
}

void Client::on_info(const std::string &msg) const noexcept {
  std::clog << msg << std::endl;
}

void Client::on_debug(const std::string &msg) const noexcept {
  std::clog << "[D] " << msg << std::endl;
}

void Client::on_performance(NettestFlags tid, uint8_t nflows,
                            double measured_bytes,
                            double elapsed_time, double max_runtime) noexcept {
  auto percent = 0.0;
  if (max_runtime > 0.0) {
    percent = (elapsed_time * 100.0 / max_runtime);
  }
  LIBNDT_EMIT_INFO("  [" << std::fixed << std::setprecision(0) << std::setw(2)
                  << std::right << percent << "%] speed: "
                  << format_speed_from_kbits(measured_bytes, elapsed_time));

  LIBNDT_EMIT_DEBUG("  [" << std::fixed << std::setprecision(0) << std::setw(2)
                  << std::right << percent << "%]"
                  << " elapsed: " << std::fixed << std::setprecision(3)
                  << std::setw(6) << elapsed_time << " s;"
                  << " test_id: " << (int)tid << "; num_flows: " << (int)nflows
                  << "; measured_bytes: " << measured_bytes);
}

void Client::on_result(std::string scope, std::string name, std::string value) noexcept {
  LIBNDT_EMIT_INFO("  - [" << scope << "] " << name << ": " << value);
}

void Client::on_server_busy(std::string msg) noexcept {
  LIBNDT_EMIT_WARNING("server is busy: " << msg);
}

// High-level API
// ``````````````

void Client::summary() noexcept {
  LIBNDT_EMIT_INFO(std::endl << "[Test results]");
  if (summary_.download_speed != 0.0) {
    LIBNDT_EMIT_INFO("Download speed: "
      << format_speed_from_kbits(summary_.download_speed));
  }
  if (summary_.upload_speed != 0.0) {
    LIBNDT_EMIT_INFO("Upload speed: "
      << format_speed_from_kbits(summary_.upload_speed));
  }
  if (summary_.min_rtt != 0) {
    LIBNDT_EMIT_INFO("Latency: " << std::fixed << std::setprecision(2)
      << (summary_.min_rtt / 1000.0) << " ms");
  }
  if (summary_.download_retrans != 0.0) {
      LIBNDT_EMIT_INFO("Download retransmission: "
        << std::fixed << std::setprecision(2)
        << (summary_.download_retrans * 100) << "%");
  }
  if (summary_.upload_retrans != 0.0) {
      LIBNDT_EMIT_INFO("Upload retransmission: "
        << std::fixed << std::setprecision(2)
        << (summary_.upload_retrans * 100) << "%");
  }
  if (web100 != nullptr) {
    LIBNDT_EMIT_DEBUG("web100: " << web100.dump());
  }
}

bool Client::query_mlabns(std::vector<std::string> *fqdns) noexcept {
  assert(fqdns != nullptr);
  if (!settings_.hostname.empty()) {
    LIBNDT_EMIT_DEBUG("no need to query mlab-ns; we have hostname");
    // When we already know the hostname that we want to use just fake out the
    // result of a mlabns query as like mlabns returned that hostname.
    fqdns->push_back(std::move(settings_.hostname));
    return true;
  }
  std::string mlabns_url = settings_.mlabns_base_url;
  if ((settings_.protocol_flags & protocol_flag_ndt7) != 0) {
    mlabns_url += "/ndt7";
  } else if ((settings_.protocol_flags & protocol_flag_tls) != 0) {
    mlabns_url += "/ndt_ssl";
  } else {
    mlabns_url += "/ndt";
  }
  if (settings_.mlabns_policy == mlabns_policy_random) {
    mlabns_url += "?policy=random";
  } else if (settings_.mlabns_policy == mlabns_policy_geo_options) {
    mlabns_url += "?policy=geo_options";
  }
  std::string body;
  if (!query_mlabns_curl(mlabns_url, settings_.timeout, &body)) {
    return false;
  }
  LIBNDT_EMIT_DEBUG("mlabns reply: " << body);
  nlohmann::json json;
  try {
    json = nlohmann::json::parse(body);
  } catch (const nlohmann::json::exception &exc) {
    LIBNDT_EMIT_WARNING("cannot parse JSON: " << exc.what());
    return false;
  }
  // In some cases mlab-ns returns a single object but in other cases (e.g.
  // with the `geo_options` policy) it returns an array. Always make an
  // array so that we can write uniform code for processing mlab-ns response.
  if (json.is_object()) {
    auto array = nlohmann::json::array();
    array.push_back(json);
    std::swap(json, array);
  }
  for (auto &entry : json) {
    std::string fqdn;
    try {
      fqdn = entry.at("fqdn").get<std::string>();
    } catch (const nlohmann::json::exception &exc) {
      LIBNDT_EMIT_WARNING("cannot access FQDN field: " << exc.what());
      return false;
    }
    LIBNDT_EMIT_DEBUG("discovered host: " << fqdn);
    fqdns->push_back(std::move(fqdn));
  }
  return true;
}

// ndt7 protocol API
// `````````````````

bool Client::ndt7_download() noexcept {
  LIBNDT_EMIT_INFO("starting ndt7 download test");
  if (!ndt7_connect("/ndt/v7/download")) {
    return false;
  }
  // The following value is the maximum amount of bytes that an implementation
  // SHOULD be prepared to handle when receiving ndt7 messages.
  constexpr internal::Size ndt7_bufsiz = (1 << 24);
  std::unique_ptr<uint8_t[]> buff{new uint8_t[ndt7_bufsiz]};
  auto begin = std::chrono::steady_clock::now();
  auto latest = begin;
	internal::Size total = 0;
  std::chrono::duration<double> elapsed;
  summary_.download_speed = 0.0;
  summary_.download_retrans = 0.0;
  summary_.min_rtt = 0;
  for (;;) {
    auto now = std::chrono::steady_clock::now();
    elapsed = now - begin;
    if (elapsed.count() > settings_.max_runtime) {
      LIBNDT_EMIT_WARNING("ndt7: download running for too much time");
      return false;
    }
    constexpr auto measurement_interval = 0.25;
    std::chrono::duration<double> interval = now - latest;
    if (interval.count() > measurement_interval) {
      if (!settings_.summary_only) {
        on_performance(nettest_flag_download, 1, static_cast<double>(total),
                     elapsed.count(), settings_.max_runtime);
      }
      latest = now;
    }
    uint8_t opcode = 0;
		internal::Size count = 0;
		internal::Err err = ws_recvmsg(sock_, &opcode, buff.get(), ndt7_bufsiz, &count);
    if (err != internal::Err::none) {
      if (err == internal::Err::eof) {
        break;
      }
      return false;
    }
    if (opcode == ws_opcode_text) {
      // The following is an issue both on armv7 and on Windows 32 bit: the
      // definition of size we have chose is such that later conversion to
      // string is problematic because our size is 64 bit while size_t is 32
      // bit on the platfrom. That said, it's unlikely that the we'll get a
      // measurement that big, so the check to make sure the casting is okay
      // is not going to be a real problem, it's just a theoric issue.
      if (count <= SIZE_MAX) {
        std::string sinfo{(const char *)buff.get(), (size_t)count};
        // Try parsing the received message as JSON.
        try {
          measurement_ = nlohmann::json::parse(sinfo);
          if (measurement_.contains("ConnectionInfo")) {
            connection_info_ = measurement_["ConnectionInfo"];
          }

          // Calculate retransmission rate (BytesRetrans / BytesSent).
          try {
            nlohmann::json tcpinfo_json = measurement_["TCPInfo"];
            double bytes_retrans = (double) tcpinfo_json["BytesRetrans"].get<int64_t>();
            double bytes_sent = (double) tcpinfo_json["BytesSent"].get<int64_t>();
            summary_.download_retrans = (bytes_sent != 0.0) ? bytes_retrans / bytes_sent : 0.0;
            summary_.min_rtt = tcpinfo_json["MinRTT"].get<uint32_t>();
          } catch(const std::exception& e) {
            LIBNDT_EMIT_WARNING("TCPInfo not available, cannot get \
              retransmission rate and latency: " << e.what());
          }
        } catch (nlohmann::json::parse_error& e) {
          LIBNDT_EMIT_WARNING("Unable to parse message as JSON: " << sinfo);
        }

        if (get_verbosity() == verbosity_debug) {
          on_result("ndt7", "download", std::move(sinfo));
        }
      }
    }
    total += count;  // Assume we won't overflow
  }
  summary_.download_speed = compute_speed_kbits(static_cast<double>(total), elapsed.count());
  return true;
}

bool Client::ndt7_upload() noexcept {
  LIBNDT_EMIT_INFO("starting ndt7 upload test");
  if (!ndt7_connect("/ndt/v7/upload")) {
    return false;
  }
  // Implementation note: we send messages smaller than the maximum message
  // size accepted by the protocol. We have chosen this value because it
  // currently seems to be a reasonable size for outgoing messages.
  constexpr internal::Size ndt7_bufsiz = (1 << 13);
  std::unique_ptr<uint8_t[]> buff{new uint8_t[ndt7_bufsiz]};
  random_printable_fill((char *)buff.get(), ndt7_bufsiz);
  // The following is the expected ndt7 transfer time for a subtest.
  constexpr double max_upload_time = 10.0;
  auto begin = std::chrono::steady_clock::now();
  auto latest = begin;
  std::chrono::duration<double> elapsed;
	internal::Size total = 0;
  summary_.upload_speed = 0.0;
  std::string frame = ws_prepare_frame(ws_opcode_binary | ws_fin_flag,
                                       buff.get(), ndt7_bufsiz);
  for (;;) {
    auto now = std::chrono::steady_clock::now();
    elapsed = now - begin;
    std::chrono::duration<double, std::micro> elapsed_usec =
      std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
    if (elapsed.count() > max_upload_time) {
      LIBNDT_EMIT_DEBUG("ndt7: upload has run for enough time");
      break;
    }
    constexpr auto measurement_interval = 0.25;
    std::chrono::duration<double> interval = now - latest;
    if (interval.count() > measurement_interval) {
      nlohmann::json measurement;
      measurement["AppInfo"] = nlohmann::json();
      measurement["AppInfo"]["ElapsedTime"] = (std::uint64_t) elapsed_usec.count();
      measurement["AppInfo"]["NumBytes"] = total;
#ifdef __linux__
      // Read tcp_info data for the socket and print it as JSON.
      struct tcp_info tcpinfo{};
      socklen_t tcpinfolen = sizeof(tcpinfo);
      if (sys->Getsockopt(sock_, IPPROTO_TCP, TCP_INFO, (void *)&tcpinfo,
                          &tcpinfolen) == 0) {
        measurement["TCPInfo"] = nlohmann::json();
        measurement["TCPInfo"]["ElapsedTime"] = (std::uint64_t) elapsed_usec.count();
#define XX(lower_, upper_) measurement["TCPInfo"][#upper_] = (uint64_t)tcpinfo.lower_;
        NDT7_ENUM_TCP_INFO
#undef XX
      }

      // Calculate retransmission rate.
      try {
        nlohmann::json tcpinfo_json = measurement["TCPInfo"];
        double bytes_retrans = (double) tcpinfo_json["TcpiBytesRetrans"].get<int64_t>();
        double bytes_sent = (double) tcpinfo_json["TcpiBytesSent"].get<int64_t>();
        summary_.upload_retrans = (bytes_sent != 0.0) ? bytes_retrans / bytes_sent : 0.0;
      } catch (const std::exception& e) {
        LIBNDT_EMIT_WARNING("Cannot calculate retransmission rate: " << e.what());
      }
#endif  // __linux__
      if (!settings_.summary_only) {
        on_performance(nettest_flag_upload, 1, static_cast<double>(total),
                     elapsed.count(), max_upload_time);
      }
      // This could fail if there are non-utf8 characters. This structure just
      // contains integers and ASCII strings, so we should be good.
      std::string json = measurement.dump();
      if (get_verbosity() == verbosity_debug) {
        on_result("ndt7", "upload", json);
      }
      // Send measurement to the server.
			internal::Err err = ws_send_frame(sock_, ws_opcode_text | ws_fin_flag,
                              (uint8_t *)json.data(), json.size());
      if (err != internal::Err::none) {
        LIBNDT_EMIT_WARNING("ndt7: cannot send measurement");
        return false;
      }
      latest = now;
    }
		internal::Err err = netx_sendn(sock_, frame.data(), frame.size());
    if (err != internal::Err::none) {
      LIBNDT_EMIT_WARNING("ndt7: cannot send frame");
      return false;
    }
    total += ndt7_bufsiz;  // Assume we won't overflow
  }
  summary_.upload_speed = compute_speed_kbits(static_cast<double>(total), elapsed.count());
  return true;
}

bool Client::ndt7_connect(std::string url_path) noexcept {
  std::string port = "443";
  if (!settings_.port.empty()) {
    port = settings_.port;
  }
  // Don't leak resources if the socket is already open.
  if (internal::IsSocketValid(sock_)) {
    LIBNDT_EMIT_DEBUG("ndt7: closing socket openned in previous attempt");
    (void)netx_closesocket(sock_);
    sock_ = (internal::Socket)-1;
  }
  // Note: ndt7 implies WebSocket and TLS
  settings_.protocol_flags |= protocol_flag_websocket | protocol_flag_tls;
	internal::Err err = netx_maybews_dial(
      settings_.hostname, port,
      ws_f_connection | ws_f_upgrade | ws_f_sec_ws_accept |
          ws_f_sec_ws_protocol,
      ws_proto_ndt7, url_path, &sock_);
  if (err != internal::Err::none) {
    return false;
  }
  LIBNDT_EMIT_DEBUG("ndt7: WebSocket connection established");
  return true;
}

// WebSocket
// `````````
// This section contains the websocket implementation. Although this has been
// written from scratch while reading the RFC, it has beem very useful to be
// able to see the websocket implementation in ndt-project/ndt, to have another
// clear, simple existing implementation to compare with.
//
// - - - BEGIN WEBSOCKET IMPLEMENTATION - - - {

internal::Err Client::ws_sendln(internal::Socket fd, std::string line) noexcept {
  LIBNDT_EMIT_DEBUG("> " << line);
  line += "\r\n";
  return netx_sendn(fd, line.c_str(), line.size());
}

internal::Err Client::ws_recvln(internal::Socket fd, std::string *line, size_t maxlen) noexcept {
  if (line == nullptr || maxlen <= 0) {
    return internal::Err::invalid_argument;
  }
  line->reserve(maxlen);
  line->clear();
  while (line->size() < maxlen) {
    char ch = {};
    auto err = netx_recvn(fd, &ch, sizeof(ch));
    if (err != internal::Err::none) {
      return err;
    }
    if (ch == '\r') {
      continue;
    }
    if (ch == '\n') {
      LIBNDT_EMIT_DEBUG("< " << *line);
      return internal::Err::none;
    }
    *line += ch;
  }
  LIBNDT_EMIT_WARNING("ws_recvln: line too long");
  return internal::Err::value_too_large;
}

internal::Err Client::ws_handshake(internal::Socket fd, std::string port, uint64_t ws_flags,
                         std::string ws_proto, std::string url_path) noexcept {
  std::string proto_header;
  {
    proto_header += "Sec-WebSocket-Protocol: ";
    proto_header += ws_proto;
  }
  {
    // Implementation note: we use the default WebSocket key provided in the RFC
    // so that we don't need to depend on OpenSSL for websocket.
    //
    // TODO(bassosimone): replace this with a randomly selected value that
    // varies for each connection. Or we're not compliant.
    constexpr auto key_header = "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==";
    std::stringstream host_header;
    host_header << "Host: " << settings_.hostname;
    // Adding nonstandard port as specified in RFC6455 Sect. 4.1.
    if ((settings_.protocol_flags & protocol_flag_tls) != 0) {
      if (port != "443") {
        host_header << ":" << port;
      }
    } else {
      if (port != "80") {
        host_header << ":" << port;
      }
    }
    std::stringstream request_line;
    request_line << "GET " << url_path << " HTTP/1.1";
		internal::Err err = internal::Err::none;
    if ((err = ws_sendln(fd, request_line.str())) != internal::Err::none ||
        (err = ws_sendln(fd, host_header.str())) != internal::Err::none ||
        (err = ws_sendln(fd, "Upgrade: websocket")) != internal::Err::none ||
        (err = ws_sendln(fd, "Connection: Upgrade")) != internal::Err::none ||
        (err = ws_sendln(fd, key_header)) != internal::Err::none ||
        (err = ws_sendln(fd, proto_header)) != internal::Err::none ||
        (err = ws_sendln(fd, "Sec-WebSocket-Version: 13")) != internal::Err::none ||
        (err = ws_sendln(fd, "")) != internal::Err::none) {
      LIBNDT_EMIT_WARNING("ws_handshake: cannot send HTTP upgrade request");
      return err;
    }
  }
  LIBNDT_EMIT_DEBUG("ws_handshake: sent HTTP/1.1 upgrade request");
  //
  // Limitations of the response processing code
  // ```````````````````````````````````````````
  // Apart from the limitations explicitly identified with TODO messages, the
  // algorithm to process the response has the following limitations:
  //
  // 1. we do not follow redirects (but we're not required to)
  //
  // 2. we do not fail the connection if the Sec-WebSocket-Extensions header is
  //    part of the handshake response (it would mean that an extension we do
  //    not support is being enforced by the server)
  //
  {
    // TODO(bassosimone): use the same value used by ndt-project/ndt
    static constexpr size_t max_line_length = 8000;
    std::string line;
    auto err = ws_recvln(fd, &line, max_line_length);
    if (err != internal::Err::none) {
      return err;
    }
    // TODO(bassosimone): ignore text after 101
    if (line != "HTTP/1.1 101 Switching Protocols") {
      LIBNDT_EMIT_WARNING("ws_handshake: unexpected response line");
      return internal::Err::ws_proto;
    }
    uint64_t flags = 0;
    // TODO(bassosimone): use the same value used by ndt-project/ndt
    constexpr size_t max_headers = 1000;
    for (size_t i = 0; i < max_headers; ++i) {
      // TODO(bassosimone): make header processing case insensitive.
      auto recvln_err = ws_recvln(fd, &line, max_line_length);
      if (recvln_err != internal::Err::none) {
        return recvln_err;
      }
      if (line == "Upgrade: websocket") {
        flags |= ws_f_upgrade;
      } else if (line == "Connection: Upgrade") {
        flags |= ws_f_connection;
      } else if (line == "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") {
        flags |= ws_f_sec_ws_accept;
      } else if (line == proto_header) {
        flags |= ws_f_sec_ws_protocol;
      } else if (line == "") {
        if ((flags & ws_flags) != ws_flags) {
          LIBNDT_EMIT_WARNING("ws_handshake: received incorrect handshake");
          return internal::Err::ws_proto;
        }
        LIBNDT_EMIT_DEBUG("ws_handshake: complete");
        return internal::Err::none;
      }
    }
  }
  LIBNDT_EMIT_DEBUG("ws_handshake: got too many headers");
  return internal::Err::value_too_large;
}

std::string Client::ws_prepare_frame(uint8_t first_byte, uint8_t *base,
                                     internal::Size count) const noexcept {
  // TODO(bassosimone): perhaps move the RNG into Client?
  constexpr internal::Size mask_size = 4;
  uint8_t mask[mask_size] = {};
  // "When preparing a masked frame, the client MUST pick a fresh masking
  //  key from the set of allowed 32-bit values." [RFC6455 Sect. 5.3]. Hence
  // we're not compliant (TODO(bassosimone)).
  random_printable_fill((char *)mask, sizeof(mask));
  std::stringstream ss;
  // Message header
  {
    // First byte
    {
      // TODO(bassosimone): add sanity checks for first byte
      ss << first_byte;
      LIBNDT_EMIT_DEBUG("ws_prepare_frame: FIN: " << std::boolalpha
                                        << ((first_byte & ws_fin_flag) != 0));
      LIBNDT_EMIT_DEBUG(
          "ws_prepare_frame: reserved: " << (first_byte & ws_reserved_mask));
      LIBNDT_EMIT_DEBUG("ws_prepare_frame: opcode: " << (first_byte & ws_opcode_mask));
    }
    // Length
    {
      LIBNDT_EMIT_DEBUG("ws_prepare_frame: mask flag: " << std::boolalpha << true);
      LIBNDT_EMIT_DEBUG("ws_prepare_frame: length: " << count);
      // Since this is a client implementation, we always include the MASK flag
      // as part of the second byte that we send on the wire. Also, the spec
      // says that we must emit the length in network byte order, which means
      // in practice that we should use big endian.
      //
      // See <https://tools.ietf.org/html/rfc6455#section-5.1>, and
      //     <https://tools.ietf.org/html/rfc6455#section-5.2>.
#define LB(value)                                                        \
  do {                                                                   \
    LIBNDT_EMIT_DEBUG("ws_prepare_frame: length byte: " << (unsigned int)(value)); \
    ss << (value);                                                       \
  } while (0)
      if (count < 126) {
        LB((uint8_t)((count & ws_len_mask) | ws_mask_flag));
      } else if (count < (1 << 16)) {
        LB((uint8_t)((126 & ws_len_mask) | ws_mask_flag));
        LB((uint8_t)((count >> 8) & 0xff));
        LB((uint8_t)(count & 0xff));
      } else {
        LB((uint8_t)((127 & ws_len_mask) | ws_mask_flag));
        LB((uint8_t)((count >> 56) & 0xff));
        LB((uint8_t)((count >> 48) & 0xff));
        LB((uint8_t)((count >> 40) & 0xff));
        LB((uint8_t)((count >> 32) & 0xff));
        LB((uint8_t)((count >> 24) & 0xff));
        LB((uint8_t)((count >> 16) & 0xff));
        LB((uint8_t)((count >> 8) & 0xff));
        LB((uint8_t)(count & 0xff));
      }
#undef LB  // Tidy
    }
    // Mask
    {
      for (internal::Size i = 0; i < mask_size; ++i) {
        LIBNDT_EMIT_DEBUG("ws_prepare_frame: mask byte: " << (unsigned int)mask[i]
                                                << " ('" << mask[i] << "')");
        ss << (uint8_t)mask[i];
      }
    }
  }
  // As mentioned in the docs of this method, we will not include any
  // body in the frame if base is a null pointer.
  {
    for (internal::Size i = 0; i < count && base != nullptr; ++i) {
      // Implementation note: judging from a GCC 8 warning, it seems that using
      // `^=` causes -Wconversion warnings, while using `= ... ^` does not.
      base[i] = base[i] ^ mask[i % mask_size];
      ss << base[i];
    }
  }
  return ss.str();
}

internal::Err Client::ws_send_frame(internal::Socket sock, uint8_t first_byte, uint8_t *base,
                          internal::Size count) const noexcept {
  std::string prep = ws_prepare_frame(first_byte, base, count);
  return netx_sendn(sock, prep.c_str(), prep.size());
}

internal::Err Client::ws_recv_any_frame(internal::Socket sock, uint8_t *opcode, bool *fin,
      uint8_t *base, internal::Size total, internal::Size *count) const noexcept {
  // TODO(bassosimone): in this function we should consider an EOF as an
  // error, because with WebSocket we have explicit FIN mechanism.
  if (opcode == nullptr || fin == nullptr || count == nullptr) {
    LIBNDT_EMIT_WARNING("ws_recv_any_frame: passed invalid return arguments");
    return internal::Err::invalid_argument;
  }
  *opcode = 0;
  *fin = false;
  *count = 0;
  if (base == nullptr || total <= 0) {
    LIBNDT_EMIT_WARNING("ws_recv_any_frame: passed invalid buffer arguments");
    return internal::Err::invalid_argument;
  }
  // Message header
	internal::Size length = 0;
  // This assert is because the code below assumes that Size is basically
  // a uint64_t value. On 32 bit systems my understanding is that the compiler
  // supports 64 bit integers via emulation, hence I believe there is no
  // need to be worried about using a 64 bit integer here. My understanding
  // is supported, e.g., by <https://stackoverflow.com/a/2692369>.
  static_assert(sizeof(internal::Size) == sizeof(uint64_t), "Size is not 64 bit wide");
  {
    uint8_t buf[2];
    auto err = netx_recvn(sock, buf, sizeof(buf));
    if (err != internal::Err::none) {
      LIBNDT_EMIT_WARNING("ws_recv_any_frame: netx_recvn() failed for header");
      return err;
    }
    LIBNDT_EMIT_DEBUG("ws_recv_any_frame: ws header: "
               << represent(std::string{(char *)buf, sizeof(buf)}));
    *fin = (buf[0] & ws_fin_flag) != 0;
    LIBNDT_EMIT_DEBUG("ws_recv_any_frame: FIN: " << std::boolalpha << *fin);
    uint8_t reserved = (uint8_t)(buf[0] & ws_reserved_mask);
    if (reserved != 0) {
      // They only make sense for extensions, which we don't use. So we return
      // error. See <https://tools.ietf.org/html/rfc6455#section-5.2>.
      LIBNDT_EMIT_WARNING("ws_recv_any_frame: invalid reserved bits: " << reserved);
      return internal::Err::ws_proto;
    }
    *opcode = (uint8_t)(buf[0] & ws_opcode_mask);
    LIBNDT_EMIT_DEBUG("ws_recv_any_frame: opcode: " << (unsigned int)*opcode);
    switch (*opcode) {
      // clang-format off
      case ws_opcode_continue:
      case ws_opcode_text:
      case ws_opcode_binary:
      case ws_opcode_close:
      case ws_opcode_ping:
      case ws_opcode_pong: break;
      // clang-format off
      default:
        // See <https://tools.ietf.org/html/rfc6455#section-5.2>.
        LIBNDT_EMIT_WARNING("ws_recv_any_frame: invalid opcode");
        return internal::Err::ws_proto;
    }
    auto hasmask = (buf[1] & ws_mask_flag) != 0;
    // We do not expect to receive a masked frame. This is client code and
    // the RFC says that a server MUST NOT mask its frames.
    //
    // See <https://tools.ietf.org/html/rfc6455#section-5.1>.
    if (hasmask) {
      LIBNDT_EMIT_WARNING("ws_recv_any_frame: received masked frame");
      return internal::Err::invalid_argument;
    }
    length = (buf[1] & ws_len_mask);
    switch (*opcode) {
      case ws_opcode_close:
      case ws_opcode_ping:
      case ws_opcode_pong:
        if (length > 125 || *fin == false) {
          LIBNDT_EMIT_WARNING("ws_recv_any_frame: control messages MUST have a "
                       "payload length of 125 bytes or less and MUST NOT "
                       "be fragmented (see RFC6455 Sect 5.5.)");
          return internal::Err::ws_proto;
        }
        break;
    }
    // As mentioned above, length is transmitted using big endian encoding.
#define AL(value)                                                            \
  do {                                                                       \
    LIBNDT_EMIT_DEBUG("ws_recv_any_frame: length byte: " << (unsigned int)(value)); \
    length += (value);                                                       \
  } while (0)
    // The following should not happen because the lenght is over 7 bits but
    // it's nice to enforce assertions to make assumptions explicit.
    assert(length <= 127);
    if (length == 126) {
      uint8_t len_buf[2];
      auto recvn_err = netx_recvn(sock, len_buf, sizeof(len_buf));
      if (recvn_err != internal::Err::none) {
        LIBNDT_EMIT_WARNING(
            "ws_recv_any_frame: netx_recvn() failed for 16 bit length");
        return recvn_err;
      }
      LIBNDT_EMIT_DEBUG("ws_recv_any_frame: 16 bit length: "
                 << represent(std::string{(char *)len_buf, sizeof(len_buf)}));
      length = 0;  // Need to reset the length as AL() does +=
      AL(((internal::Size)len_buf[0]) << 8);
      AL((internal::Size)len_buf[1]);
    } else if (length == 127) {
      uint8_t len_buf[8];
      auto recvn_err = netx_recvn(sock, len_buf, sizeof(len_buf));
      if (recvn_err != internal::Err::none) {
        LIBNDT_EMIT_WARNING(
            "ws_recv_any_frame: netx_recvn() failed for 64 bit length");
        return recvn_err;
      }
      LIBNDT_EMIT_DEBUG("ws_recv_any_frame: 64 bit length: "
                 << represent(std::string{(char *)len_buf, sizeof(len_buf)}));
      length = 0;  // Need to reset the length as AL() does +=
      AL(((internal::Size)len_buf[0]) << 56);
      if ((len_buf[0] & 0x80) != 0) {
        // See <https://tools.ietf.org/html/rfc6455#section-5.2>: "[...] the
        // most significant bit MUST be 0."
        LIBNDT_EMIT_WARNING("ws_recv_any_frame: 64 bit length: invalid first bit");
        return internal::Err::ws_proto;
      }
      AL(((internal::Size)len_buf[1]) << 48);
      AL(((internal::Size)len_buf[2]) << 40);
      AL(((internal::Size)len_buf[3]) << 32);
      AL(((internal::Size)len_buf[4]) << 24);
      AL(((internal::Size)len_buf[5]) << 16);
      AL(((internal::Size)len_buf[6]) << 8);
      AL(((internal::Size)len_buf[7]));
    }
#undef AL  // Tidy
    if (length > total) {
      LIBNDT_EMIT_WARNING("ws_recv_any_frame: buffer too small");
      return internal::Err::message_size;
    }
    LIBNDT_EMIT_DEBUG("ws_recv_any_frame: length: " << length);
  }
  LIBNDT_EMIT_DEBUG("ws_recv_any_frame: received header");
  // Message body
  if (length > 0) {
    assert(length <= total);
    auto err = netx_recvn(sock, base, length);
    if (err != internal::Err::none) {
      LIBNDT_EMIT_WARNING("ws_recv_any_frame: netx_recvn() failed for body");
      return err;
    }
    // This makes the code too noisy when using -verbose. It may still be
    // useful to remove the comment when debugging.
    /*
    LIBNDT_EMIT_DEBUG("ws_recv_any_frame: received body: "
               << represent(std::string{(char *)base, length}));
    */
    *count = length;
  } else {
    LIBNDT_EMIT_DEBUG("ws_recv_any_frame: no body in this message");
    assert(*count == 0);
  }
  return internal::Err::none;
}

internal::Err Client::ws_recv_frame(internal::Socket sock, uint8_t *opcode, bool *fin,
      uint8_t *base, internal::Size total, internal::Size *count) const noexcept {
  // "Control frames (see Section 5.5) MAY be injected in the middle of
  // a fragmented message.  Control frames themselves MUST NOT be fragmented."
  //    -- RFC6455 Section 5.4.
  if (opcode == nullptr || fin == nullptr || count == nullptr) {
    LIBNDT_EMIT_WARNING("ws_recv_frame: passed invalid return arguments");
    return internal::Err::invalid_argument;
  }
  if (base == nullptr || total <= 0) {
    LIBNDT_EMIT_WARNING("ws_recv_frame: passed invalid buffer arguments");
    return internal::Err::invalid_argument;
  }
  auto err = internal::Err::none;
again:
  *opcode = 0;
  *fin = false;
  *count = 0;
  err = ws_recv_any_frame(sock, opcode, fin, base, total, count);
  if (err != internal::Err::none) {
    LIBNDT_EMIT_WARNING("ws_recv_frame: ws_recv_any_frame() failed");
    return err;
  }
  // "The application MUST NOT send any more data frames after sending a
  // Close frame." (RFC6455 Sect. 5.5.1). We're good as long as, for example,
  // we don't ever send a CLOSE but we just reply to CLOSE and then return
  // with an error, which will cause the connection to be closed. Note that
  // we MUST reply with CLOSE here (again Sect. 5.5.1).
  if (*opcode == ws_opcode_close) {
    LIBNDT_EMIT_DEBUG("ws_recv_frame: received CLOSE frame; sending CLOSE back");
    // Setting the FIN flag because control messages MUST NOT be fragmented
    // as specified in Section 5.5 of RFC6455.
    (void)ws_send_frame(sock, ws_opcode_close | ws_fin_flag, nullptr, 0);
    // TODO(bassosimone): distinguish between a shutdown at the socket layer
    // and a proper shutdown implemented at the WebSocket layer.
    return internal::Err::eof;
  }
  if (*opcode == ws_opcode_pong) {
    // RFC6455 Sect. 5.5.3 says that we must ignore a PONG.
    LIBNDT_EMIT_DEBUG("ws_recv_frame: received PONG frame; continuing to read");
    goto again;
  }
  if (*opcode == ws_opcode_ping) {
    // TODO(bassosimone): in theory a malicious server could DoS us by sending
    // a constant stream of PING frames for a long time.
    LIBNDT_EMIT_DEBUG("ws_recv_frame: received PING frame; PONGing back");
    assert(*count <= total);
    err = ws_send_frame(sock, ws_opcode_pong | ws_fin_flag, base, *count);
    if (err != internal::Err::none) {
      LIBNDT_EMIT_WARNING("ws_recv_frame: ws_send_frame() failed for PONG frame");
      return err;
    }
    LIBNDT_EMIT_DEBUG("ws_recv_frame: continuing to read after PONG");
    goto again;
  }
  return internal::Err::none;
}

internal::Err Client::ws_recvmsg(  //
    internal::Socket sock, uint8_t *opcode, uint8_t *base, internal::Size total,
    internal::Size *count) const noexcept {
  // General remark from RFC6455 Sect. 5.4: "[I]n absence of extensions, senders
  // and receivers must not depend on [...] specific frame boundaries."
  //
  // Also: "In the absence of any extension, a receiver doesn't have to buffer
  // the whole frame in order to process it." (Sect 5.4). However, currently
  // this implementation does that because we know NDT messages are "smallish"
  // not only for the control protocol but also for c2s and s2c, where in
  // general we attempt to use messages smaller than 256K.
  if (opcode == nullptr || count == nullptr) {
    LIBNDT_EMIT_WARNING("ws_recv: passed invalid return arguments");
    return internal::Err::invalid_argument;
  }
  if (base == nullptr || total <= 0) {
    LIBNDT_EMIT_WARNING("ws_recv: passed invalid buffer arguments");
    return internal::Err::invalid_argument;
  }
  bool fin = false;
  *opcode = 0;
  *count = 0;
  auto err = ws_recv_frame(sock, opcode, &fin, base, total, count);
  if (err != internal::Err::none) {
    // We don't want to scary the user in case of clean EOF
    if (err != internal::Err::eof) {
      LIBNDT_EMIT_WARNING("ws_recv: ws_recv_frame() failed for first frame");
    }
    return err;
  }
  if (*opcode != ws_opcode_binary && *opcode != ws_opcode_text) {
    LIBNDT_EMIT_WARNING("ws_recv: received unexpected opcode: " << *opcode);
    return internal::Err::ws_proto;
  }
  if (fin) {
    LIBNDT_EMIT_DEBUG("ws_recv: the first frame is also the last frame");
    return internal::Err::none;
  }
  while (*count < total) {
    if ((uintptr_t)base > UINTPTR_MAX - *count) {
      LIBNDT_EMIT_WARNING("ws_recv: avoiding pointer overflow");
      return internal::Err::value_too_large;
    }
    uint8_t op = 0;
		internal::Size n = 0;
    err = ws_recv_frame(sock, &op, &fin, base + *count, total - *count, &n);
    if (err != internal::Err::none) {
      LIBNDT_EMIT_WARNING("ws_recv: ws_recv_frame() failed for continuation frame");
      return err;
    }
    if (*count > internal::SizeMax - n) {
      LIBNDT_EMIT_WARNING("ws_recv: avoiding integer overflow");
      return internal::Err::value_too_large;
    }
    *count += n;
    if (op != ws_opcode_continue) {
      LIBNDT_EMIT_WARNING("ws_recv: received unexpected opcode: " << op);
      return internal::Err::ws_proto;
    }
    if (fin) {
      LIBNDT_EMIT_DEBUG("ws_recv: this is the last frame");
      return internal::Err::none;
    }
    LIBNDT_EMIT_DEBUG("ws_recv: this is not the last frame");
  }
  LIBNDT_EMIT_WARNING("ws_recv: buffer smaller than incoming message");
  return internal::Err::message_size;
}

// } - - - END WEBSOCKET IMPLEMENTATION - - -

// Networking layer
// ````````````````

// Required by OpenSSL code below. Must be outside because we want the code
// to compile also where we don't have OpenSSL support enabled.
#ifdef _WIN32
#define OS_SET_LAST_ERROR(ec) ::SetLastError(ec)
#else
#define OS_SET_LAST_ERROR(ec) errno = ec
#endif

// - - - BEGIN BIO IMPLEMENTATION - - - {
//
// This BIO implementation is based on the implementation of rabbitmq-c
// by @alanxz: <https://github.com/alanxz/rabbitmq-c/pull/402>.
//
// The code is available under the MIT license.
//
// The purpose of this BIO implementation is to pass the MSG_NOSIGNAL
// flag to socket I/O functions on Linux systems. While there, it seems
// convenient to route these I/O calls to the mockable methods of the
// client class, allowing for (1) more regress testing and (2) the
// possibility to very easily observe bytes on the wire. (I know that
// OpenSSL also allows that using callbacks but since we're making a
// BIO that possibility comes out very easily anyway.)
//
// We assume that a OpenSSL 1.1.0-like API is available.
/*-
 * Portions created by Alan Antonuk are Copyright (c) 2017 Alan Antonuk.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

// Helper used to route read and write calls to Client's I/O methods. We
// disregard the const qualifier of the `base` argument for the write operation,
// but that is not a big deal since we add it again before calling the real
// Socket op (see libndt_bio_write() below).
static int libndt_bio_operation(
    BIO *bio, char *base, int count,
    std::function<internal::Ssize(Client *, internal::Socket, char *, internal::Size)> operation,
    std::function<void(BIO *)> set_retry) noexcept {
  // Implementation note: before we have a valid Client pointer we cannot
  // of course use mocked functions. Hence OS_SET_LAST_ERROR().
  if (bio == nullptr || base == nullptr || count <= 0) {
    OS_SET_LAST_ERROR(LIBNDT_OS_EINVAL);
    return -1;
  }
  auto clnt = static_cast<Client *>(::BIO_get_data(bio));
  if (clnt == nullptr) {
    OS_SET_LAST_ERROR(LIBNDT_OS_EINVAL);
    return -1;
  }
  // Using a `int` to store a `SOCKET` is safe for internal non documented
  // reasons: even on Windows64 kernel handles use only 24 bits. See also
  // this Stack Overflow post: <https://stackoverflow.com/a/1953738>.
  int sock{};
  ::BIO_get_fd(bio, &sock);
  ::BIO_clear_retry_flags(bio);
  // Cast to Socket safe as int is okay to represent a Socket as we explained
  // above. Cast to Size safe because we've checked for negative above.
	internal::Ssize rv = operation(clnt, (internal::Socket)sock, base, (internal::Size)count);
  if (rv < 0) {
    assert(rv == -1);
    auto err = clnt->netx_map_errno(clnt->sys->GetLastError());
    if (err == internal::Err::operation_would_block) {
      set_retry(bio);
    }
    return -1;
  }
  // Cast to int safe because count was initially int. We anyway deploy an
  // assertion just in case (TM) but that should not happen (TM).
  assert(rv <= INT_MAX);
  return (int)rv;
}

// Write data using the underlying socket.
static int libndt_bio_write(BIO *bio, const char *base, int count) noexcept {
  // clang-format off
  return libndt_bio_operation(
      bio, (char *)base, count,
      [](Client *clnt, internal::Socket sock, char *base, internal::Size count) noexcept {
        return clnt->sys->Send(sock, (const char *)base, count);
      },
      [](BIO *bio) noexcept { ::BIO_set_retry_write(bio); });
  // clang-format on
}

// Read data using the underlying socket.
static int libndt_bio_read(BIO *bio, char *base, int count) noexcept {
  // clang-format off
  return libndt_bio_operation(
      bio, base, count,
      [](Client *clnt, internal::Socket sock, char *base, internal::Size count) noexcept {
        return clnt->sys->Recv(sock, base, count);
      },
      [](BIO *bio) noexcept { ::BIO_set_retry_read(bio); });
  // clang-format on
}

class BioMethodDeleter {
 public:
  void operator()(BIO_METHOD *meth) noexcept {
    if (meth != nullptr) {
      ::BIO_meth_free(meth);
    }
  }
};
using UniqueBioMethod = std::unique_ptr<BIO_METHOD, BioMethodDeleter>;

static BIO_METHOD *libndt_bio_method() noexcept {
  static std::atomic_bool initialized{false};
  static UniqueBioMethod method;
  static std::mutex mutex;
  if (!initialized) {
    std::unique_lock<std::mutex> _{mutex};
    if (!initialized) {
      BIO_METHOD *mm = ::BIO_meth_new(BIO_TYPE_SOCKET, "libndt_bio_method");
      if (mm == nullptr) {
        return nullptr;
      }
      // BIO_s_socket() returns a const BIO_METHOD in OpenSSL v1.1.0. We cast
      // that back to non const for the purpose of getting its methods.
      BIO_METHOD *m = (BIO_METHOD *)BIO_s_socket();
      BIO_meth_set_create(mm, BIO_meth_get_create(m));
      BIO_meth_set_destroy(mm, BIO_meth_get_destroy(m));
      BIO_meth_set_ctrl(mm, BIO_meth_get_ctrl(m));
      BIO_meth_set_callback_ctrl(mm, BIO_meth_get_callback_ctrl(m));
      BIO_meth_set_read(mm, libndt_bio_read);
      BIO_meth_set_write(mm, libndt_bio_write);
      BIO_meth_set_gets(mm, BIO_meth_get_gets(m));
      BIO_meth_set_puts(mm, BIO_meth_get_puts(m));
      method.reset(mm);
      initialized = true;
    }
  }
  return method.get();
}

// } - - - END BIO IMPLEMENTATION - - -

// Common function to map OpenSSL errors to Err.
static internal::Err map_ssl_error(const Client *client, SSL *ssl, int ret) noexcept {
  auto reason = ::SSL_get_error(ssl, ret);
  switch (reason) {
    case SSL_ERROR_NONE:
      return internal::Err::none;
    case SSL_ERROR_ZERO_RETURN:
      // TODO(bassosimone): consider the issue of dirty shutdown.
      return internal::Err::eof;
    case SSL_ERROR_WANT_READ:
      return internal::Err::ssl_want_read;
    case SSL_ERROR_WANT_WRITE:
      return internal::Err::ssl_want_write;
    case SSL_ERROR_SYSCALL:
      auto ecode = client->sys->GetLastError();
      if (ecode) {
        return client->netx_map_errno(ecode);
      }
      return internal::Err::ssl_syscall;
  }
  // TODO(bassosimone): in this case it may be nice to print the error queue
  // so to give the user a better understanding of what has happened.
  return internal::Err::ssl_generic;
}

// Retry simple, nonblocking OpenSSL operations such as handshake or shutdown.
static internal::Err ssl_retry_unary_op(std::string opname, Client *client, SSL *ssl,
                              internal::Socket fd, Timeout timeout,
                              std::function<int(SSL *)> unary_op) noexcept {
  auto err = internal::Err::none;
again:
  err = map_ssl_error(client, ssl, unary_op(ssl));
  // Retry if needed
  if (err == internal::Err::ssl_want_read) {
    err = client->netx_wait_readable(fd, timeout);
    if (err == internal::Err::none) {
      // TODO(bassosimone): make sure we don't loop in this function forever.
      goto again;
    }
  } else if (err == internal::Err::ssl_want_write) {
    err = client->netx_wait_writeable(fd, timeout);
    if (err == internal::Err::none) {
      goto again;
    }
  }
  // Otherwise let the caller know
  if (err != internal::Err::none) {
    LIBNDT_EMIT_WARNING_EX(client, opname << " failed: " << internal::libndt_perror(err));
  }
  return err;
}

internal::Err Client::netx_maybews_dial(const std::string &hostname,
                              const std::string &port, uint64_t ws_flags,
                              std::string ws_protocol, std::string url_path,
                              internal::Socket *sock) noexcept {
  auto err = netx_maybessl_dial(hostname, port, sock);
  if (err != internal::Err::none) {
    return err;
  }
  LIBNDT_EMIT_DEBUG("netx_maybews_dial: netx_maybessl_dial() returned successfully");
  if ((settings_.protocol_flags & protocol_flag_websocket) == 0) {
    LIBNDT_EMIT_DEBUG("netx_maybews_dial: websocket not enabled");
    return internal::Err::none;
  }
  LIBNDT_EMIT_DEBUG("netx_maybews_dial: about to start websocket handhsake");
  err = ws_handshake(*sock, port, ws_flags, ws_protocol, url_path);
  if (err != internal::Err::none) {
    (void)netx_closesocket(*sock);
    *sock = (internal::Socket)-1;
    return err;
  }
  LIBNDT_EMIT_DEBUG("netx_maybews_dial: established websocket channel");
  return internal::Err::none;
}

internal::Err Client::netx_maybessl_dial(const std::string &hostname,
                               const std::string &port, internal::Socket *sock) noexcept {
  // Temporarily clear the TLS flag because I/O functions inside of socks5h
  // code would otherwise fail given we've not established TLS yet. Then restore
  // the original flags right after the socks5h code returns.
  auto flags = settings_.protocol_flags;
  settings_.protocol_flags &= ~protocol_flag_tls;
  auto err = netx_maybesocks5h_dial(hostname, port, sock);
  settings_.protocol_flags = flags;
  if (err != internal::Err::none) {
    return err;
  }
  LIBNDT_EMIT_DEBUG(
      "netx_maybessl_dial: netx_maybesocks5h_dial() returned successfully");
  if ((settings_.protocol_flags & protocol_flag_tls) == 0) {
    LIBNDT_EMIT_DEBUG("netx_maybessl_dial: TLS not enabled");
    return internal::Err::none;
  }
  LIBNDT_EMIT_DEBUG("netx_maybetls_dial: about to start TLS handshake");
  if (settings_.ca_bundle_path.empty() && settings_.tls_verify_peer) {
#ifndef _WIN32
    // See <https://serverfault.com/a/722646>
    std::vector<std::string> candidates{
        "/etc/ssl/cert.pem",                   // macOS
        "/etc/ssl/certs/ca-certificates.crt",  // Debian
    };
    for (auto &candidate : candidates) {
      if (access(candidate.c_str(), R_OK) == 0) {
        LIBNDT_EMIT_DEBUG("Using '" << candidate.c_str() << "' as CA");
        settings_.ca_bundle_path = candidate;
        break;
      }
    }
    if (settings_.ca_bundle_path.empty()) {
#endif
      LIBNDT_EMIT_WARNING(
          "You did not provide me with a CA bundle path. Without this "
          "information I cannot validate the other TLS endpoint. So, "
          "I will not continue to run this test.");
      return internal::Err::invalid_argument;
#ifndef _WIN32
    }
#endif
  }
  SSL *ssl = nullptr;
  {
    // TODO(bassosimone): understand whether we can remove old SSL versions
    // taking into account that the NDT server runs on very old code.
    SSL_CTX *ctx = ::SSL_CTX_new(SSLv23_client_method());
    if (ctx == nullptr) {
      LIBNDT_EMIT_WARNING("SSL_CTX_new() failed");
      netx_closesocket(*sock);
      return internal::Err::ssl_generic;
    }
    LIBNDT_EMIT_DEBUG("SSL_CTX created");
    if (settings_.tls_verify_peer) {
      if (!::SSL_CTX_load_verify_locations(  //
              ctx, settings_.ca_bundle_path.c_str(), nullptr)) {
        LIBNDT_EMIT_WARNING("Cannot load the CA bundle path from the file system");
        ::SSL_CTX_free(ctx);
        netx_closesocket(*sock);
        return internal::Err::ssl_generic;
      }
      LIBNDT_EMIT_DEBUG("Loaded the CA bundle path");
    }
    ssl = ::SSL_new(ctx);
    if (ssl == nullptr) {
      LIBNDT_EMIT_WARNING("SSL_new() failed");
      ::SSL_CTX_free(ctx);
      netx_closesocket(*sock);
      return internal::Err::ssl_generic;
    }
    LIBNDT_EMIT_DEBUG("SSL created");
    ::SSL_CTX_free(ctx);  // Referenced by `ssl` so safe to free here
    assert(fd_to_ssl_.count(*sock) == 0);
    // Implementation note: after this point `netx_closesocket(*sock)` will
    // imply that `::SSL_free(ssl)` is also called.
    fd_to_ssl_[*sock] = ssl;
  }
  BIO *bio = ::BIO_new(libndt_bio_method());
  if (bio == nullptr) {
    LIBNDT_EMIT_WARNING("BIO_new() failed");
    netx_closesocket(*sock);
    //::SSL_free(ssl); // MUST NOT be called because of fd_to_ssl
    return internal::Err::ssl_generic;
  }
  LIBNDT_EMIT_DEBUG("libndt BIO created");
  // We use BIO_NOCLOSE because it's the socket that owns the BIO and the SSL
  // via fd_to_ssl rather than the other way around. Note that sockets are
  // always `int` in OpenSSL notwithstanding their definition on Windows, so
  // here we're casting unconditionally to silence compiler warnings.
  //
  // See <https://www.openssl.org/docs/man1.1.1/man3/BIO_s_socket.html> and
  //     <https://stackoverflow.com/questions/1953639> for why this is scary
  //     but fundamentally the right thing to do in this context.
  ::BIO_set_fd(bio, (int)*sock, BIO_NOCLOSE);
  // For historical reasons, if the two BIOs are equal, the SSL object will
  // increase the refcount of bio just once rather than twice.
  ::SSL_set_bio(ssl, bio, bio);
  ::BIO_set_data(bio, this);
  ::SSL_set_connect_state(ssl);
  LIBNDT_EMIT_DEBUG("Socket added to SSL context");
  if (settings_.tls_verify_peer) {
    // This approach for validating the hostname should work with versions
    // of OpenSSL greater than v1.0.2 and with LibreSSL. Code taken from the
    // wiki: <https://wiki.openssl.org/index.php/Hostname_validation>.
    X509_VERIFY_PARAM *p = SSL_get0_param(ssl);
    assert(p != nullptr);
    X509_VERIFY_PARAM_set_hostflags(p, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
    if (!::X509_VERIFY_PARAM_set1_host(p, hostname.data(), hostname.size())) {
      LIBNDT_EMIT_WARNING("Cannot set the hostname for hostname validation");
      netx_closesocket(*sock);
      //::SSL_free(ssl); // MUST NOT be called because of fd_to_ssl
      return internal::Err::ssl_generic;
    }
    SSL_set_verify(ssl, SSL_VERIFY_PEER, nullptr);
    LIBNDT_EMIT_DEBUG("SSL_VERIFY_PEER configured");
  }
  err = ssl_retry_unary_op("SSL_do_handshake", this, ssl, *sock,
                           settings_.timeout, [](SSL *ssl) -> int {
                             ERR_clear_error();
                             return ::SSL_do_handshake(ssl);
                           });
  if (err != internal::Err::none) {
    netx_closesocket(*sock);
    //::SSL_free(ssl); // MUST NOT be called because of fd_to_ssl
    return internal::Err::ssl_generic;
  }
  LIBNDT_EMIT_DEBUG("SSL handshake complete");
  return internal::Err::none;
}

internal::Err Client::netx_maybesocks5h_dial(const std::string &hostname,
                                   const std::string &port,
                                   internal::Socket *sock) noexcept {
  if (settings_.socks5h_port.empty()) {
    LIBNDT_EMIT_DEBUG("socks5h: not configured, connecting directly");
    return netx_dial(hostname, port, sock);
  }
  {
    auto err = netx_dial("127.0.0.1", settings_.socks5h_port, sock);
    if (err != internal::Err::none) {
      return err;
    }
  }
  LIBNDT_EMIT_INFO("socks5h: connected to proxy");
  {
    char auth_request[] = {
        5,  // version
        1,  // number of methods
        0   // "no auth" method
    };
    auto err = netx_sendn(*sock, auth_request, sizeof(auth_request));
    if (err != internal::Err::none) {
      LIBNDT_EMIT_WARNING("socks5h: cannot send auth_request");
      netx_closesocket(*sock);
      *sock = (libndt::internal::Socket)-1;
      return err;
    }
    LIBNDT_EMIT_DEBUG("socks5h: sent this auth request: "
               << represent(std::string{auth_request, sizeof(auth_request)}));
  }
  {
    char auth_response[2] = {
        0,  // version
        0   // method
    };
    auto err = netx_recvn(*sock, auth_response, sizeof(auth_response));
    if (err != internal::Err::none) {
      LIBNDT_EMIT_WARNING("socks5h: cannot recv auth_response");
      netx_closesocket(*sock);
      *sock = (libndt::internal::Socket)-1;
      return err;
    }
    constexpr uint8_t version = 5;
    if (auth_response[0] != version) {
      LIBNDT_EMIT_WARNING("socks5h: received unexpected version number");
      netx_closesocket(*sock);
      *sock = (libndt::internal::Socket)-1;
      return internal::Err::socks5h;
    }
    constexpr uint8_t auth_method = 0;
    if (auth_response[1] != auth_method) {
      LIBNDT_EMIT_WARNING("socks5h: received unexpected auth_method");
      netx_closesocket(*sock);
      *sock = (libndt::internal::Socket)-1;
      return internal::Err::socks5h;
    }
    LIBNDT_EMIT_DEBUG("socks5h: authenticated with proxy; response: "
               << represent(std::string{auth_response, sizeof(auth_response)}));
  }
  {
    std::string connect_request;
    {
      std::stringstream ss;
      ss << (uint8_t)5;  // version
      ss << (uint8_t)1;  // CMD_CONNECT
      ss << (uint8_t)0;  // reserved
      ss << (uint8_t)3;  // ATYPE_DOMAINNAME
      if (hostname.size() > UINT8_MAX) {
        LIBNDT_EMIT_WARNING("socks5h: hostname is too long");
        netx_closesocket(*sock);
        *sock = (libndt::internal::Socket)-1;
        return internal::Err::invalid_argument;
      }
      ss << (uint8_t)hostname.size();
      ss << hostname;
      uint16_t portno{};
      {
        const char *errstr = nullptr;
        portno = (uint16_t)sys->Strtonum(port.c_str(), 0, UINT16_MAX, &errstr);
        if (errstr != nullptr) {
          LIBNDT_EMIT_WARNING("socks5h: invalid port number: " << errstr);
          netx_closesocket(*sock);
          *sock = (libndt::internal::Socket)-1;
          return internal::Err::invalid_argument;
        }
      }
      portno = htons(portno);
      ss << (uint8_t)((char *)&portno)[0] << (uint8_t)((char *)&portno)[1];
      connect_request = ss.str();
      LIBNDT_EMIT_DEBUG("socks5h: connect_request: " << represent(connect_request));
    }
    auto err = netx_sendn(  //
        *sock, connect_request.data(), connect_request.size());
    if (err != internal::Err::none) {
      LIBNDT_EMIT_WARNING("socks5h: cannot send connect_request");
      netx_closesocket(*sock);
      *sock = (libndt::internal::Socket)-1;
      return err;
    }
    LIBNDT_EMIT_DEBUG("socks5h: sent connect request");
  }
  {
    char connect_response_hdr[] = {
        0,  // version
        0,  // error
        0,  // reserved
        0   // type
    };
    auto err = netx_recvn(  //
        *sock, connect_response_hdr, sizeof(connect_response_hdr));
    if (err != internal::Err::none) {
      LIBNDT_EMIT_WARNING("socks5h: cannot recv connect_response_hdr");
      netx_closesocket(*sock);
      *sock = (libndt::internal::Socket)-1;
      return err;
    }
    LIBNDT_EMIT_DEBUG("socks5h: connect_response_hdr: " << represent(std::string{
                   connect_response_hdr, sizeof(connect_response_hdr)}));
    constexpr uint8_t version = 5;
    if (connect_response_hdr[0] != version) {
      LIBNDT_EMIT_WARNING("socks5h: invalid message version");
      netx_closesocket(*sock);
      *sock = (libndt::internal::Socket)-1;
      return internal::Err::socks5h;
    }
    if (connect_response_hdr[1] != 0) {
      // TODO(bassosimone): map the socks5 error to a system error
      LIBNDT_EMIT_WARNING("socks5h: connect() failed: "
                   << (unsigned)(uint8_t)connect_response_hdr[1]);
      netx_closesocket(*sock);
      *sock = (libndt::internal::Socket)-1;
      return internal::Err::io_error;
    }
    if (connect_response_hdr[2] != 0) {
      LIBNDT_EMIT_WARNING("socks5h: invalid reserved field");
      netx_closesocket(*sock);
      *sock = (libndt::internal::Socket)-1;
      return internal::Err::socks5h;
    }
    // receive IP or domain
    switch (connect_response_hdr[3]) {
      case 1:  // ipv4
      {
        constexpr internal::Size expected = 4;  // ipv4
        char buf[expected];
        auto recvn_err = netx_recvn(*sock, buf, sizeof(buf));
        if (recvn_err != internal::Err::none) {
          LIBNDT_EMIT_WARNING("socks5h: cannot recv ipv4 address");
          netx_closesocket(*sock);
          *sock = (libndt::internal::Socket)-1;
          return recvn_err;
        }
        // TODO(bassosimone): log the ipv4 address. However tor returns a zero
        // ipv4 and so there is little added value in logging.
        break;
      }
      case 3:  // domain
      {
        uint8_t len = 0;
        auto recvn_err = netx_recvn(*sock, &len, sizeof(len));
        if (recvn_err != internal::Err::none) {
          LIBNDT_EMIT_WARNING("socks5h: cannot recv domain length");
          netx_closesocket(*sock);
          *sock = (libndt::internal::Socket)-1;
          return recvn_err;
        }
        char domain[UINT8_MAX + 1];  // space for final '\0'
        recvn_err = netx_recvn(*sock, domain, len);
        if (recvn_err != internal::Err::none) {
          LIBNDT_EMIT_WARNING("socks5h: cannot recv domain");
          netx_closesocket(*sock);
          *sock = (libndt::internal::Socket)-1;
          return recvn_err;
        }
        domain[len] = 0;
        LIBNDT_EMIT_DEBUG("socks5h: domain: " << domain);
        break;
      }
      case 4:  // ipv6
      {
        constexpr internal::Size expected = 16;  // ipv6
        char buf[expected];
        auto recvn_err = netx_recvn(*sock, buf, sizeof(buf));
        if (recvn_err != internal::Err::none) {
          LIBNDT_EMIT_WARNING("socks5h: cannot recv ipv6 address");
          netx_closesocket(*sock);
          *sock = (libndt::internal::Socket)-1;
          return recvn_err;
        }
        // TODO(bassosimone): log the ipv6 address. However tor returns a zero
        // ipv6 and so there is little added value in logging.
        break;
      }
      default:
        LIBNDT_EMIT_WARNING("socks5h: invalid address type");
        netx_closesocket(*sock);
        *sock = (libndt::internal::Socket)-1;
        return internal::Err::socks5h;
    }
    // receive the port
    {
      uint16_t real_port = 0;
      auto recvn_err = netx_recvn(*sock, &real_port, sizeof(real_port));
      if (recvn_err != internal::Err::none) {
        LIBNDT_EMIT_WARNING("socks5h: cannot recv port");
        netx_closesocket(*sock);
        *sock = (libndt::internal::Socket)-1;
        return recvn_err;
      }
      real_port = ntohs(real_port);
      LIBNDT_EMIT_DEBUG("socks5h: port number: " << real_port);
    }
  }
  LIBNDT_EMIT_INFO("socks5h: the proxy has successfully connected");
  return internal::Err::none;
}

#ifdef _WIN32
#define E(name) WSAE##name
#else
#define E(name) E##name
#endif

/*static*/ internal::Err Client::netx_map_errno(int ec) noexcept {
  // clang-format off
  switch (ec) {
    case 0: {
      assert(false);  // we don't expect `errno` to be zero
      return internal::Err::io_error;
    }
#ifndef _WIN32
		case E(PIPE): return internal::Err::broken_pipe;
#endif
		case E(CONNABORTED): return internal::Err::connection_aborted;
		case E(CONNREFUSED): return internal::Err::connection_refused;
		case E(CONNRESET): return internal::Err::connection_reset;
		case E(HOSTUNREACH): return internal::Err::host_unreachable;
		case E(INTR): return internal::Err::interrupted;
		case E(INVAL): return internal::Err::invalid_argument;
#ifndef _WIN32
		case E(IO): return internal::Err::io_error;
#endif
		case E(NETDOWN): return internal::Err::network_down;
		case E(NETRESET): return internal::Err::network_reset;
		case E(NETUNREACH): return internal::Err::network_unreachable;
		case E(INPROGRESS): return internal::Err::operation_in_progress;
		case E(WOULDBLOCK): return internal::Err::operation_would_block;
#if !defined _WIN32 && EAGAIN != EWOULDBLOCK
		case E(AGAIN): return internal::Err::operation_would_block;
#endif
		case E(TIMEDOUT): return internal::Err::timed_out;
  }
  // clang-format on
  return internal::Err::io_error;
}

#undef E  // Tidy up

internal::Err Client::netx_map_eai(int ec) noexcept {
  // clang-format off
  switch (ec) {
		case EAI_AGAIN: return internal::Err::ai_again;
		case EAI_FAIL: return internal::Err::ai_fail;
		case EAI_NONAME: return internal::Err::ai_noname;
#ifdef EAI_SYSTEM
    case EAI_SYSTEM: {
      return netx_map_errno(sys->GetLastError());
    }
#endif
  }
  // clang-format on
  return internal::Err::ai_generic;
}

#ifdef _WIN32
// Depending on the version of Winsock it's either EAGAIN or EINPROGRESS
#define CONNECT_IN_PROGRESS(e) \
  (e == internal::Err::operation_would_block || e == internal::Err::operation_in_progress)
#else
#define CONNECT_IN_PROGRESS(e) (e == internal::Err::operation_in_progress)
#endif

internal::Err Client::netx_dial(const std::string &hostname, const std::string &port,
                      internal::Socket *sock) noexcept {
  assert(sock != nullptr);
  if (*sock != -1) {
    LIBNDT_EMIT_WARNING("netx_dial: socket already connected");
    return internal::Err::invalid_argument;
  }
  // Implementation note: we could perform getaddrinfo() in one pass but having
  // a virtual API that resolves a hostname to a vector of IP addresses makes
  // life easier when you want to override hostname resolution, because you have
  // to reimplement a simpler method, compared to reimplementing getaddrinfo().
  std::vector<std::string> addresses;
	internal::Err err;
  if ((err = netx_resolve(hostname, &addresses)) != internal::Err::none) {
    return err;
  }
  for (auto &addr : addresses) {
    addrinfo hints{};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags |= AI_NUMERICHOST | AI_NUMERICSERV;
    addrinfo *rp = nullptr;
    int rv = sys->Getaddrinfo(addr.data(), port.data(), &hints, &rp);
    if (rv != 0) {
      LIBNDT_EMIT_WARNING("netx_dial: unexpected getaddrinfo() failure");
      return netx_map_eai(rv);
    }
    assert(rp);
    for (auto aip = rp; (aip); aip = aip->ai_next) {
      sys->SetLastError(0);
      *sock = sys->NewSocket(aip->ai_family, aip->ai_socktype, 0);
      if (!internal::IsSocketValid(*sock)) {
        LIBNDT_EMIT_WARNING("netx_dial: socket() failed");
        continue;
      }
#ifdef SO_NOSIGPIPE
      // Implementation note: SO_NOSIGPIPE is the nonportable BSD solution to
      // avoid SIGPIPE when writing on a connection closed by the peer.
      {
        auto on = 1;
        if (::setsockopt(  //
                *sock, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on)) != 0) {
          LIBNDT_EMIT_WARNING("netx_dial: setsockopt(..., SO_NOSIGPIPE) failed");
          sys->Closesocket(*sock);
          *sock = -1;
          continue;
        }
      }
#endif  // SO_NOSIGPIPE
      if (netx_setnonblocking(*sock, true) != internal::Err::none) {
        LIBNDT_EMIT_WARNING("netx_dial: netx_setnonblocking() failed");
        sys->Closesocket(*sock);
        *sock = (libndt::internal::Socket)-1;
        continue;
      }
      // While on Unix ai_addrlen is socklen_t, it's size_t on Windows. Just
      // for the sake of correctness, add a check that ensures that the size has
      // a reasonable value before casting to socklen_t. My understanding is
      // that size_t is `ULONG_PTR` while socklen_t is most likely `int`.
#ifdef _WIN32
      if (aip->ai_addrlen > sizeof(sockaddr_in6)) {
        LIBNDT_EMIT_WARNING("netx_dial: unexpected size of aip->ai_addrlen");
        sys->Closesocket(*sock);
        *sock = (libndt::internal::Socket)-1;
        continue;
      }
#endif
      if (sys->Connect(*sock, aip->ai_addr, (socklen_t)aip->ai_addrlen) == 0) {
        LIBNDT_EMIT_DEBUG("netx_dial: connect(): okay immediately");
        break;
      }
      auto connect_err = netx_map_errno(sys->GetLastError());
      if (CONNECT_IN_PROGRESS(connect_err)) {
        connect_err = netx_wait_writeable(*sock, settings_.timeout);
        if (connect_err == internal::Err::none) {
          int soerr = 0;
          socklen_t soerrlen = sizeof(soerr);
          if (sys->Getsockopt(*sock, SOL_SOCKET, SO_ERROR, (void *)&soerr,
                             &soerrlen) == 0) {
            assert(soerrlen == sizeof(soerr));
            if (soerr == 0) {
              LIBNDT_EMIT_DEBUG("netx_dial: connect(): okay");
              break;
            }
            sys->SetLastError(soerr);
          }
        }
      }
      LIBNDT_EMIT_WARNING("netx_dial: connect() failed: "
                   << internal::libndt_perror(netx_map_errno(sys->GetLastError())));
      sys->Closesocket(*sock);
      *sock = (libndt::internal::Socket)-1;
    }
    sys->Freeaddrinfo(rp);
    if (*sock != -1) {
      break;  // we have a connection!
    }
  }
  // TODO(bassosimone): it's possible to write a better algorithm here
  return *sock != -1 ? internal::Err::none : internal::Err::io_error;
}

#undef CONNECT_IN_PROGRESS  // Tidy

internal::Err Client::netx_recv(internal::Socket fd, void *base, internal::Size count,
                      internal::Size *actual) const noexcept {
  auto err = internal::Err::none;
again:
  err = netx_recv_nonblocking(fd, base, count, actual);
  if (err == internal::Err::none) {
    return internal::Err::none;
  }
  if (err == internal::Err::operation_would_block || err == internal::Err::ssl_want_read) {
    err = netx_wait_readable(fd, settings_.timeout);
  } else if (err == internal::Err::ssl_want_write) {
    err = netx_wait_writeable(fd, settings_.timeout);
  }
  if (err == internal::Err::none) {
    goto again;
  }
  LIBNDT_EMIT_DEBUG(
      "netx_recv: netx_recv_nonblocking() failed: " << internal::libndt_perror(err));
  return err;
}

internal::Err Client::netx_recv_nonblocking(internal::Socket fd, void *base, internal::Size count,
                                  internal::Size *actual) const noexcept {
  assert(base != nullptr && actual != nullptr);
  *actual = 0;
  if (count <= 0) {
    LIBNDT_EMIT_WARNING(
        "netx_recv_nonblocking: explicitly disallowing zero read; use "
        "netx_poll() to check the state of a socket");
    return internal::Err::invalid_argument;
  }
  sys->SetLastError(0);
  if ((settings_.protocol_flags & protocol_flag_tls) != 0) {
    if (count > INT_MAX) {
      return internal::Err::invalid_argument;
    }
    if (fd_to_ssl_.count(fd) != 1) {
      return internal::Err::invalid_argument;
    }
    auto ssl = fd_to_ssl_.at(fd);
    // TODO(bassosimone): add mocks and regress tests for OpenSSL.
    ERR_clear_error();
    int ret = ::SSL_read(ssl, base, (int)count);
    if (ret <= 0) {
      return map_ssl_error(this, ssl, ret);
    }
    *actual = (internal::Size)ret;
    return internal::Err::none;
  }
  auto rv = sys->Recv(fd, base, count);
  if (rv < 0) {
    assert(rv == -1);
    return netx_map_errno(sys->GetLastError());
  }
  if (rv == 0) {
    assert(count > 0);  // guaranteed by the above check
    return internal::Err::eof;
  }
  *actual = (internal::Size)rv;
  return internal::Err::none;
}

internal::Err Client::netx_recvn(internal::Socket fd, void *base, internal::Size count) const noexcept {
	internal::Size off = 0;
  while (off < count) {
		internal::Size n = 0;
    if ((uintptr_t)base > UINTPTR_MAX - off) {
      return internal::Err::value_too_large;
    }
		internal::Err err = netx_recv(fd, ((char *)base) + off, count - off, &n);
    if (err != internal::Err::none) {
      return err;
    }
    if (off > internal::SizeMax - n) {
      return internal::Err::value_too_large;
    }
    off += n;
  }
  return internal::Err::none;
}

internal::Err Client::netx_send(internal::Socket fd, const void *base, internal::Size count,
                      internal::Size *actual) const noexcept {
  auto err = internal::Err::none;
again:
  err = netx_send_nonblocking(fd, base, count, actual);
  if (err == internal::Err::none) {
    return internal::Err::none;
  }
  if (err == internal::Err::ssl_want_read) {
    err = netx_wait_readable(fd, settings_.timeout);
  } else if (err == internal::Err::operation_would_block || err == internal::Err::ssl_want_write) {
    err = netx_wait_writeable(fd, settings_.timeout);
  }
  if (err == internal::Err::none) {
    goto again;
  }
  LIBNDT_EMIT_DEBUG(
      "netx_send: netx_send_nonblocking() failed: " << internal::libndt_perror(err));
  return err;
}

internal::Err Client::netx_send_nonblocking(internal::Socket fd, const void *base, internal::Size count,
                                  internal::Size *actual) const noexcept {
  assert(base != nullptr && actual != nullptr);
  *actual = 0;
  if (count <= 0) {
    LIBNDT_EMIT_WARNING(
        "netx_send_nonblocking: explicitly disallowing zero send; use "
        "netx_poll() to check the state of a socket");
    return internal::Err::invalid_argument;
  }
  sys->SetLastError(0);
  if ((settings_.protocol_flags & protocol_flag_tls) != 0) {
    if (count > INT_MAX) {
      return internal::Err::invalid_argument;
    }
    if (fd_to_ssl_.count(fd) != 1) {
      return internal::Err::invalid_argument;
    }
    auto ssl = fd_to_ssl_.at(fd);
    ERR_clear_error();
    // TODO(bassosimone): add mocks and regress tests for OpenSSL.
    int ret = ::SSL_write(ssl, base, (int)count);
    if (ret <= 0) {
      return map_ssl_error(this, ssl, ret);
    }
    *actual = (internal::Size)ret;
    return internal::Err::none;
  }
  auto rv = sys->Send(fd, base, count);
  if (rv < 0) {
    assert(rv == -1);
    return netx_map_errno(sys->GetLastError());
  }
  // Send() should not return zero unless count is zero. So consider a zero
  // return value as an I/O error rather than EOF.
  if (rv == 0) {
    assert(count > 0);  // guaranteed by the above check
    return internal::Err::io_error;
  }
  *actual = (internal::Size)rv;
  return internal::Err::none;
}

internal::Err Client::netx_sendn(internal::Socket fd, const void *base, internal::Size count) const noexcept {
	internal::Size off = 0;
  while (off < count) {
		internal::Size n = 0;
    if ((uintptr_t)base > UINTPTR_MAX - off) {
      return internal::Err::value_too_large;
    }
		internal::Err err = netx_send(fd, ((char *)base) + off, count - off, &n);
    if (err != internal::Err::none) {
      return err;
    }
    if (off > internal::SizeMax - n) {
      return internal::Err::value_too_large;
    }
    off += n;
  }
  return internal::Err::none;
}

internal::Err Client::netx_resolve(const std::string &hostname,
                         std::vector<std::string> *addrs) noexcept {
  assert(addrs != nullptr);
  LIBNDT_EMIT_DEBUG("netx_resolve: " << hostname);
  addrinfo hints{};
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags |= AI_NUMERICHOST | AI_NUMERICSERV;
  addrinfo *rp = nullptr;
  constexpr const char *portno = "80";  // any port would do
  int rv = sys->Getaddrinfo(hostname.data(), portno, &hints, &rp);
  if (rv != 0) {
    hints.ai_flags &= ~AI_NUMERICHOST;
    rv = sys->Getaddrinfo(hostname.data(), portno, &hints, &rp);
    if (rv != 0) {
      auto err = netx_map_eai(rv);
      LIBNDT_EMIT_WARNING(
          "netx_resolve: getaddrinfo() failed: " << internal::libndt_perror(err));
      return err;
    }
    // FALLTHROUGH
  }
  assert(rp);
  LIBNDT_EMIT_DEBUG("netx_resolve: getaddrinfo(): okay");
	internal::Err result = internal::Err::none;
  for (auto aip = rp; (aip); aip = aip->ai_next) {
    char address[NI_MAXHOST], port[NI_MAXSERV];
    // The following casts from `size_t` to `socklen_t` are safe for sure
    // because NI_MAXHOST and NI_MAXSERV are small values. To make sure this
    // assumption is correct, deploy the following static assertion. Here I am
    // using INT_MAX as upper bound since socklen_t SHOULD be `int`.
    static_assert(sizeof(address) <= INT_MAX && sizeof(port) <= INT_MAX,
                  "Wrong assumption about NI_MAXHOST or NI_MAXSERV");
    // Additionally on Windows there's a cast from size_t to socklen_t that
    // needs to be handled as we do above for getaddrinfo().
#ifdef _WIN32
    if (aip->ai_addrlen > sizeof(sockaddr_in6)) {
      LIBNDT_EMIT_WARNING("netx_resolve: unexpected size of aip->ai_addrlen");
      result = internal::Err::value_too_large;
      break;
    }
#endif
    if (sys->Getnameinfo(aip->ai_addr, (socklen_t)aip->ai_addrlen, address,
                        (socklen_t)sizeof(address), port,
                        (socklen_t)sizeof(port),
                        NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
      LIBNDT_EMIT_WARNING("netx_resolve: unexpected getnameinfo() failure");
      result = internal::Err::ai_generic;
      break;
    }
    addrs->push_back(address);  // we only care about address
    LIBNDT_EMIT_DEBUG("netx_resolve: - " << address);
  }
  sys->Freeaddrinfo(rp);
  return result;
}

internal::Err Client::netx_setnonblocking(internal::Socket fd, bool enable) noexcept {
#ifdef _WIN32
  u_long lv = (enable) ? 1UL : 0UL;
  if (sys->Ioctlsocket(fd, FIONBIO, &lv) != 0) {
    return netx_map_errno(sys->GetLastError());
  }
#else
  auto flags = sys->Fcntl(fd, F_GETFL);
  if (flags < 0) {
    assert(flags == -1);
    return netx_map_errno(sys->GetLastError());
  }
  if (enable) {
    flags |= O_NONBLOCK;
  } else {
    flags &= ~O_NONBLOCK;
  }
  if (sys->Fcntl(fd, F_SETFL, flags) != 0) {
    return netx_map_errno(sys->GetLastError());
  }
#endif
  return internal::Err::none;
}

static internal::Err netx_wait(const Client *client, internal::Socket fd, Timeout timeout,
                     short expected_events) noexcept {
  pollfd pfd{};
  pfd.fd = fd;
  pfd.events |= expected_events;
  std::vector<pollfd> pfds;
  pfds.push_back(pfd);
  // The following makes sure that it's okay to cast Timeout (an unsigned int
  // type) to poll()'s timeout type (i.e. signed int).
  static_assert(sizeof(timeout) == sizeof(int), "Unexpected Timeout size");
  if (timeout > INT_MAX / 1000) {
    timeout = INT_MAX / 1000;
  }
  auto err = client->netx_poll(&pfds, (int)timeout * 1000);
  // Either it's success and something happened or we failed and nothing
  // must have happened on the socket. We previously checked whether we had
  // `expected_events` set however the flags actually set by poll are
  // dependent on the system and file descriptor type. Hence it is more
  // robust to only make sure that at least a flag is set.
  //
  // Also, note that we explicitly clear revents in next_poll() before
  // calling the system implementation of poll().
  //
  // See also Stack Overflow: <https://stackoverflow.com/a/25249958>.
  assert((err == internal::Err::none && pfds[0].revents != 0) ||
         (err != internal::Err::none && pfds[0].revents == 0));
  return err;
}

internal::Err Client::netx_wait_readable(internal::Socket fd, Timeout timeout) const noexcept {
  return netx_wait(this, fd, timeout, POLLIN);
}

internal::Err Client::netx_wait_writeable(internal::Socket fd, Timeout timeout) const noexcept {
  return netx_wait(this, fd, timeout, POLLOUT);
}

internal::Err Client::netx_poll(
      std::vector<pollfd> *pfds, int timeout_msec) const noexcept {
  if (pfds == nullptr) {
    LIBNDT_EMIT_WARNING("netx_poll: passed a null vector of descriptors");
    return internal::Err::invalid_argument;
  }
  for (auto &pfd : *pfds) {
    pfd.revents = 0;  // clear unconditionally
  }
  int rv = 0;
#ifndef _WIN32
again:
#endif
  // Different operating systems have different representations of size_t
  // and of nfds_t. Overcome these differences by choosing a smaller
  // representation of the fdset size and letting the compiler promote
  // it to the correct integer. We don't need many fds in any case.
  if (pfds->size() > UINT8_MAX) {
    LIBNDT_EMIT_WARNING("netx_poll: avoiding overflow");
    return internal::Err::value_too_large;
  }
  rv = sys->Poll(pfds->data(), (uint8_t)pfds->size(), timeout_msec);
  // TODO(bassosimone): handle the case where POLLNVAL is returned.
#ifdef _WIN32
  if (rv == SOCKET_ERROR) {
    return netx_map_errno(sys->GetLastError());
  }
#else
  if (rv < 0) {
    assert(rv == -1);
    auto err = netx_map_errno(sys->GetLastError());
    if (err == internal::Err::interrupted) {
      goto again;
    }
    return err;
  }
#endif
  return (rv == 0) ? internal::Err::timed_out : internal::Err::none;
}

internal::Err Client::netx_shutdown_both(internal::Socket fd) noexcept {
  if ((settings_.protocol_flags & protocol_flag_tls) != 0) {
    if (fd_to_ssl_.count(fd) != 1) {
      return internal::Err::invalid_argument;
    }
    auto ssl = fd_to_ssl_.at(fd);
    auto err = ssl_retry_unary_op(  //
        "SSL_shutdown", this, ssl, fd, settings_.timeout,
        [](SSL *ssl) -> int {
          ERR_clear_error();
          return ::SSL_shutdown(ssl);
        });
    if (err != internal::Err::none) {
      return err;
    }
  }
  if (sys->Shutdown(fd, LIBNDT_OS_SHUT_RDWR) != 0) {
    return netx_map_errno(sys->GetLastError());
  }
  return internal::Err::none;
}

internal::Err Client::netx_closesocket(internal::Socket fd) noexcept {
  if ((settings_.protocol_flags & protocol_flag_tls) != 0) {
    if (fd_to_ssl_.count(fd) != 1) {
      return internal::Err::invalid_argument;
    }
    ::SSL_free(fd_to_ssl_.at(fd));
    fd_to_ssl_.erase(fd);
  }
  if (sys->Closesocket(fd) != 0) {
    return netx_map_errno(sys->GetLastError());
  }
  return internal::Err::none;
}

// Curl helpers
// ````````````

class CurlxLoggerAdapter : public internal::Logger {
 public:
  explicit CurlxLoggerAdapter(Client *client) noexcept : client_{client} {}

  bool is_warning_enabled() const noexcept override {
		return client_->get_verbosity() >= verbosity_warning;
  }

  bool is_info_enabled() const noexcept override {
		return client_->get_verbosity() >= verbosity_info;
  }

  bool is_debug_enabled() const noexcept override {
		return client_->get_verbosity() >= verbosity_debug;
  }

  void emit_warning(const std::string &s) const noexcept override {
		client_->on_warning(s);
  }

  void emit_info(const std::string &s) const noexcept override {
		client_->on_info(s);
  }

  void emit_debug(const std::string &s) const noexcept override {
		client_->on_debug(s);
  }

  ~CurlxLoggerAdapter() noexcept override {}

 private:
  Client *client_;
};

bool Client::query_mlabns_curl(const std::string &url, long timeout,
                               std::string *body) noexcept {
  CurlxLoggerAdapter adapter{this};
  internal::Curlx curlx{adapter};
  return curlx.GetMaybeSOCKS5(settings_.socks5h_port, url, timeout, body);
}

// Other helpers
// `````````````

Verbosity Client::get_verbosity() const noexcept {
  return settings_.verbosity;
}

}  // namespace libndt
}  // namespace measurement_kit
#endif
