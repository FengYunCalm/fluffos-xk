/*
 * type.h
 *
 *  Created on: Nov 15, 2014
 *      Author: sunyc
 */

#ifndef SRC_LPC_TYPE_H_
#define SRC_LPC_TYPE_H_

#include <cmath>
#include <cstdint>
#include <limits>      // for max/min
#include <inttypes.h>  // for PRId64

/* type definiation for LPC types. */
typedef std::int64_t LPC_INT;
#define LPC_INT_MAX (std::numeric_limits<std::int64_t>::max())
#define LPC_INT_MIN (std::numeric_limits<std::int64_t>::min())
#define LPC_INT_FMTSTR_P PRId64

typedef double LPC_FLOAT;
#define LPC_FLOAT_MAX (std::numeric_limits<double>::max())
#define LPC_FLOAT_MIN (std::numeric_limits<double>::min())
#define LPC_FLOAT_FMTSTR_P "f"

// Floating-point to LPC_INT conversion is only defined for finite values in
// the half-open interval [-2^63, 2^63). The upper bound must remain exclusive:
// converting LPC_INT_MAX to double rounds to 2^63.
inline bool lpc_float_to_int(LPC_FLOAT value, LPC_INT* result) {
  constexpr LPC_FLOAT kIntUpperExclusive = 0x1p63;
  if (result == nullptr || !std::isfinite(value) || value < -kIntUpperExclusive ||
      value >= kIntUpperExclusive) {
    return false;
  }
  *result = static_cast<LPC_INT>(value);
  return true;
}

#endif /* SRC_LPC_TYPE_H_ */
