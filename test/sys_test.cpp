// Part of Measurement Kit <https://measurement-kit.github.io/>.
// Measurement Kit is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.

#include "libndt7/internal/sys.hpp"

#include <string.h>

#define CATCH_CONFIG_MAIN
#include "third_party/github.com/catchorg/Catch2/catch.hpp"

using namespace measurement_kit::libndt::internal;

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
