#ifndef UTILS_H
#define UTILS_H

#ifndef ARDUINO
#include <assert.h>
#define ASSERT assert
#else
#define ASSERT(x)
#endif

// Arduino define min/max macros, temporarily get rid of them
#ifdef min
#pragma push_macro("min")
#undef min
#endif

#ifdef max
#undef max
#pragma push_macro("max")
#endif

namespace private_utils {

template <typename T> T min(const T &v1, const T &v2) { return (v1 < v2) ? v1 : v2; }
template <typename T> T max(const T &v1, const T &v2) { return (v1 > v2) ? v1 : v2; }

template <typename T> struct TAntiConst { typedef T type; };
template <typename T> struct TAntiConst<const T> { typedef T type; };

}

#pragma pop_macro("min")
#pragma pop_macro("max")

#endif // UTILS_H
