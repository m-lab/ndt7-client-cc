// Part of Measurement Lab <https://www.measurementlab.net/>.
// Measurement Lab libndt7 is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.
#ifndef MEASUREMENTLAB_LIBNDT7_INTERNAL_ASSERT_HPP
#define MEASUREMENTLAB_LIBNDT7_INTERNAL_ASSERT_HPP

// libndt7/internal/assert.hpp - assert API

#include <cstdlib>

// LIBNDT7_ASSERT is an assert you cannot disable.
#define LIBNDT7_ASSERT(condition) \
  if (!(condition)) abort()

#endif
