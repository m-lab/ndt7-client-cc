// Part of Measurement Kit <https://measurement-kit.github.io/>.
// Measurement Kit is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.

#include "third_party/github.com/nlohmann/json/json.hpp"

#include "libndt7/libndt7.hpp"

#ifndef _WIN32
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include <errno.h>
#ifndef _WIN32
#include <fcntl.h>
#endif
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <deque>
#include <vector>

#define CATCH_CONFIG_MAIN
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#include "third_party/github.com/catchorg/Catch2/catch.hpp"
#pragma GCC diagnostic pop

#ifdef _WIN32
#define OS_EINVAL WSAEINVAL
#define OS_EWOULDBLOCK WSAEWOULDBLOCK
#else
#define OS_EINVAL EINVAL
#define OS_EWOULDBLOCK EWOULDBLOCK
#endif

using namespace measurement_kit::libndt7;

// Unit tests
// ==========
//
// Speaking of coverage, if specific code is already tested by running the
// example client, we don't need to write also a test for it here.

// UrlParts Client::parse_ws_url(const std::string& url) tests
// -----------------------------------------------------------

TEST_CASE("Client::parse_ws_url() table tests") {
  struct Test {
    std::string url;
    UrlParts want;
  };
  Test cases[] = {
    {
      .url = "ws://test:80/",
      .want = {.scheme = "ws", .host = "test", .port = "80", .path = "/"},
    },
    {
      .url = "wss://this.example.com/path/to/something",
      .want = {.scheme = "wss", .host = "this.example.com", .port = "443", .path = "/path/to/something"},
    },
    {
      .url = "ws://this.example.com",
      .want = {.scheme = "ws", .host = "this.example.com", .port = "80", .path = ""},
    },
    {
      .url = "wss:///",
      .want = {.scheme = "wss", .host = "", .port = "443", .path = "/"},
    },
    {
      .url = "ws://",
      .want = {.scheme = "ws", .host = "", .port = "80", .path = ""},
    },
    {
      .url = "://",
      .want = {.scheme = "", .host = "", .port = "", .path = ""},
    },
    /*
    TODO(soltesz): support parsing IPv6 hosts.
    {
      .url = "ws://[::1]/test?foo",
      .want = {.scheme = "ws", .host = "[::1]", .port = "80", .path = "/test?foo"},
    },
    */
  };
  for (unsigned long i = 0; i < sizeof(cases)/sizeof(cases[0]); i++ ) {
    auto parts = parse_ws_url(cases[i].url);
    REQUIRE(parts.scheme == cases[i].want.scheme);
    REQUIRE(parts.host == cases[i].want.host);
    REQUIRE(parts.port == cases[i].want.port);
    REQUIRE(parts.path == cases[i].want.path);
  }
}

// std::string format_http_params(const std::map<std::string, std::string>& params);
TEST_CASE("Client::format_http_params() table tests") {
  struct Test {
    std::map<std::string, std::string> params;
    std::string want;
  };
  Test cases[] = {
    {
      .params = {{"key", "value"}, {"name", "okay"}},
      .want = "key=value&name=okay",
    },
    {
      .params = {{"key", "value with space"}, {"name", "okay!@#$"}},
      .want = "key=value%20with%20space&name=okay%21%40%23%24",
    },
  };
  for (unsigned long i = 0; i < sizeof(cases)/sizeof(cases[0]); i++ ) {
    auto got = format_http_params(cases[i].params);
    REQUIRE(got == cases[i].want);
  }
}

// Client::run() tests
// -------------------

class FailQueryMlabns : public Client {
 public:
  using Client::Client;
  bool query_locate_api(const std::map<std::string, std::string>&, std::vector<nlohmann::json>*) noexcept override {
    return false;
  }
};

TEST_CASE("Client::run() deals with Client::query_locate_api() failure") {
  FailQueryMlabns client;
  REQUIRE(client.run() == false);
}

// Client::on_warning() tests
// --------------------------

TEST_CASE("Client::on_warning() works as expected") {
  Client client;
  client.on_warning("calling on_warning() to increase coverage");
}

// Client::query_locate_api() tests
// ----------------------------

class FailQueryMlabnsCurl : public Client {
 public:
  using Client::Client;
  bool query_locate_api_curl(const std::string &, long,
                         std::string *) noexcept override {
    return false;
  }
};

TEST_CASE("Client::query_locate_api() does nothing when we already know hostname") {
  Settings settings;
  settings.hostname = "ndt-mlab1-trn01.mlab-oti.measurement-lab.org";
  FailQueryMlabnsCurl client{settings};
  std::vector<std::string> v;
  std::vector<nlohmann::json> targets;
  std::map<std::string, std::string> metadata;
  REQUIRE(client.query_locate_api(metadata, &targets) == true);
}

TEST_CASE(
    "Client::query_locate_api() deals with Client::query_locate_api_curl() failure") {
  FailQueryMlabnsCurl client;
  std::vector<nlohmann::json> targets;
  std::map<std::string, std::string> metadata;
  REQUIRE(client.query_locate_api(metadata, &targets) == false);
}

class EmptyMlabnsJson : public Client {
 public:
  using Client::Client;
  bool query_locate_api_curl(const std::string &, long,
                         std::string *body) noexcept override {
    *body = "";
    return true;
  }
};

TEST_CASE("Client::query_locate_api() deals with empty JSON") {
  EmptyMlabnsJson client;
  std::vector<nlohmann::json> targets;
  std::map<std::string, std::string> metadata;
  REQUIRE(client.query_locate_api(metadata, &targets) == false);
}

class InvalidMlabnsJson : public Client {
 public:
  using Client::Client;
  bool query_locate_api_curl(const std::string &, long,
                         std::string *body) noexcept override {
    *body = "{{{{";
    return true;
  }
};

TEST_CASE("Client::query_locate_api() deals with invalid JSON") {
  InvalidMlabnsJson client;
  std::vector<nlohmann::json> targets;
  std::map<std::string, std::string> metadata;
  REQUIRE(client.query_locate_api(metadata, &targets) == false);
}

class IncompleteMlabnsJson : public Client {
 public:
  using Client::Client;
  bool query_locate_api_curl(const std::string &, long,
                         std::string *body) noexcept override {
    *body = "{}";
    return true;
  }
};

