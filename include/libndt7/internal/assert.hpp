// Part of Measurement Kit <https://measurement-kit.github.io/>.
// Measurement Kit is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.
#ifndef MEASUREMENT_KIT_LIBNDT7_INTERNAL_ASSERT_HPP
#define MEASUREMENT_KIT_LIBNDT7_INTERNAL_ASSERT_HPP

// libndt/internal/assert.hpp - assert API

#include <cstdlib>

// LIBNDT7_ASSERT is an assert you cannot disable.
#define LIBNDT7_ASSERT(condition) \
  if (!(condition)) abort()

#endif
