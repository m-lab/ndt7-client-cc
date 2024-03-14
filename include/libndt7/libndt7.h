// Part of Measurement Lab <https://www.measurementlab.net/>.
// Measurement Lab libndt7 is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.
#ifndef MEASUREMENTLAB_LIBNDT7_API_H
#define MEASUREMENTLAB_LIBNDT7_API_H

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#ifndef LIBNDT7_SINGLE_INCLUDE
#include "third_party/github.com/nlohmann/json/json_fwd.hpp"
#endif  // !LIBNDT7_SINGLE_INCLUDE

/// \file libndt7.h
///
/// \brief Public header of m-lab/ndt7-client-cc libndt7. The basic usage is a
/// simple as creating a `libndt7::Client c` instance and then calling
/// `c.run()`. More advanced usage may require you to create a subclass of
/// `libndt7::Client` and override specific virtual methods to customize the
/// behaviour.
///
/// This implementation provides version 7 of the NDT protocol (aka ndt7). The
/// code in this library includes C2S (upload) and S2C (download) ndt7 subtests
/// based on the ndt7 specification, which is described at \see
/// https://github.com/m-lab/ndt-server/blob/master/spec/ndt7-protocol.md.
///
/// Throughout this file, we'll use NDT or ndt7 interchangeably to indicate
/// version 7 of the protocol.
///
/// \remark As a general rule, what is not documented using Doxygen comments
/// inside of this file is considered either internal or experimental. We
/// recommend you to only use documented interfaces.
///
/// Usage example follows. We assume that you have downloaded the single include
/// headers of nlohmann/json >= 3.0.0 and of libndt7.
///
/// ```
/// #include "json.hpp"
/// #include "libndt7.h"
/// measurementlab::libndt7::Client client;
/// client.run();
/// ```
///
/// \warning Not including nlohmann/json before including libndt7 will cause
/// the build to fail, because libndt7 uses nlohmann/json symbols.

#ifndef LIBNDT7_SINGLE_INCLUDE
typedef struct ssl_st SSL;
struct pollfd;
#endif  // !LIBNDT7_SINGLE_INCLUDE

namespace measurementlab {
namespace libndt7 {

#ifndef LIBNDT7_SINGLE_INCLUDE
namespace internal {
enum class Err;
class Sys;
using Size = uint64_t;
#ifdef _WIN32
using Socket = SOCKET;
#else
using Socket = int;
#endif
}  // namespace internal
using Timeout = unsigned int;
#endif  // !LIBNDT7_SINGLE_INCLUDE

// Structure to store extracted URL parts
struct UrlParts {
  std::string scheme;
  std::string host;
  std::string port;
  std::string path;
};

// Exported for testing.
UrlParts parse_ws_url(const std::string &url);

std::string format_http_params(
    const std::map<std::string, std::string> &params);

// Utility functions.
double compute_speed_kbits(double data_bytes, double elapsed_sec) noexcept;

std::string format_speed_from_kbits(double data_bytes,
                                    double elapsed_sec) noexcept;

// Versioning
// ``````````

/// Type containing a version number.
using Version = unsigned int;

/// Major API version number of m-lab/ndt7-client-cc libndt7.
constexpr Version version_major = Version{0};

/// Minor API version number of m-lab/ndt7-client-cc libndt7.
constexpr Version version_minor = Version{27};

/// Patch API version number of m-lab/ndt7-client-cc libndt7.
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
  /// to write the message onto the `std::clog` standard stream. \warning This
  /// method could be called from a different thread context.
  virtual void on_info(const std::string &s) const noexcept = 0;

  /// Called when a debug message is emitted. The default behavior is
  /// to write the message onto the `std::clog` standard stream. \warning This
  /// method could be called from a different thread context.
  virtual void on_debug(const std::string &s) const noexcept = 0;