TEST_CASE("Client::query_locate_api() deals with incomplete JSON") {
  IncompleteMlabnsJson client;
  std::vector<nlohmann::json> targets;
  std::map<std::string, std::string> metadata;
  REQUIRE(client.query_locate_api(metadata, &targets) == false);
}

// Client::netx_maybesocks5h_dial() tests
// --------------------------------------

class FailNetxConnect : public Client {
 public:
  using Client::Client;
	internal::Err netx_dial(const std::string &, const std::string &,
                        internal::Socket *) noexcept override {
    return internal::Err::io_error;
  }
};

TEST_CASE(
    "Client::netx_maybesocks5h_dial() deals with Client::netx_dial() "
    "error when a socks5 port is specified") {
  Settings settings;
  settings.socks5h_port = "9050";
  FailNetxConnect client{settings};
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_maybesocks5h_dial("www.google.com", "80", &sock) ==
          internal::Err::io_error);
}

class Maybesocks5hConnectFailFirstNetxSendn : public Client {
 public:
  using Client::Client;
	internal::Err netx_dial(const std::string &, const std::string &,
                        internal::Socket *sock) noexcept override {
    *sock = 17 /* Something "valid" */;
    return internal::Err::none;
  }
	internal::Err netx_sendn(internal::Socket, const void *,
                         internal::Size) const noexcept override {
    return internal::Err::io_error;
  }
};

TEST_CASE(
    "Client::netx_maybesocks5h_dial() deals with Client::netx_sendn() "
    "failure when sending auth_request") {
  Settings settings;
  settings.socks5h_port = "9050";
  Maybesocks5hConnectFailFirstNetxSendn client{settings};
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_maybesocks5h_dial("www.google.com", "80", &sock) ==
          internal::Err ::io_error);
}

class Maybesocks5hConnectFailFirstNetxRecvn : public Client {
 public:
  using Client::Client;
	internal::Err netx_dial(const std::string &, const std::string &,
                        internal::Socket *sock) noexcept override {
    *sock = 17 /* Something "valid" */;
    return internal::Err::none;
  }
	internal::Err netx_sendn(internal::Socket, const void *,
                         internal::Size) const noexcept override {
    return internal::Err::none;
  }
	internal::Err netx_recvn(internal::Socket, void *,
                         internal::Size) const noexcept override {
    return internal::Err::io_error;
  }
};

TEST_CASE(
    "Client::netx_maybesocks5h_dial() deals with Client::netx_sendn() "
    "failure when receiving auth_response") {
  Settings settings;
  settings.socks5h_port = "9050";
  Maybesocks5hConnectFailFirstNetxRecvn client{settings};
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_maybesocks5h_dial("www.google.com", "80", &sock) ==
          internal::Err::io_error);
}

class Maybesocks5hConnectInvalidAuthResponseVersion : public Client {
 public:
  using Client::Client;
	internal::Err netx_dial(const std::string &, const std::string &,
                        internal::Socket *sock) noexcept override {
    *sock = 17 /* Something "valid" */;
    return internal::Err::none;
  }
	internal::Err netx_sendn(internal::Socket, const void *,
                         internal::Size) const noexcept override {
    return internal::Err::none;
  }
	internal::Err netx_recvn(internal::Socket, void *buf,
                         internal::Size size) const noexcept override {
    assert(size == 2);
    (void)size;
    ((char *)buf)[0] = 4;  // unexpected
    ((char *)buf)[1] = 0;
    return internal::Err::none;
  }
};

TEST_CASE(
    "Client::netx_maybesocks5h_dial() deals with invalid version "
    "number in the auth_response") {
  Settings settings;
  settings.socks5h_port = "9050";
  Maybesocks5hConnectInvalidAuthResponseVersion client{settings};
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_maybesocks5h_dial("www.google.com", "80", &sock) ==
          internal::Err::socks5h);
}

class Maybesocks5hConnectInvalidAuthResponseMethod : public Client {
 public:
  using Client::Client;
	internal::Err netx_dial(const std::string &, const std::string &,
                        internal::Socket *sock) noexcept override {
    *sock = 17 /* Something "valid" */;
    return internal::Err::none;
  }
	internal::Err netx_sendn(internal::Socket, const void *,
                         internal::Size) const noexcept override {
    return internal::Err::none;
  }
	internal::Err netx_recvn(internal::Socket, void *buf,
                         internal::Size size) const noexcept override {
    assert(size == 2);
    (void)size;
    ((char *)buf)[0] = 5;
    ((char *)buf)[1] = 1;
    return internal::Err::none;
  }
};

TEST_CASE(
    "Client::netx_maybesocks5h_dial() deals with invalid method "
    "number in the auth_response") {
  Settings settings;
  settings.socks5h_port = "9050";
  Maybesocks5hConnectInvalidAuthResponseMethod client{settings};
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_maybesocks5h_dial("www.google.com", "80", &sock) ==
          internal::Err::socks5h);
}

class Maybesocks5hConnectInitialHandshakeOkay : public Client {
 public:
  using Client::Client;
	internal::Err netx_dial(const std::string &, const std::string &,
                        internal::Socket *sock) noexcept override {
    *sock = 17 /* Something "valid" */;
    return internal::Err::none;
  }
	internal::Err netx_sendn(internal::Socket, const void *,
                         internal::Size) const noexcept override {
    return internal::Err::none;
  }
	internal::Err netx_recvn(internal::Socket, void *buf,
                         internal::Size size) const noexcept override {
    assert(size == 2);
    (void)size;
    ((char *)buf)[0] = 5;
    ((char *)buf)[1] = 0;
    return internal::Err::none;
  }
};

TEST_CASE("Client::netx_maybesocks5h_dial() deals with too long hostname") {
  Settings settings;
  settings.socks5h_port = "9050";
  Maybesocks5hConnectInitialHandshakeOkay client{settings};
	internal::Socket sock = (internal::Socket)-1;
  std::string hostname;
  for (size_t i = 0; i < 300; ++i) {
    hostname += "A";
  }
  REQUIRE(client.netx_maybesocks5h_dial(hostname, "80", &sock) ==
          internal::Err::invalid_argument);
}

