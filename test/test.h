#ifndef TEST_H
#define TEST_H

#include "gtest/gtest.h"

#include <inttypes.h>

typedef TStdioVirtPtr<uint8_t>::type TUCharVirtPtr;

class CAllocFixture: public ::testing::Test
{
protected:
    CStdioVirtMemAlloc<> valloc;

public:
    void SetUp(void) { valloc.start(); }
    void TearDown(void) { valloc.stop(); }
};

template <typename T> class CWrapFixture: public CAllocFixture
{
protected:
    typename TStdioVirtPtr<T>::type wrapper;

public:
    CWrapFixture(void) : wrapper() { } // UNDONE: we need this for proper construction, problem?
};

#if 0
// From http://stackoverflow.com/a/17236988
inline void print128int(__uint128_t x)
{
    printf("__int128: %016" PRIx64 "%016" PRIx64 "\n",(uint64_t)(x>>64),(uint64_t)x);
}
#endif

#endif // TEST_H