  /// Called to inform you about the measured speed. The default behavior is
  /// to write the provided information as an info message. @param tid is either
  /// nettest_flag_download or nettest_flag_upload. @param nflows is the number
  /// of used flows. @param measured_bytes is the number of bytes received
  /// or sent since the beginning of the measurement. @param elapsed_sec
  /// is the number of seconds elapsed since the beginning of the nettest.
  /// @param max_runtime is the maximum runtime of this nettest, as copied from
  /// the Settings. @remark By dividing @p elapsed_sec by @p max_runtime, you
  /// can get the percentage of completion of the current nettest. @remark We
  /// provide you with @p tid, so you know whether the nettest is downloading
  /// bytes from the server or uploading bytes to the server. \warning This
  /// method could be called from another thread context.
  virtual void on_performance(NettestFlags tid, uint8_t nflows,
                              double measured_bytes, double elapsed_sec,
                              double max_runtime) noexcept = 0;

  /// Called to provide you with NDT results. The default behavior is to write
  /// the provided information as an info message. @param scope is "tcp_info"
  /// when we're passing you TCP info variables, "summary" when we're passing
  /// you summary variables, or "ndt7" when we're passing you results returned
  /// by a ndt7 server. @param name is the name of the variable; if @p scope is
  /// "ndt7", then @p name should be "download". @param value is the variable
  /// value; variables are serialized JSON returned by the server when
  /// running an ndt7 test. \warning This method could be called from another
  /// thread context.
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

// Settings
// ````````

/// NDT client settings. If you do not customize the settings when creating
/// a Client, the defaults listed below will be used instead.
class Settings {
 public:
  /// Base URL to be used to query the Locate API service. If you specify an
  /// explicit hostname, Locate API won't be used. Note that the URL specified
  /// here MUST NOT end with a final slash.
  std::string locate_api_base_url = "https://locate.measurementlab.net";

  /// Timeout used for I/O operations.
  Timeout timeout = Timeout{7} /* seconds */;

  /// Host name of the NDT server to use. If this is left blank (the default),
  /// we will use Locate API to discover a nearby server.
  std::string hostname;

  /// Port of the NDT server to use. If this is not specified, we will use
  /// the most correct port depending on the configuration.
  std::string port = "443";

  /// Scheme to use connecting to the NDT server. If this is not specified, we
  /// will use the secure websocket configuration.
  std::string scheme = "wss";

  /// The tests you want to run with the NDT server. By default we run
  /// a download test, because that is probably the typical usage.
  NettestFlags nettest_flags = nettest_flag_download;

  /// Verbosity of the client. By default no message is emitted. Set to other
  /// values to get more messages (useful when debugging).
  Verbosity verbosity = verbosity_quiet;

  /// Metadata to include in the server side logs. By default we just identify
  /// the client version and the library.
  std::map<std::string, std::string> metadata{
      {"client_library_version", "v0.1.0"},
      {"client_library_name", "m-lab/libndt7-cc"},
  };

  /// user_agent is the user agent provided for Locate API requests.
  std::string user_agent = "libndt7-cc-agent/v0.1.0";

  /// Type of NDT protocol that you want to use. Selecting the protocol may
  /// cause libndt7 to use different default settings for the port or for
  /// the Locate API. Clear text ndt7 uses port 80, ndt7-over-TLS uses 443.
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

// SummaryData
// ```````````

// SummaryData contains the fields that summarize a completed test.
struct SummaryData {
  // Download speed in kbit/s.
  double download_speed;

  // Upload speed in kbit/s.
  double upload_speed;

  // Download retransmission rate (bytes_retrans / bytes_sent).
  double download_retrans;

  // Upload retransmission rate (bytes_retrans / bytes_sent).
  double upload_retrans;

  // TCPInfo's MinRTT (microseconds).
  uint32_t min_rtt;
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

