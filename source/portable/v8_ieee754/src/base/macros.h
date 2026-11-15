// Minimal stand-in for the parts of V8 10.2.154.26 src/base/{macros.h,compiler-specific.h,overflowing-math.h,build_config.h,base-export.h}
// that ieee754.cc / ieee754.h use. The two upstream files themselves are unmodified.
#pragma once
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#if defined(_MSC_VER) && !defined(__clang__)
#define V8_CC_MSVC 1
#endif
#define V8_INLINE inline
#define V8_WARN_UNUSED_RESULT
#define V8_BASE_EXPORT

namespace v8 {
namespace base {

// macros.h: memcpy based bit_cast
template <class Dest, class Source>
inline Dest bit_cast(const Source& source) {
  static_assert(sizeof(Dest) == sizeof(Source), "bit_cast size mismatch");
  Dest dest;
  memcpy(&dest, &source, sizeof(dest));
  return dest;
}

// overflowing-math.h: plain IEEE division
template <typename T>
inline T Divide(T x, T y) {
  return x / y;
}

// overflowing-math.h: two's complement wraparound arithmetic (unsigned arithmetic, converted back)
template <typename T>
inline T NegateWithWraparound(T x) {
  using U = std::make_unsigned_t<T>;
  return static_cast<T>(U{0} - static_cast<U>(x));
}
template <typename T>
inline T SubWithWraparound(T a, T b) {
  using U = std::make_unsigned_t<T>;
  return static_cast<T>(static_cast<U>(a) - static_cast<U>(b));
}

}  // namespace base
}  // namespace v8
