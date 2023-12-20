// Part of Measurement Kit <https://measurement-kit.github.io/>.
// Measurement Kit is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.
#ifndef MEASUREMENT_KIT_LIBNDT7_INTERNAL_ERR_HPP
#define MEASUREMENT_KIT_LIBNDT7_INTERNAL_ERR_HPP

// libndt7/internal/err.hpp - definition of error

#include <climits>
#include <sstream>
#include <string>

#include <openssl/err.h>

namespace measurement_kit {
namespace libndt7 {
namespace internal {

enum class Err {
  none,
  //
  // Error codes that map directly to errno values. Here we use the naming used
  // by the C++ library <https://en.cppreference.com/w/cpp/error/errc>.
  //
  broken_pipe,
  connection_aborted,
  connection_refused,
  connection_reset,
  function_not_supported,
  host_unreachable,
  interrupted,
  invalid_argument,
  io_error,
  message_size,
  network_down,
  network_reset,
  network_unreachable,
  operation_in_progress,
  operation_would_block,
  timed_out,
  value_too_large,
  //
  // Getaddrinfo() error codes. See <http://man.openbsd.org/gai_strerror>.
  //
  ai_generic,
  ai_again,
  ai_fail,
  ai_noname,
  //
  // SSL error codes. See <http://man.openbsd.org/SSL_get_error>.
  //
  ssl_generic,
  ssl_want_read,
  ssl_want_write,
  ssl_syscall,
  //
  // Libndt misc error codes.
  //
  eof,       // We got an unexpected EOF
  socks5h,   // SOCKSv5 protocol error
  ws_proto,  // WebSocket protocol error
};

std::string libndt_perror(Err err) noexcept;
std::string ssl_format_error() noexcept;

std::string libndt_perror(Err err) noexcept {
  std::string rv;
  //
#define LIBNDT7_PERROR(value) \
  case Err::value: rv = #value; break
  //
  switch (err) {
    LIBNDT7_PERROR(none);
    LIBNDT7_PERROR(broken_pipe);
    LIBNDT7_PERROR(connection_aborted);
    LIBNDT7_PERROR(connection_refused);
    LIBNDT7_PERROR(connection_reset);
    LIBNDT7_PERROR(function_not_supported);
    LIBNDT7_PERROR(host_unreachable);
    LIBNDT7_PERROR(interrupted);
    LIBNDT7_PERROR(invalid_argument);
    LIBNDT7_PERROR(io_error);
    LIBNDT7_PERROR(message_size);
    LIBNDT7_PERROR(network_down);
    LIBNDT7_PERROR(network_reset);
    LIBNDT7_PERROR(network_unreachable);
    LIBNDT7_PERROR(operation_in_progress);
    LIBNDT7_PERROR(operation_would_block);
    LIBNDT7_PERROR(timed_out);
    LIBNDT7_PERROR(value_too_large);
    LIBNDT7_PERROR(eof);
    LIBNDT7_PERROR(ai_generic);
    LIBNDT7_PERROR(ai_again);
    LIBNDT7_PERROR(ai_fail);
    LIBNDT7_PERROR(ai_noname);
    LIBNDT7_PERROR(socks5h);
    LIBNDT7_PERROR(ssl_generic);
    LIBNDT7_PERROR(ssl_want_read);
    LIBNDT7_PERROR(ssl_want_write);
    LIBNDT7_PERROR(ssl_syscall);
    LIBNDT7_PERROR(ws_proto);
  }
#undef LIBNDT7_PERROR  // Tidy
  //
  if (err == Err::ssl_generic) {
    rv += ": ";
    rv += ssl_format_error();
  }
  //
  return rv;
}

std::string ssl_format_error() noexcept {
  std::stringstream ss;
  for (unsigned short i = 0; i < USHRT_MAX; ++i) {
    unsigned long err = ERR_get_error();
    if (err == 0) {
      break;
    }
    ss << ((i > 0) ? ": " : "") << ERR_reason_error_string(err);
  }
  return ss.str();
}

}  // namespace internal
}  // namespace libndt7
}  // namespace measurement_kit
#endif  // MEASUREMENT_KIT_LIBNDT7_INTERNAL_ERR_HPP