TEST_CASE("Client::netx_maybesocks5h_dial() deals with invalid port") {
  Settings settings;
  settings.socks5h_port = "9050";
  Maybesocks5hConnectInitialHandshakeOkay client{settings};
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_maybesocks5h_dial("www.google.com", "xx", &sock) ==
          internal::Err::invalid_argument);
}

class Maybesocks5hConnectFailSecondNetxSendn : public Client {
 public:
  using Client::Client;
	internal::Err netx_dial(const std::string &, const std::string &,
                        internal::Socket *sock) noexcept override {
    *sock = 17 /* Something "valid" */;
    return internal::Err::none;
  }
	internal::Err netx_sendn(internal::Socket, const void *,
                         internal::Size size) const noexcept override {
    return size == 3 ? internal::Err::none : internal::Err::io_error;
  }
	internal::Err netx_recvn(internal::Socket, void *buf,
                         internal::Size size) const noexcept override {
    assert(size == 2);
    (void)size;
    ((char *)buf)[0] = 5;
    ((char *)buf)[1] = 0;
    return internal::Err::none;
  }
};

TEST_CASE(
    "Client::netx_maybesocks5h_dial() deals with Client::netx_sendn() "
    "error while sending connect_request") {
  Settings settings;
  settings.socks5h_port = "9050";
  Maybesocks5hConnectFailSecondNetxSendn client{settings};
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_maybesocks5h_dial("www.google.com", "80", &sock) ==
          internal::Err::io_error);
}

class Maybesocks5hConnectFailSecondNetxRecvn : public Client {
 public:
  using Client::Client;
	internal::Err netx_dial(const std::string &, const std::string &,
                        internal::Socket *sock) noexcept override {
    *sock = 17 /* Something "valid" */;
    return internal::Err::none;
  }
	internal::Err netx_sendn(internal::Socket, const void *,
                         internal::Size) const noexcept override {
    return internal::Err::none;
  }
	internal::Err netx_recvn(internal::Socket, void *buf,
                         internal::Size size) const noexcept override {
    if (size == 2) {
      ((char *)buf)[0] = 5;
      ((char *)buf)[1] = 0;
      return internal::Err::none;
    }
    return internal::Err::io_error;
  }
};

TEST_CASE(
    "Client::netx_maybesocks5h_dial() deals with Client::recvn() "
    "error while receiving connect_response_hdr") {
  Settings settings;
  settings.socks5h_port = "9050";
  Maybesocks5hConnectFailSecondNetxRecvn client{settings};
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_maybesocks5h_dial("www.google.com", "80", &sock) ==
          internal::Err::io_error);
}

class Maybesocks5hConnectInvalidSecondVersion : public Client {
 public:
  using Client::Client;
	internal::Err netx_dial(const std::string &, const std::string &,
                        internal::Socket *sock) noexcept override {
    *sock = 17 /* Something "valid" */;
    return internal::Err::none;
  }
	internal::Err netx_sendn(internal::Socket, const void *,
                         internal::Size) const noexcept override {
    return internal::Err::none;
  }
	internal::Err netx_recvn(internal::Socket, void *buf,
                         internal::Size size) const noexcept override {
    if (size == 2) {
      ((char *)buf)[0] = 5;
      ((char *)buf)[1] = 0;
      return internal::Err::none;
    }
    if (size == 4) {
      ((char *)buf)[0] = 4;  // unexpected
      ((char *)buf)[1] = 0;
      return internal::Err::none;
    }
    return internal::Err::io_error;
  }
};

TEST_CASE(
    "Client::netx_maybesocks5h_dial() deals with receiving "
    "invalid version number in second Client::recvn()") {
  Settings settings;
  settings.socks5h_port = "9050";
  Maybesocks5hConnectInvalidSecondVersion client{settings};
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_maybesocks5h_dial("www.google.com", "80", &sock) ==
          internal::Err::socks5h);
}

class Maybesocks5hConnectErrorResult : public Client {
 public:
  using Client::Client;
	internal::Err netx_dial(const std::string &, const std::string &,
                        internal::Socket *sock) noexcept override {
    *sock = 17 /* Something "valid" */;
    return internal::Err::none;
  }
	internal::Err netx_sendn(internal::Socket, const void *,
                         internal::Size) const noexcept override {
    return internal::Err::none;
  }
	internal::Err netx_recvn(internal::Socket, void *buf,
                         internal::Size size) const noexcept override {
    if (size == 2) {
      ((char *)buf)[0] = 5;
      ((char *)buf)[1] = 0;
      return internal::Err::none;
    }
    if (size == 4) {
      ((char *)buf)[0] = 5;
      ((char *)buf)[1] = 1;  // error occurred
      return internal::Err::none;
    }
    return internal::Err::io_error;
  }
};

TEST_CASE(
    "Client::netx_maybesocks5h_dial() deals with receiving "
    "an error code in second Client::recvn()") {
  Settings settings;
  settings.socks5h_port = "9050";
  Maybesocks5hConnectErrorResult client{settings};
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_maybesocks5h_dial("www.google.com", "80", &sock) ==
          internal::Err::io_error);
}

class Maybesocks5hConnectInvalidReserved : public Client {
 public:
  using Client::Client;
	internal::Err netx_dial(const std::string &, const std::string &,
                        internal::Socket *sock) noexcept override {
    *sock = 17 /* Something "valid" */;
    return internal::Err::none;
  }
	internal::Err netx_sendn(internal::Socket, const void *,
                         internal::Size) const noexcept override {
    return internal::Err::none;
  }
	internal::Err netx_recvn(internal::Socket, void *buf,
                         internal::Size size) const noexcept override {
    if (size == 2) {
      ((char *)buf)[0] = 5;
      ((char *)buf)[1] = 0;
      return internal::Err::none;
    }
    if (size == 4) {
      ((char *)buf)[0] = 5;
      ((char *)buf)[1] = 0;
      ((char *)buf)[2] = 1;  // should instead be zero
      return internal::Err::none;
    }
    return internal::Err::io_error;
  }
};

TEST_CASE(
    "Client::netx_maybesocks5h_dial() deals with receiving "
    "an invalid reserved field in second Client::recvn()") {
  Settings settings;
  settings.socks5h_port = "9050";
  Maybesocks5hConnectInvalidReserved client{settings};
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_maybesocks5h_dial("www.google.com", "80", &sock) ==
          internal::Err::socks5h);
}

