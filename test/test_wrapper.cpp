#include "virtmem.h"
#include "stdioalloc.h"
#include "test.h"

#include <iostream>

// helper function, can't use assertions directly
template <typename T, typename A>::testing::AssertionResult wrapperIsNull(const CVirtPtr<T, A> &p)
{
    if (p == 0)
        return ::testing::AssertionSuccess();
    else
        return ::testing::AssertionFailure() << " ptr wrapper not null";
}

template <typename T> class CWrapFixture: public CAllocFixture
{
protected:
    typename TStdioVirtPtr<T>::type wrapper;
};

struct STestStruct { int x, y; };
bool operator==(const STestStruct &s1, const STestStruct &s2) { return s1.x == s2.x && s1.y == s2.y; }
bool operator!=(const STestStruct &s1, const STestStruct &s2) { return s1.x != s2.x && s1.y != s2.y; }

std::ostream &operator<<(std::ostream &os, const STestStruct& s)
{
    return os << "(" << s.x << ", " << s.y << ")";
}

typedef ::testing::Types<char, int, unsigned int, STestStruct> TWrapTypes;
TYPED_TEST_CASE(CWrapFixture, TWrapTypes);

TYPED_TEST(CWrapFixture, TrivialWrapTest)
{
    EXPECT_TRUE(wrapperIsNull(this->wrapper));
    EXPECT_EQ(this->wrapper, NILL);

    this->wrapper = this->wrapper.alloc();
    EXPECT_FALSE(wrapperIsNull(this->wrapper));
    ASSERT_NE(this->wrapper, NILL);

    TypeParam val;
    memset(&val, 10, sizeof(val));
    *this->wrapper = val;
    EXPECT_EQ((TypeParam)*this->wrapper, val);
    this->valloc.clearPages();
    EXPECT_EQ((TypeParam)*this->wrapper, val);

    this->wrapper.free(this->wrapper);
    EXPECT_TRUE(wrapperIsNull(this->wrapper));
    EXPECT_EQ(this->wrapper, NILL);
}

// Wrapped C pointers

