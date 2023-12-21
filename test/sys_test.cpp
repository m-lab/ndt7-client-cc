// Part of Measurement Lab <https://www.measurementlab.net/>.
// Measurement Lab libndt7 is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.

#include "libndt7/internal/sys.hpp"

#include <string.h>

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

TEST_CASE("strtonum() deals with minval greater than maxval") {
  const char *errstr = nullptr;
  Sys sys{};
  REQUIRE(sys.Strtonum("0", 10, 9, &errstr) == 0);
  REQUIRE(strcmp(errstr, "invalid") == 0);
}

TEST_CASE("strtonum() deals with empty string") {
  const char *errstr = nullptr;
  Sys sys{};
  REQUIRE(sys.Strtonum("", 0, 128, &errstr) == 0);
  REQUIRE(strcmp(errstr, "invalid") == 0);
}

TEST_CASE("strtonum() deals with non-number") {
  const char *errstr = nullptr;
  Sys sys{};
  REQUIRE(sys.Strtonum("foo", 0, 128, &errstr) == 0);
  REQUIRE(strcmp(errstr, "invalid") == 0);
}

TEST_CASE("strtonum() deals with characters at end of number") {
  const char *errstr = nullptr;
  Sys sys{};
  REQUIRE(sys.Strtonum("17foo", 0, 128, &errstr) == 0);
  REQUIRE(strcmp(errstr, "invalid") == 0);
}

TEST_CASE("strtonum() deals with too small input number") {
  const char *errstr = nullptr;
  Sys sys{};
  REQUIRE(sys.Strtonum("1", 17, 128, &errstr) == 0);
  REQUIRE(strcmp(errstr, "too small") == 0);
}

TEST_CASE("strtonum() deals with too large input number") {
  const char *errstr = nullptr;
  Sys sys{};
  REQUIRE(sys.Strtonum("130", 17, 128, &errstr) == 0);
  REQUIRE(strcmp(errstr, "too large") == 0);
}