class Maybesocks5hConnectFailAddressNetxRecvn : public Client {
 public:
  using Client::Client;
	internal::Err netx_dial(const std::string &, const std::string &,
                        internal::Socket *sock) noexcept override {
    *sock = 17 /* Something "valid" */;
    return internal::Err::none;
  }
	internal::Err netx_sendn(internal::Socket, const void *,
                         internal::Size) const noexcept override {
    return internal::Err::none;
  }
  uint8_t type = 0;
  std::shared_ptr<bool> seen = std::make_shared<bool>(false);
	internal::Err netx_recvn(internal::Socket, void *buf,
                         internal::Size size) const noexcept override {
    if (size == 2) {
      ((char *)buf)[0] = 5;
      ((char *)buf)[1] = 0;
      return internal::Err::none;
    }
    if (size == 4 && !*seen) {
      *seen = true;  // use flag because IPv4 is also 4 bytes
      assert(type != 0);
      ((char *)buf)[0] = 5;
      ((char *)buf)[1] = 0;
      ((char *)buf)[2] = 0;
      ((char *)buf)[3] = (char)type;  // Sign change safe b/c we're serializing
      return internal::Err::none;
    }
    // the subsequent recvn() will fail
    return internal::Err::io_error;
  }
};

TEST_CASE(
    "Client::netx_maybesocks5h_dial() deals with Client::recvn() "
    "error when reading a IPv4") {
  Settings settings;
  settings.socks5h_port = "9050";
  Maybesocks5hConnectFailAddressNetxRecvn client{settings};
  client.type = 1;
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_maybesocks5h_dial("www.google.com", "80", &sock) ==
          internal::Err::io_error);
}

TEST_CASE(
    "Client::netx_maybesocks5h_dial() deals with Client::recvn() "
    "error when reading a IPv6") {
  Settings settings;
  settings.socks5h_port = "9050";
  Maybesocks5hConnectFailAddressNetxRecvn client{settings};
  client.type = 4;
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_maybesocks5h_dial("www.google.com", "80", &sock) ==
          internal::Err::io_error);
}

TEST_CASE(
    "Client::netx_maybesocks5h_dial() deals with Client::recvn() "
    "error when reading a invalid address type") {
  Settings settings;
  settings.socks5h_port = "9050";
  Maybesocks5hConnectFailAddressNetxRecvn client{settings};
  client.type = 7;
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_maybesocks5h_dial("www.google.com", "80", &sock) ==
          internal::Err::socks5h);
}

class Maybesocks5hConnectWithArray : public Client {
 public:
  using Client::Client;
	internal::Err netx_dial(const std::string &, const std::string &,
                        internal::Socket *sock) noexcept override {
    *sock = 17 /* Something "valid" */;
    return internal::Err::none;
  }
	internal::Err netx_sendn(internal::Socket, const void *,
                         internal::Size) const noexcept override {
    return internal::Err::none;
  }
  std::shared_ptr<std::deque<std::string>> array = std::make_shared<
      std::deque<std::string>>();
	internal::Err netx_recvn(internal::Socket, void *buf,
                         internal::Size size) const noexcept override {
    if (!array->empty() && size == (*array)[0].size()) {
      for (size_t idx = 0; idx < (*array)[0].size(); ++idx) {
        ((char *)buf)[idx] = (*array)[0][idx];
      }
      array->pop_front();
      return internal::Err::none;
    }
    return internal::Err::io_error;
  }
};

TEST_CASE(
    "Client::netx_maybesocks5h_dial() deals with Client::recvn() "
    "error when failing to read domain length") {
  Settings settings;
  settings.socks5h_port = "9050";
  Maybesocks5hConnectWithArray client{settings};
  *client.array = {
      std::string{"\5\0", 2},
      std::string{"\5\0\0\3", 4},
  };
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_maybesocks5h_dial("www.google.com", "80", &sock) ==
          internal::Err::io_error);
}

TEST_CASE(
    "Client::netx_maybesocks5h_dial() deals with Client::recvn() "
    "error when failing to read domain") {
  Settings settings;
  settings.socks5h_port = "9050";
  Maybesocks5hConnectWithArray client{settings};
  *client.array = {
      std::string{"\5\0", 2},
      std::string{"\5\0\0\3", 4},
      std::string{"\7", 1},
  };
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_maybesocks5h_dial("www.google.com", "80", &sock) ==
          internal::Err::io_error);
}

TEST_CASE(
    "Client::netx_maybesocks5h_dial() deals with Client::recvn() "
    "error when failing to read port") {
  Settings settings;
  settings.socks5h_port = "9050";
  Maybesocks5hConnectWithArray client{settings};
  *client.array = {
      std::string{"\5\0", 2},
      std::string{"\5\0\0\3", 4},
      std::string{"\7", 1},
      std::string{"123.org", 7},
  };
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_maybesocks5h_dial("www.google.com", "80", &sock) ==
          internal::Err::io_error);
}

TEST_CASE("Client::netx_maybesocks5h_dial() works with IPv4 (mocked)") {
  Settings settings;
  settings.socks5h_port = "9050";
  Maybesocks5hConnectWithArray client{settings};
  *client.array = {
      std::string{"\5\0", 2},
      std::string{"\5\0\0\1", 4},
      std::string{"\0\0\0\0", 4},
      std::string{"\0\0", 2},
  };
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_maybesocks5h_dial("www.google.com", "80", &sock) ==
          internal::Err::none);
}

TEST_CASE("Client::netx_maybesocks5h_dial() works with IPv6 (mocked)") {
  Settings settings;
  settings.socks5h_port = "9050";
  Maybesocks5hConnectWithArray client{settings};
  *client.array = {
      std::string{"\5\0", 2},
      std::string{"\5\0\0\4", 4},
      std::string{"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16},
      std::string{"\0\0", 2},
  };
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_maybesocks5h_dial("www.google.com", "80", &sock) ==
          internal::Err::none);
}

// Client::netx_map_errno() tests
// ------------------------------

#ifdef _WIN32
#define E(name) WSAE##name
#else
#define E(name) E##name
#endif

