// Part of Measurement Kit <https://measurement-kit.github.io/>.
// Measurement Kit is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.
#ifndef MEASUREMENT_KIT_LIBNDT_INTERNAL_ASSERT_HPP
#define MEASUREMENT_KIT_LIBNDT_INTERNAL_ASSERT_HPP

// libndt/internal/assert.hpp - assert API

#include <cstdlib>

// LIBNDT_ASSERT is an assert you cannot disable.
#define LIBNDT_ASSERT(condition) \
  if (!(condition)) abort()

#endif