  /// Runs an ndt7 test based on the configured settings. On success, `run`
  /// returns true. When using the Locate API, `run` will attempt a test with
  /// multiple servers, stopping on the first success or continue trying the
  /// next server on failure. If all attempts fail, `run` returns false.
  bool run() noexcept;

  // After running a successful test with `run`, `get_summary` returns the test
  // summary metrics.
  SummaryData get_summary() noexcept;

  void on_warning(const std::string &s) const noexcept override;

  void on_info(const std::string &s) const noexcept override;

  void on_debug(const std::string &s) const noexcept override;

  void on_performance(NettestFlags tid, uint8_t nflows, double measured_bytes,
                      double elapsed_sec, double max_runtime) noexcept override;

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
  // If you're just interested to use m-lab/ndt7-client-cc libndt7, you can stop
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
  virtual bool query_locate_api(const std::map<std::string, std::string> &opts,
                                std::vector<nlohmann::json> *urls) noexcept;
  virtual std::string get_static_locate_result(std::string opts,
                                               std::string scheme,
                                               std::string hostname,
                                               std::string port);
  virtual std::string replace_all_with(std::string templ, std::string pattern,
                                       std::string replace);

  // ndt7 protocol API
  // `````````````````
  //
  // This API allows you to perform ndt7 tests. The plan is to increasingly
  // use ndt7 code and eventually deprecate and remove NDT.
  //
  // Note that we cannot have ndt7 without OpenSSL.

  // ndt7_download performs a ndt7 download. Returns true if the download
  // succeeds and false in case of failure.
  bool ndt7_download(const UrlParts &url) noexcept;

  // ndt7_upload is like ndt7_download but performs an upload.
  bool ndt7_upload(const UrlParts &url) noexcept;

  // ndt7_connect connects to @p url_path.
  bool ndt7_connect(const UrlParts &url) noexcept;

  // WebSocket
  // `````````
  //
  // This section contain a WebSocket implementation.

  // Send @p line over @p fd.
  virtual internal::Err ws_sendln(internal::Socket fd,
                                  std::string line) noexcept;

  // Receive shorter-than @p maxlen @p *line over @p fd.
  virtual internal::Err ws_recvln(internal::Socket fd, std::string *line,
                                  size_t maxlen) noexcept;

  // Perform websocket handshake. @param fd is the socket to use. @param
  // ws_flags specifies what headers to send and to expect (for more information
  // see the ws_f_xxx constants defined below). @param ws_protocol specifies
  // what protocol to specify as Sec-WebSocket-Protocol in the upgrade request.
  // @param port is used to construct the Host header. @param url_path is the
  // URL path to use for performing the websocket upgrade.
  virtual internal::Err ws_handshake(internal::Socket fd, std::string port,
                                     uint64_t ws_flags, std::string ws_protocol,
                                     std::string url_path) noexcept;

  // Prepare and return a WebSocket frame containing @p first_byte and
  // the content of @p base and @p count as payload. If @p base is nullptr
  // then we'll just not include a body in the prepared frame.
  virtual std::string ws_prepare_frame(uint8_t first_byte, uint8_t *base,
                                       internal::Size count) const noexcept;

  // Send @p count bytes from @p base over @p sock as a frame whose first byte
  // @p first_byte should contain the opcode and possibly the FIN flag.
  virtual internal::Err ws_send_frame(internal::Socket sock, uint8_t first_byte,
                                      uint8_t *base,
                                      internal::Size count) const noexcept;

  // Receive a frame from @p sock. Puts the opcode in @p *opcode. Puts whether
  // there is a FIN flag in @p *fin. The buffer starts at @p base and it
  // contains @p total bytes. Puts in @p *count the actual number of bytes
  // in the message. @return The error that occurred or Err::none.
  internal::Err ws_recv_any_frame(internal::Socket sock, uint8_t *opcode,
                                  bool *fin, uint8_t *base,
                                  internal::Size total,
                                  internal::Size *count) const noexcept;