TEST_CASE("Client::netx_map_errno() correctly maps all errors") {
  using namespace measurement_kit::libndt7;
#ifdef NDEBUG  // There is an assertion that would fail in DEBUG mode
  REQUIRE(Client::netx_map_errno(0) == internal::Err::io_error);
#endif
#ifndef _WIN32
  REQUIRE(Client::netx_map_errno(E(PIPE)) == internal::Err::broken_pipe);
#endif
  REQUIRE(Client::netx_map_errno(E(CONNABORTED)) == internal::Err::connection_aborted);
  REQUIRE(Client::netx_map_errno(E(CONNREFUSED)) == internal::Err::connection_refused);
  REQUIRE(Client::netx_map_errno(E(CONNRESET)) == internal::Err::connection_reset);
  REQUIRE(Client::netx_map_errno(E(HOSTUNREACH)) == internal::Err::host_unreachable);
  REQUIRE(Client::netx_map_errno(E(INTR)) == internal::Err::interrupted);
  REQUIRE(Client::netx_map_errno(E(INVAL)) == internal::Err::invalid_argument);
#ifndef _WIN32
  REQUIRE(Client::netx_map_errno(E(IO)) == internal::Err::io_error);
#endif
  REQUIRE(Client::netx_map_errno(E(NETDOWN)) == internal::Err::network_down);
  REQUIRE(Client::netx_map_errno(E(NETRESET)) == internal::Err::network_reset);
  REQUIRE(Client::netx_map_errno(E(NETUNREACH)) == internal::Err::network_unreachable);
  REQUIRE(Client::netx_map_errno(E(INPROGRESS)) == internal::Err::operation_in_progress);
  REQUIRE(Client::netx_map_errno(E(WOULDBLOCK)) == internal::Err::operation_would_block);
  REQUIRE(Client::netx_map_errno(E(TIMEDOUT)) == internal::Err::timed_out);
#if !defined _WIN32 && EAGAIN != EWOULDBLOCK
  REQUIRE(Client::netx_map_errno(E(AGAIN)) == internal::Err::operation_would_block);
#endif
}

// Client::netx_map_eai() tests
// ----------------------------

TEST_CASE("Client::netx_map_eai() correctly maps all errors") {
  using namespace measurement_kit::libndt7;
  Client client;
  REQUIRE(client.netx_map_eai(EAI_AGAIN) == internal::Err::ai_again);
  REQUIRE(client.netx_map_eai(EAI_FAIL) == internal::Err::ai_fail);
  REQUIRE(client.netx_map_eai(EAI_NONAME) == internal::Err::ai_noname);
#ifdef EAI_SYSTEM
  {
    client.sys->SetLastError(E(WOULDBLOCK));
    REQUIRE(client.netx_map_eai(EAI_SYSTEM) == internal::Err::operation_would_block);
    client.sys->SetLastError(0);
  }
#endif
}

#undef E  // Tidy

// Client::netx_dial() tests
// -------------------------

TEST_CASE("Client::netx_dial() requires initial socket to be -1") {
  Client client;
	internal::Socket sock = 21;
  REQUIRE(client.netx_dial("1.2.3.4", "33", &sock) ==
          internal::Err::invalid_argument);
}

class FailNetxResolve : public Client {
 public:
  using Client::Client;
	internal::Err netx_resolve(const std::string &,
                           std::vector<std::string> *) noexcept override {
    return internal::Err::ai_again;
  }
};

TEST_CASE("Client::netx_dial() deals with Client::netx_resolve() failure") {
  FailNetxResolve client;
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_dial("1.2.3.4", "33", &sock) == internal::Err::ai_again);
}

class FailGetaddrinfoInNetxConnectClient : public Client {
 public:
  using Client::Client;
	internal::Err netx_resolve(const std::string &str,
                           std::vector<std::string> *addrs) noexcept override {
    REQUIRE(str == "1.2.3.4");  // make sure it did not change
    addrs->push_back(str);
    return internal::Err::none;
  }
};

class FailGetaddrinfoInNetxConnectSys : public internal::Sys {
 public:
  using Sys::Sys;
  int Getaddrinfo(const char *, const char *, const addrinfo *,
                  addrinfo **) const noexcept override {
    return EAI_AGAIN;
  }
};

TEST_CASE("Client::netx_dial() deals with Client::getaddrinfo() failure") {
  FailGetaddrinfoInNetxConnectClient client;
  client.sys.reset(new FailGetaddrinfoInNetxConnectSys{});
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_dial("1.2.3.4", "33", &sock) == internal::Err::ai_again);
}

class FailSocket : public internal::Sys {
 public:
  using Sys::Sys;
	internal::Socket NewSocket(int, int, int) const noexcept override {
    this->SetLastError(OS_EINVAL);
    return (internal::Socket)-1;
  }
};

TEST_CASE("Client::netx_dial() deals with Client::socket() failure") {
  Client client;
  client.sys.reset(new FailSocket{});
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_dial("1.2.3.4", "33", &sock) == internal::Err::io_error);
}

class FailSetnonblocking : public Client {
 public:
  using Client::Client;
	internal::Err netx_setnonblocking(internal::Socket, bool) noexcept override {
    return internal::Err::io_error;
  }
};

TEST_CASE(
    "Client::netx_dial() deals with Client::netx_setnonblocking() failure") {
  FailSetnonblocking client;
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_dial("1.2.3.4", "33", &sock) == internal::Err::io_error);
}

class FailSocketConnectImmediate : public internal::Sys {
 public:
  using Sys::Sys;
  int Connect(  //
      internal::Socket, const sockaddr *, socklen_t) const noexcept override {
    this->SetLastError(OS_EINVAL);
    return -1;
  }
};

TEST_CASE(
    "Client::netx_dial() deals with immediate Client::connect() failure") {
  Client client{};
  client.sys.reset(new FailSocketConnectImmediate);
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_dial("1.2.3.4", "33", &sock) == internal::Err::io_error);
}

#ifdef _WIN32
#define OS_EINPROGRESS WSAEWOULDBLOCK
#else
#define OS_EINPROGRESS EINPROGRESS
#endif

class FailSocketConnectTimeoutClient : public Client {
 public:
  using Client::Client;
	internal::Err netx_poll(std::vector<pollfd> *, int) const noexcept override {
    return internal::Err::timed_out;
  }
};

