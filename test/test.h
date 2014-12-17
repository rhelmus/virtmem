#ifndef TEST_H
#define TEST_H

#include "gtest/gtest.h"

class CAllocFixture: public ::testing::Test
{
protected:
    CStdioVirtMemAlloc valloc;

public:
    void SetUp(void) { valloc.start(); }
    void TearDown(void) { valloc.stop(); }
};


#endif // TEST_H
