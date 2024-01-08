// Part of Measurement Lab <https://www.measurementlab.net/>.
// Measurement Lab libndt7 is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.

#include "libndt7/internal/curlx.hpp"

#define CATCH_CONFIG_MAIN
// TODO(github.com/m-lab/ndt7-client-cc/issues/10): Remove pragma ignoring warning when possible.
#if !defined(__clang__) && defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#include "third_party/github.com/catchorg/Catch2/catch.hpp"
#if !defined(__clang__) && defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

using namespace measurementlab::libndt7::internal;

static const Logger &NoLoggerInstance() {
  static NoLogger singleton;
  return singleton;
}

// Curlx::GetMaybeSOCKS5() tests
// -----------------------------

class FailCurlxEasyInit : public Curlx {
 public:
  using Curlx::Curlx;
  UniqueCurl NewUniqueCurl() noexcept override { return {}; }
};

TEST_CASE("Curlx::GetMaybeSOSCKS5() deals with Curlx::NewUniqueCurl() failure") {
  FailCurlxEasyInit curlx{NoLoggerInstance()};
  std::string body;
  REQUIRE(!curlx.GetMaybeSOCKS5("", "http://x.org", 1, &body));
}

class FailCurlxSetoptProxy : public Curlx {
 public:
  using Curlx::Curlx;
  CURLcode SetoptProxy(UniqueCurl &, const std::string &) noexcept override {
    return CURLE_UNSUPPORTED_PROTOCOL;  // any error is okay here
  }
};

TEST_CASE("Curlx::GetMaybeSOCKS5() deals with Curlx::SetoptProxy() failure") {
  FailCurlxSetoptProxy curlx{NoLoggerInstance()};
  std::string body;
  REQUIRE(!curlx.GetMaybeSOCKS5("9050", "http://x.org", 1, &body));
}

// Curlx::Get() tests
// ------------------

TEST_CASE("Curlx::Get() deals with null body") {
  Curlx curlx{NoLoggerInstance()};
  UniqueCurl handle{curlx.NewUniqueCurl()};
  REQUIRE(curlx.Get(handle, "http://x.org", 1, nullptr) == false);
}

class FailCurlxSetoptUrl : public Curlx {
 public:
  using Curlx::Curlx;
  CURLcode SetoptURL(UniqueCurl &, const std::string &) noexcept { return CURLE_AGAIN; }
};

TEST_CASE("Curlx::Get() deals with Curlx::SetoptURL() failure") {
  FailCurlxSetoptUrl curlx{NoLoggerInstance()};
  UniqueCurl handle{curlx.NewUniqueCurl()};
  std::string body;
  REQUIRE(curlx.Get(handle, "http://x.org", 1, &body) == false);
}

class FailCurlxSetoptWritefunction : public Curlx {
 public:
  using Curlx::Curlx;
  CURLcode SetoptWriteFunction(UniqueCurl &, CurlWriteCb) noexcept override {
    return CURLE_AGAIN;
  }
};

TEST_CASE("Curlx::Get() deals with Curlx::SetoptWriteFunction() failure") {
  FailCurlxSetoptWritefunction curlx{NoLoggerInstance()};
  UniqueCurl handle{curlx.NewUniqueCurl()};
  std::string body;
  REQUIRE(curlx.Get(handle, "http://x.org", 1, &body) == false);
}

class FailCurlxSetoptWritedata : public Curlx {
 public:
  using Curlx::Curlx;
  CURLcode SetoptWriteData(UniqueCurl &, void *) noexcept override { return CURLE_AGAIN; }
};

TEST_CASE("Curlx::Get() deals with Curlx::SetoptWriteData() failure") {
  FailCurlxSetoptWritedata curlx{NoLoggerInstance()};
  UniqueCurl handle{curlx.NewUniqueCurl()};
  std::string body;
  REQUIRE(curlx.Get(handle, "http://x.org", 1, &body) == false);
}

class FailCurlxSetoptTimeout : public Curlx {
 public:
  using Curlx::Curlx;
  CURLcode SetoptTimeout(UniqueCurl &, long) noexcept override { return CURLE_AGAIN; }
};

TEST_CASE("Curlx::Get() deals with Curlx::SetoptTimeout() failure") {
  FailCurlxSetoptTimeout curlx{NoLoggerInstance()};
  UniqueCurl handle{curlx.NewUniqueCurl()};
  std::string body;
  REQUIRE(curlx.Get(handle, "http://x.org", 1, &body) == false);
}

class FailCurlxSetoptFailonerror : public Curlx {
 public:
  using Curlx::Curlx;
  CURLcode SetoptFailonerr(UniqueCurl &) noexcept override { return CURLE_AGAIN; }
};

TEST_CASE("Curlx::Get() deals with Curlx::SetoptFailonerror() failure") {
  FailCurlxSetoptFailonerror curlx{NoLoggerInstance()};
  UniqueCurl handle{curlx.NewUniqueCurl()};
  std::string body;
  REQUIRE(curlx.Get(handle, "http://x.org", 1, &body) == false);
}

class FailCurlxPerform : public Curlx {
 public:
  using Curlx::Curlx;
  CURLcode Perform(UniqueCurl &) noexcept override { return CURLE_AGAIN; }
};

TEST_CASE("Curlx::Get() deals with Curlx::Perform() failure") {
  FailCurlxPerform curlx{NoLoggerInstance()};
  UniqueCurl handle{curlx.NewUniqueCurl()};
  std::string body;
  REQUIRE(curlx.Get(handle, "http://x.org", 1, &body) == false);
}

// Curlx::SetoptProxy() tests
// --------------------------

TEST_CASE("Curlx::SetoptProxy() works") {
  Curlx curlx{NoLoggerInstance()};
  UniqueCurl handle{curlx.NewUniqueCurl()};
  REQUIRE(curlx.SetoptProxy(handle, "socks5h://127.0.0.1:9050") == CURLE_OK);
}