class FailSocketConnectTimeoutSys : public internal::Sys {
 public:
  using Sys::Sys;
  int Connect(  //
      internal::Socket, const sockaddr *, socklen_t) const noexcept override {
    this->SetLastError(OS_EINPROGRESS);
    return -1;
  }
};

TEST_CASE("Client::netx_dial() deals with Client::connect() timeout") {
  FailSocketConnectTimeoutClient client{};
  client.sys.reset(new FailSocketConnectTimeoutSys{});
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_dial("1.2.3.4", "33", &sock) == internal::Err::io_error);
}

class FailSocketConnectGetsockoptErrorClient : public Client {
 public:
  using Client::Client;
	internal::Err netx_poll(
      std::vector<pollfd> *pfds, int) const noexcept override {
    for (auto &fd : *pfds) {
      fd.revents = fd.events;
    }
    return internal::Err::none;
  }
};

class FailSocketConnectGetsockoptErrorSys : public internal::Sys {
 public:
  using Sys::Sys;
  int Connect(  //
      internal::Socket, const sockaddr *, socklen_t) const noexcept override {
    this->SetLastError(OS_EINPROGRESS);
    return -1;
  }
  int Getsockopt(internal::Socket, int, int, void *,
                 socklen_t *) const noexcept override {
    this->SetLastError(OS_EINVAL);
    return -1;
  }
};

TEST_CASE(
    "Client::netx_dial() deals with Client::connect() getsockopt() error") {
  FailSocketConnectGetsockoptErrorClient client{};
  client.sys.reset(new FailSocketConnectGetsockoptErrorSys{});
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_dial("1.2.3.4", "33", &sock) == internal::Err::io_error);
}

class FailSocketConnectSocketErrorClient : public Client {
 public:
  using Client::Client;
	internal::Err netx_poll(
      std::vector<pollfd> *pfds, int) const noexcept override {
    for (auto &fd : *pfds) {
      fd.revents = fd.events;
    }
    return internal::Err::none;
  }
};

class FailSocketConnectSocketErrorSys : public internal::Sys {
 public:
  using Sys::Sys;
  int Connect(  //
      internal::Socket, const sockaddr *, socklen_t) const noexcept override {
    this->SetLastError(OS_EINPROGRESS);
    return -1;
  }
  virtual int Getsockopt(internal::Socket, int, int, void *value,
                         socklen_t *) const noexcept override {
    int *ivalue = static_cast<int *>(value);
    *ivalue = OS_EINVAL;  // Any error would actually do here
    return 0;
  }
};

TEST_CASE("Client::netx_dial() deals with Client::connect() socket error") {
  FailSocketConnectSocketErrorClient client{};
  client.sys.reset(new FailSocketConnectSocketErrorSys{});
	internal::Socket sock = (internal::Socket)-1;
  REQUIRE(client.netx_dial("1.2.3.4", "33", &sock) == internal::Err::io_error);
}

// Client::netx_recv_nonblocking() tests
// -------------------------------------

TEST_CASE("Client::netx_recv_nonblocking() deals with zero recv correctly") {
  Client client;
  char buf{};
	internal::Size n = 0;
  REQUIRE(client.netx_recv_nonblocking(0, &buf, 0, &n) ==
          internal::Err::invalid_argument);
}

// Client::netx_recvn() tests
// --------------------------

#ifdef _WIN32
#define OS_SSIZE_MAX INT_MAX
#else
#define OS_SSIZE_MAX SSIZE_MAX
#endif

TEST_CASE("Client::netx_recvn() deals with too-large buffer") {
  Client client;
  char buf{};
  REQUIRE(client.netx_recvn(0, &buf, (unsigned long long)OS_SSIZE_MAX + 1) ==
          internal::Err::invalid_argument);
}

class FailNetxRecv : public Client {
 public:
  using Client::Client;
	internal::Err netx_recv(internal::Socket, void *, internal::Size,
                        internal::Size *) const noexcept override {
    return internal::Err::invalid_argument;
  }
};

TEST_CASE("Client::netx_recvn() deals with Client::netx_recv() failure") {
  char buf[1024];
  FailNetxRecv client;
  REQUIRE(client.netx_recvn(0, buf, sizeof(buf)) ==
          internal::Err::invalid_argument);
}

class RecvEof : public internal::Sys {
 public:
  using Sys::Sys;
	internal::Ssize Recv(internal::Socket, void *,
                     internal::Size) const noexcept override {
    return 0;
  }
};

TEST_CASE("Client::netx_recvn() deals with Client::recv() EOF") {
  char buf[1024];
  Client client;
  client.sys.reset(new RecvEof{});
  REQUIRE(client.netx_recvn(0, buf, sizeof(buf)) == internal::Err::eof);
}

class PartialNetxRecvAndThenError : public Client {
 public:
  using Client::Client;
  static constexpr internal::Size amount = 11;
  static constexpr internal::Size good_amount = 3;
	internal::Err netx_recv(internal::Socket, void *buf, internal::Size size,
                        internal::Size *rv) const noexcept override {
    if (size == amount) {
      assert(size >= good_amount);
      for (size_t i = 0; i < good_amount; ++i) {
        ((char *)buf)[i] = 'A';
      }
      *rv = good_amount;
      return internal::Err::none;
    }
    *rv = 0;
    return internal::Err::invalid_argument;
  }
};

TEST_CASE(
    "Client::netx_recvn() deals with partial Client::netx_recv() and then "
    "error") {
  char buf[PartialNetxRecvAndThenError::amount] = {};
  PartialNetxRecvAndThenError client;
  REQUIRE(client.netx_recvn(0, buf, sizeof(buf)) ==
          internal::Err::invalid_argument);
  // Just to make sure the code path was entered correctly. We still think that
  // the right behaviour here is to return -1, not a short read.
  for (size_t i = 0; i < sizeof(buf); ++i) {
    if (i < PartialNetxRecvAndThenError::good_amount) {
      REQUIRE(buf[i] == 'A');
    } else {
      REQUIRE(buf[i] == '\0');
    }
  }
}