  // Receive a frame. Automatically and transparently responds to PING, ignores
  // PONG, and handles CLOSE frames. Arguments like ws_recv_any_frame().
  internal::Err ws_recv_frame(internal::Socket sock, uint8_t *opcode, bool *fin,
                              uint8_t *base, internal::Size total,
                              internal::Size *count) const noexcept;

  // Receive a message consisting of one or more frames. Transparently handles
  // PING and PONG frames. Handles CLOSE frames. @param sock is the socket to
  // use. @param opcode is where the opcode is returned. @param base is the
  // beginning of the buffer. @param total is the size of the buffer. @param
  // count contains the actual message size. @return An error on failure or
  // Err::none in case of success.
  internal::Err ws_recvmsg(internal::Socket sock, uint8_t *opcode,
                           uint8_t *base, internal::Size total,
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
  //   succeeds, it then attempts to establish a TLS connection (if enabled);
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
                                          const std::string &port,
                                          uint64_t ws_flags,
                                          std::string ws_protocol,
                                          std::string url_path,
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
  virtual internal::Err netx_dial(const std::string &hostname,
                                  const std::string &port,
                                  internal::Socket *sock) noexcept;

  // Receive from the network.
  virtual internal::Err netx_recv(internal::Socket fd, void *base,
                                  internal::Size count,
                                  internal::Size *actual) const noexcept;

  // Receive from the network without blocking.
  virtual internal::Err netx_recv_nonblocking(
      internal::Socket fd, void *base, internal::Size count,
      internal::Size *actual) const noexcept;

  // Receive exactly N bytes from the network.
  virtual internal::Err netx_recvn(internal::Socket fd, void *base,
                                   internal::Size count) const noexcept;

  // Send data to the network.
  virtual internal::Err netx_send(internal::Socket fd, const void *base,
                                  internal::Size count,
                                  internal::Size *actual) const noexcept;

  // Send to the network without blocking.
  virtual internal::Err netx_send_nonblocking(
      internal::Socket fd, const void *base, internal::Size count,
      internal::Size *actual) const noexcept;

  // Send exactly N bytes to the network.
  virtual internal::Err netx_sendn(internal::Socket fd, const void *base,
                                   internal::Size count) const noexcept;

  // Resolve hostname into a list of IP addresses.
  virtual internal::Err netx_resolve(const std::string &hostname,
                                     std::vector<std::string> *addrs) noexcept;

  // Set socket non blocking.
  virtual internal::Err netx_setnonblocking(internal::Socket fd,
                                            bool enable) noexcept;

  // Pauses until the socket becomes readable.
  virtual internal::Err netx_wait_readable(internal::Socket,
                                           Timeout timeout) const noexcept;

  // Pauses until the socket becomes writeable.
  virtual internal::Err netx_wait_writeable(internal::Socket,
                                            Timeout timeout) const noexcept;

  // Main function for dealing with I/O patterned after poll(2).
  virtual internal::Err netx_poll(std::vector<pollfd> *fds,
                                  int timeout_msec) const noexcept;

  // Shutdown both ends of a socket.
  virtual internal::Err netx_shutdown_both(internal::Socket fd) noexcept;

  // Close a socket.
  virtual internal::Err netx_closesocket(internal::Socket fd) noexcept;

  virtual bool query_locate_api_curl(const std::string &url, long timeout,
                                     std::string *body) noexcept;

  // Other helpers

  Verbosity get_verbosity() const noexcept;

  // Reference to overridable system dependencies
  std::unique_ptr<internal::Sys> sys;

 protected:
  SummaryData summary_;

  // ndt7 Measurement object.
  std::unique_ptr<nlohmann::json> measurement_;

  // ndt7 ConnectionInfo object.
  std::unique_ptr<nlohmann::json> connection_info_;

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

}  // namespace libndt7
}  // namespace measurementlab

#endif  // MEASUREMENTLAB_LIBNDT7_API_H
