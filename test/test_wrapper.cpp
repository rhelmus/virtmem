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
        return ::testing::AssertionFailure() << "ptr wrapper not null: " << (uint64_t)p.getRawNum();
}

struct STestStruct { int x, y; };
bool operator==(const STestStruct &s1, const STestStruct &s2) { return s1.x == s2.x && s1.y == s2.y; }
bool operator!=(const STestStruct &s1, const STestStruct &s2) { return s1.x != s2.x && s1.y != s2.y; }

std::ostream &operator<<(std::ostream &os, const STestStruct& s)
{
    return os << "(" << s.x << ", " << s.y << ")";
}

class CTestClass
{
public:
    static int constructedClasses;

    CTestClass(void) { ++constructedClasses; }
    ~CTestClass(void) { --constructedClasses; }
};

int CTestClass::constructedClasses = 0;

typedef ::testing::Types<char, int, long, float, double, STestStruct, int *, void *(*)(void)> TWrapTypes;
TYPED_TEST_CASE(CWrapFixture, TWrapTypes);

template <typename T> class CLimitedWrapFixture: public CWrapFixture<T> { };
typedef ::testing::Types<char, int, long, float, double> TLimitedWrapTypes;
TYPED_TEST_CASE(CLimitedWrapFixture, TLimitedWrapTypes);

typedef CWrapFixture<CTestClass> CClassWrapFixture;

TYPED_TEST(CWrapFixture, SimpleWrapTest)
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

TYPED_TEST(CWrapFixture, BaseWrapTest)
{
    this->wrapper = this->wrapper.alloc();

    CVirtPtrBase basewrp = this->wrapper;
    EXPECT_EQ(basewrp, this->wrapper);
    typename TStdioVirtPtr<TypeParam>::type wrp = static_cast<typename TStdioVirtPtr<TypeParam>::type>(basewrp);
    EXPECT_EQ(basewrp, wrp);
    EXPECT_EQ(wrp, this->wrapper);
}

TYPED_TEST(CWrapFixture, CWrapWrapTest)
{
    TypeParam val;
    memset(&val, 10, sizeof(val));
    this->wrapper = this->wrapper.wrap(&val);

    EXPECT_EQ(this->wrapper.unwrap(), &val);
    EXPECT_EQ((TypeParam)*this->wrapper, val);

    this->valloc.clearPages();

    EXPECT_EQ(this->wrapper.unwrap(), &val);
    EXPECT_EQ((TypeParam)*this->wrapper, val);
}

TYPED_TEST(CLimitedWrapFixture, ArithmeticTest)
{
    const int size = 10, start = 10; // Offset from zero to avoid testing to zero initialized values

    this->wrapper = this->wrapper.alloc(size * sizeof(TypeParam));
    typename TStdioVirtPtr<TypeParam>::type wrpp = this->wrapper;
    for (int i=start; i<size+start; ++i)
    {
        *wrpp = i;
        ++wrpp;
    }

    wrpp = this->wrapper;
    this->valloc.clearPages();

    for (int i=start; i<size+start; ++i)
    {
        EXPECT_EQ(*wrpp, i);
        ++wrpp;
    }

    wrpp = this->wrapper;
    EXPECT_EQ(wrpp, this->wrapper);
    wrpp += 2; wrpp += -2;
    EXPECT_EQ(wrpp, this->wrapper);
    EXPECT_EQ(*wrpp, *this->wrapper);

    typename TStdioVirtPtr<TypeParam>::type wrpp2 = this->wrapper;
    EXPECT_EQ(*wrpp, *(wrpp2++));
    EXPECT_EQ(*(++wrpp), *wrpp2);
    EXPECT_EQ(wrpp, wrpp2);
    --wrpp;
    EXPECT_EQ(*wrpp, *this->wrapper);
    EXPECT_NE(wrpp, wrpp2--);
    EXPECT_EQ(wrpp, wrpp2);

    wrpp2 = wrpp + 2;
    EXPECT_GT(wrpp2, wrpp);
    EXPECT_LT(wrpp, wrpp2);
    EXPECT_EQ(wrpp + 2, wrpp2);

    EXPECT_EQ(wrpp2 - 2, wrpp);
    EXPECT_EQ((wrpp2 - wrpp), 2);

    EXPECT_EQ((wrpp += 2), wrpp2);
    EXPECT_EQ((wrpp2 -= 2), this->wrapper);

    wrpp = this->wrapper;
    wrpp2 = wrpp + 1;
    EXPECT_LE(wrpp, this->wrapper);
    EXPECT_LE(wrpp, wrpp2);
    EXPECT_GE(wrpp2, this->wrapper);
    EXPECT_GE(wrpp2, wrpp);
}

TYPED_TEST(CLimitedWrapFixture, WrappedArithmeticTest)
{
    const int size = 10, start = 10;

    TypeParam *buf = new TypeParam[size];
    this->wrapper = this->wrapper.wrap(buf);
    TypeParam *bufp = buf;
    typename TStdioVirtPtr<TypeParam>::type wrpp = this->wrapper;

    for (int i=start; i<size+start; ++i)
    {
        *wrpp = i;
        ++wrpp;
    }

    wrpp = this->wrapper;
    this->valloc.clearPages();

    for (int i=start; i<size+start; ++i)
    {
        EXPECT_EQ(*wrpp, *bufp);
        ++wrpp, ++bufp;
    }

    ++bufp; ++wrpp;
    EXPECT_EQ(bufp, wrpp.unwrap());

    --bufp; --wrpp;
    EXPECT_EQ(bufp, wrpp.unwrap());

    EXPECT_EQ(bufp + 2, (wrpp + 2).unwrap());

    bufp += 2; wrpp += 2;
    EXPECT_EQ(bufp, wrpp.unwrap());
    EXPECT_EQ(bufp - 2, (wrpp - 2).unwrap());

    bufp -= 2; wrpp -= 2;
    EXPECT_EQ(bufp, wrpp.unwrap());

    wrpp += 5;
    EXPECT_LT(bufp, wrpp.unwrap());
    EXPECT_LE(bufp, wrpp.unwrap());
    EXPECT_GT(wrpp.unwrap(), bufp);
    EXPECT_GE(wrpp.unwrap(), bufp);

    bufp += 5;
    EXPECT_EQ((wrpp - this->wrapper), (bufp - buf));

    delete [] buf;
}

TEST_F(CClassWrapFixture, ClassAllocTest)
{
    ASSERT_EQ(CTestClass::constructedClasses, 0);

    TStdioVirtPtr<CTestClass>::type cptr = cptr.newClass();
    EXPECT_EQ(CTestClass::constructedClasses, 1);
    cptr.deleteClass(cptr);
    EXPECT_EQ(CTestClass::constructedClasses, 0);
}

TEST_F(CClassWrapFixture, ClassArrayAllocTest)
{
    const int elements = 10;

    TStdioVirtPtr<CTestClass>::type cptr = cptr.newArray(elements);
    EXPECT_EQ(CTestClass::constructedClasses, elements);
    cptr.deleteArray(cptr);
    EXPECT_EQ(CTestClass::constructedClasses, 0);
}