class PartialRecvAndThenEof : public internal::Sys {
 public:
  using Sys::Sys;
  static constexpr internal::Size amount = 7;
  static constexpr internal::Size good_amount = 5;
	internal::Ssize Recv(internal::Socket, void *buf,
                     internal::Size size) const noexcept override {
    if (size == amount) {
      assert(size >= good_amount);
      for (size_t i = 0; i < good_amount; ++i) {
        ((char *)buf)[i] = 'B';
      }
      return good_amount;
    }
    return 0;
  }
};

TEST_CASE(
    "Client::netx_recvn() deals with partial Client::recv() and then EOF") {
  char buf[PartialRecvAndThenEof::amount] = {};
  Client client;
  client.sys.reset(new PartialRecvAndThenEof{});
  REQUIRE(client.netx_recvn(0, buf, sizeof(buf)) == internal::Err::eof);
  // Just to make sure the code path was entered correctly. We still think that
  // the right behaviour here is to return zero, not a short read.
  for (size_t i = 0; i < sizeof(buf); ++i) {
    if (i < PartialRecvAndThenEof::good_amount) {
      REQUIRE(buf[i] == 'B');
    } else {
      REQUIRE(buf[i] == '\0');
    }
  }
}

// Client::netx_send_nonblocking() tests
// -------------------------------------

TEST_CASE("Client::netx_send() deals with zero send correctly") {
  Client client;
	internal::Size n = 0;
  char buf{};
  REQUIRE(client.netx_send_nonblocking(0, &buf, 0, &n) ==
          internal::Err::invalid_argument);
}

// Client::netx_sendn() tests
// --------------------------

TEST_CASE("Client::netx_sendn() deals with too-large buffer") {
  Client client;
  char buf{};
  REQUIRE(client.netx_sendn(0, &buf, (unsigned long long)OS_SSIZE_MAX + 1) ==
          internal::Err::invalid_argument);
}

class FailSend : public internal::Sys {
 public:
  using Sys::Sys;
	internal::Ssize Send(internal::Socket, const void *,
                     internal::Size) const noexcept override {
    this->SetLastError(OS_EINVAL);
    return -1;
  }
};

TEST_CASE("Client::netx_sendn() deals with Client::send() failure") {
  char buf[1024];
  Client client;
  client.sys.reset(new FailSend{});
  REQUIRE(client.netx_sendn(0, buf, sizeof(buf)) ==
          internal::Err::invalid_argument);
}

// As much as EOF should not appear on a socket when sending, be ready.
class SendEof : public internal::Sys {
 public:
  using Sys::Sys;
	internal::Ssize Send(internal::Socket, const void *,
                     internal::Size) const noexcept override {
    return 0;
  }
};

TEST_CASE("Client::netx_sendn() deals with Client::send() EOF") {
  char buf[1024];
  Client client;
  client.sys.reset(new SendEof{});
  REQUIRE(client.netx_sendn(0, buf, sizeof(buf)) == internal::Err::io_error);
}

class PartialSendAndThenError : public internal::Sys {
 public:
  using Sys::Sys;
  static constexpr internal::Size amount = 11;
  static constexpr internal::Size good_amount = 3;
  std::shared_ptr<internal::Size> successful = std::make_shared<internal::Size>(0);
	internal::Ssize Send(internal::Socket, const void *,
                     internal::Size size) const noexcept override {
    if (size == amount) {
      assert(size >= good_amount);
      *successful += good_amount;
      return good_amount;
    }
    this->SetLastError(OS_EINVAL);
    return -1;
  }
};

TEST_CASE("Client::send() deals with partial Client::send() and then error") {
  char buf[PartialSendAndThenError::amount] = {};
  Client client;
  auto sys = new PartialSendAndThenError{}; // managed by client
  client.sys.reset(sys);
  REQUIRE(client.netx_sendn(0, buf, sizeof(buf)) ==
          internal::Err::invalid_argument);
  // Just to make sure the code path was entered correctly. We still think that
  // the right behaviour here is to return -1, not a short write.
  //
  // Usage of `exp` is required to make clang compile (unclear to me why).
  auto exp = PartialSendAndThenError::good_amount;
  REQUIRE((*sys->successful) == exp);
}

// See above comment regarding likelihood of send returning EOF (i.e. zero)
class PartialSendAndThenEof : public internal::Sys {
 public:
  using Sys::Sys;
  static constexpr internal::Size amount = 7;
  static constexpr internal::Size good_amount = 5;
  std::shared_ptr<internal::Size> successful = std::make_shared<internal::Size>(0);
	internal::Ssize Send(internal::Socket, const void *,
                     internal::Size size) const noexcept override {
    if (size == amount) {
      assert(size >= good_amount);
      *successful += good_amount;
      return good_amount;
    }
    return 0;
  }
};

TEST_CASE(
    "Client::netx_sendn() deals with partial Client::send() and then EOF") {
  char buf[PartialSendAndThenEof::amount] = {};
  Client client;
  auto sys = new PartialSendAndThenEof{}; // managed by client
  client.sys.reset(sys);
  REQUIRE(client.netx_sendn(0, buf, sizeof(buf)) == internal::Err::io_error);
  // Just to make sure the code path was entered correctly. We still think that
  // the right behaviour here is to return zero, not a short write.
  //
  // Usage of `exp` is required to make clang compile (unclear to me why).
  auto exp = PartialSendAndThenEof::good_amount;
  REQUIRE((*sys->successful) == exp);
}

// Client::netx_resolve() tests
// ----------------------------

class FailGetaddrinfo : public internal::Sys {
 public:
  using Sys::Sys;
  int Getaddrinfo(const char *, const char *, const addrinfo *,
                  addrinfo **) const noexcept override {
    return EAI_AGAIN;
  }
};

TEST_CASE("Client::netx_resolve() deals with Client::getaddrinfo() failure") {
  Client client;
  client.sys.reset(new FailGetaddrinfo{});
  std::vector<std::string> addrs;
  REQUIRE(client.netx_resolve("x.org", &addrs) == internal::Err::ai_again);
}

class FailGetnameinfo : public internal::Sys {
 public:
  using Sys::Sys;
  int Getnameinfo(const sockaddr *, socklen_t, char *, socklen_t, char *,
                  socklen_t, int) const noexcept override {
    return EAI_AGAIN;
  }
};

