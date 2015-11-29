#ifndef TEST_H
#define TEST_H

#include "gtest/gtest.h"

#include <inttypes.h>

using namespace virtmem;

typedef TStdioVirtPtr<uint8_t>::type UCharVirtPtr;
typedef TStdioVirtPtr<char>::type CharVirtPtr;

class VAllocFixture: public ::testing::Test
{
protected:
    StdioVAlloc<> valloc;

public:
    void SetUp(void) { valloc.setPoolSize(1024 * 1024 * 10); valloc.start(); }
    void TearDown(void) { valloc.stop(); }
};

template <typename T> class VPtrFixture: public VAllocFixture
{
protected:
    typename TStdioVirtPtr<T>::type vptr;

public:
    VPtrFixture(void) : vptr() { } // UNDONE: we need this for proper construction, problem?
};

#if 0
// From http://stackoverflow.com/a/17236988
inline void print128int(__uint128_t x)
{
    printf("__int128: %016" PRIx64 "%016" PRIx64 "\n",(uint64_t)(x>>64),(uint64_t)x);
}
#endif

#endif // TEST_H
