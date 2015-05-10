#ifndef VIRTMEM_UTILS_H
#define VIRTMEM_UTILS_H

#ifndef ARDUINO
#include <assert.h>
#define ASSERT assert
#else
#include <Arduino.h>

// From https://forum.pjrc.com/threads/23256-Get-Free-Memory-for-Teensy-3-0?p=34242&viewfull=1#post34242
inline void freeRam(const char *msg)
{
    uint32_t stacktop;
    uint32_t heaptop;

    // current position of the stack.
    stacktop = (uint32_t)&stacktop;

    // current position of heap.
    void *top = malloc(1);
    heaptop = (uint32_t)top;
    free(top);

    // The difference is the free, available ram.
    Serial.print(msg); Serial.println(stacktop - heaptop);

//    return stacktop - heaptop;
}

#if 1
#define ASSERT(x) \
    do \
    { \
        if ((x)) ; else \
        { \
            Serial.print("Assertion failed!: "); Serial.print(#x); Serial.print(" @ "); Serial.print(__FILE__); Serial.print(":"); Serial.println(__LINE__); \
            while (true) \
                ; \
        } \
    } \
    while (false);
#else
#define ASSERT(x)
#endif

#endif

namespace private_utils {

template <typename T> T minimal(const T &v1, const T &v2) { return (v1 < v2) ? v1 : v2; }
template <typename T> T maximal(const T &v1, const T &v2) { return (v1 > v2) ? v1 : v2; }

template <typename T> struct TAntiConst { typedef T type; };
template <typename T> struct TAntiConst<const T> { typedef T type; };

}

#endif // VIRTMEM_UTILS_H