TEST_CASE("Client::netx_resolve() deals with Client::getnameinfo() failure") {
  Client client;
  client.sys.reset(new FailGetnameinfo{});
  std::vector<std::string> addrs;
  REQUIRE(client.netx_resolve("x.org", &addrs) == internal::Err::ai_generic);
}

// Client::netx_setnonblocking() tests
// -----------------------------------

#ifdef _WIN32

class FailIoctlsocket : public internal::Sys {
 public:
  using Sys::Sys;
  u_long expect = 2UL;  // value that should not be used
  int Ioctlsocket(internal::Socket, long cmd,
                  u_long *value) const noexcept override {
    REQUIRE(cmd == FIONBIO);
    REQUIRE(*value == expect);
    this->SetLastError(WSAEINVAL);
    return -1;
  }
};

TEST_CASE(
    "Client::netx_setnonblocking() deals with Client::ioctlsocket() failure") {
  Client client;
  auto sys = new FailIoctlsocket{}; // managed by client
  client.sys.reset(sys);
  {
    sys->expect = 1UL;
    REQUIRE(client.netx_setnonblocking(17, true) ==
            internal::Err::invalid_argument);
  }
  {
    sys->expect = 0UL;
    REQUIRE(client.netx_setnonblocking(17, false) ==
            internal::Err::invalid_argument);
  }
}

#else

class FailFcntlGet : public internal::Sys {
 public:
  using Sys::Sys;
  using Sys::Fcntl;
  int Fcntl(internal::Socket, int cmd) const noexcept override {
    REQUIRE(cmd == F_GETFL);
    errno = EINVAL;
    return -1;
  }
};

TEST_CASE(
    "Client::netx_setnonblocking() deals with Client::fcntl(F_GETFL) failure") {
  Client client;
  client.sys.reset(new FailFcntlGet{});
  REQUIRE(client.netx_setnonblocking(17, true) ==
          internal::Err::invalid_argument);
}

class FailFcntlSet : public internal::Sys {
 public:
  using Sys::Sys;
  int Fcntl(internal::Socket, int cmd) const noexcept override {
    REQUIRE(cmd == F_GETFL);
    return 0;
  }
  int expect = ~0;  // value that should never appear
  int Fcntl(internal::Socket, int cmd, int flags) const noexcept override {
    REQUIRE(cmd == F_SETFL);
    REQUIRE(flags == expect);
    errno = EINVAL;
    return -1;
  }
};

TEST_CASE(
    "Client::netx_setnonblocking() deals with Client::fcntl(F_SETFL) failure") {
  Client client;
  auto sys = new FailFcntlSet{}; // managed by client
  client.sys.reset(sys);
  {
    sys->expect = O_NONBLOCK;
    REQUIRE(client.netx_setnonblocking(17, true) ==
            internal::Err::invalid_argument);
  }
  {
    sys->expect = 0;
    REQUIRE(client.netx_setnonblocking(17, false) ==
            internal::Err::invalid_argument);
  }
}

#endif  // _WIN32

// Client::netx_poll() tests
// ---------------------------

#ifndef _WIN32

class InterruptPoll : public internal::Sys {
 public:
  using Sys::Sys;
  std::shared_ptr<unsigned int> count = std::make_shared<unsigned int>();
  int Poll(pollfd *, nfds_t, int) const noexcept override {
    if ((*count)++ == 0) {
      this->SetLastError(EINTR);
    } else {
      this->SetLastError(EIO);
    }
    return -1;
  }
};

TEST_CASE("Client::netx_poll() deals with EINTR") {
  pollfd pfd{};
  constexpr internal::Socket sock = 17;
  pfd.fd = sock;
  pfd.events |= POLLIN;
  std::vector<pollfd> pfds;
  pfds.push_back(pfd);
  Client client;
  auto sys = new InterruptPoll{}; // managed by client
  client.sys.reset(sys);
  constexpr int timeout = 100;
  REQUIRE(client.netx_poll(&pfds, timeout) == internal::Err::io_error);
  REQUIRE(*sys->count == 2);
}

#endif  // !_WIN32

class TimeoutPoll : public internal::Sys {
 public:
  using Sys::Sys;
#ifdef _WIN32
  int Poll(LPWSAPOLLFD, ULONG, INT) const noexcept override
#else
  int Poll(pollfd *, nfds_t, int) const noexcept override
#endif
  {
    return 0;
  }
};

TEST_CASE("Client::netx_poll() deals with timeout") {
  pollfd pfd{};
  constexpr internal::Socket sock = 17;
  pfd.fd = sock;
  pfd.events |= POLLIN;
  std::vector<pollfd> pfds;
  pfds.push_back(pfd);
  Client client;
  client.sys.reset(new TimeoutPoll{});
  constexpr int timeout = 100;
  REQUIRE(client.netx_poll(&pfds, timeout) == internal::Err::timed_out);
}

// Client::query_locate_api_curl() tests
// ---------------------------------

#ifdef HAVE_CURL
TEST_CASE("Client::query_locate_api_curl() deals with Curl{} failure") {
  Client client;
  // Note: passing `nullptr` should cause Curl{} to fail and hence we can
  // also easily check for cases where Curl{} fails.
  REQUIRE(client.query_locate_api_curl("", 3, nullptr) == false);
}
#endif

// Client::sys->GetLastError() tests
// ----------------------------------

#ifdef _WIN32
#define OS_EINVAL WSAEINVAL
#else
#define OS_EINVAL EINVAL
#endif

TEST_CASE("Client::sys->GetLastError() works as expected") {
  Client client;
  client.sys->SetLastError(OS_EINVAL);
  REQUIRE(client.sys->GetLastError() == OS_EINVAL);
  client.sys->SetLastError(0);  // clear
  REQUIRE(client.sys->GetLastError() == 0);
}

// Client::recv() tests
// --------------------

TEST_CASE("Sys::recv() deals with too-large buffer") {
  Client client;
  REQUIRE(client.sys->Recv(
        0, nullptr, (unsigned long long)OS_SSIZE_MAX + 1) == -1);
}

// Client::send() tests
// --------------------

TEST_CASE("Sys::send() deals with too-large buffer") {
  Client client;
  REQUIRE(client.sys->Send(
        0, nullptr, (unsigned long long)OS_SSIZE_MAX + 1) == -1);
}
