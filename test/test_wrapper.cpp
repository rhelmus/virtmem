#include "virtmem.h"
#include "stdio_alloc.h"
#include "test.h"

#include <iostream>

#include <stddef.h>

using namespace virtmem;

// helper function, can't use assertions directly
template <typename T, typename A>::testing::AssertionResult wrapperIsNull(const CVirtPtr<T, A> &p)
{
    if (p == 0)
        return ::testing::AssertionSuccess();
    else
        return ::testing::AssertionFailure() << "ptr wrapper not null: " << (uint64_t)p.getRawNum();
}

struct STestStruct
{
    int x, y;

    // For membr ptr tests
    struct SSubStruct { int x, y; } sub;
    TCharVirtPtr vbuf;
    char buf[10]; // NOTE: this has to be last for MembrAssignTest
};

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

typedef CWrapFixture<int> CIntWrapFixture;
typedef CWrapFixture<CTestClass> CClassWrapFixture;
typedef CWrapFixture<STestStruct> CStructWrapFixture;

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

    typename TStdioVirtPtr<TypeParam>::type wrp2 = wrp2.alloc();
    *wrp2 = *this->wrapper;
    EXPECT_EQ((TypeParam)*this->wrapper, (TypeParam)*wrp2);

    this->wrapper.free(this->wrapper);
    EXPECT_TRUE(wrapperIsNull(this->wrapper));
    EXPECT_EQ(this->wrapper, NILL);
}

TYPED_TEST(CWrapFixture, ConstWrapTest)
{
    this->wrapper = this->wrapper.alloc();
    TypeParam val;
    memset(&val, 10, sizeof(val));
    *this->wrapper = val;

    typename TStdioVirtPtr<TypeParam>::type wrp2 = wrp2.alloc();
    *wrp2 = *this->wrapper;
    typename TStdioVirtPtr<const TypeParam>::type cwrp2 = wrp2;
    EXPECT_EQ((TypeParam)*wrp2, (TypeParam)*cwrp2);
    EXPECT_EQ((TypeParam)*cwrp2, (TypeParam)*wrp2);

    memset(&val, 20, sizeof(val));
    *this->wrapper = val;
    // fails if assignment yielded shallow copy
    EXPECT_NE((TypeParam)*this->wrapper, (TypeParam)*cwrp2) << "ptrs: " << (uint64_t)this->wrapper.getRawNum() << "/" << (uint64_t)cwrp2.getRawNum();

    *this->wrapper = *cwrp2;
    EXPECT_EQ((TypeParam)*this->wrapper, (TypeParam)*cwrp2);
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

TEST_F(CIntWrapFixture, AlignmentTest)
{
    const int bufsize = 17;

    this->wrapper = this->wrapper.alloc();
    TCharVirtPtr buf = buf.alloc(bufsize);
    TCharVirtPtr unalignedbuf = &buf[1];
    valloc.clearPages();
    volatile char c = *unalignedbuf; // force load of unaligned address
    ASSERT_EQ(reinterpret_cast<intptr_t>(valloc.read(this->wrapper.getRawNum(), sizeof(int))) & (sizeof(int)-1), 0);

    // check if int is still aligned after locking a big page
    valloc.clearPages();
    valloc.makeLock(unalignedbuf.getRawNum(), valloc.getBigPageSize(), true);
    valloc.releaseLock(unalignedbuf.getRawNum());

    ASSERT_EQ(reinterpret_cast<intptr_t>(valloc.read(this->wrapper.getRawNum(), sizeof(int))) & (sizeof(int)-1), 0);
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

// Tests for CVirtPtr::CValueWrapper
TEST_F(CIntWrapFixture, OperatorTest)
{
    const int start = 100;

    TStdioVirtPtr<int>::type vptr = vptr.alloc();
    int i = start;

    EXPECT_EQ(*vptr = start, start);
    EXPECT_EQ(*vptr + 1, i + 1);
    EXPECT_EQ(*vptr - 1, i - 1);
    EXPECT_EQ(*vptr * 2, i * 2);
    EXPECT_EQ(*vptr / 2, i / 2);

    EXPECT_EQ(i, *vptr);
    EXPECT_EQ(i + 1, *vptr + 1);
    EXPECT_EQ(i - 1, *vptr - 1);
    EXPECT_EQ(i * 2, *vptr * 2);
    EXPECT_EQ(i / 2, *vptr / 2);

    i += 10;
    *vptr += 10;
    EXPECT_EQ(*vptr, i);

    i -= 10;
    *vptr -= 10;
    EXPECT_EQ(*vptr, i);

    ++i;
    EXPECT_NE(*vptr, i);
    EXPECT_NE(i, *vptr);
    EXPECT_LT(*vptr, i);
    EXPECT_LE(*vptr, i);
    EXPECT_GT(i, *vptr);
    EXPECT_GE(i, *vptr);

    *vptr += 10;
    EXPECT_EQ(*vptr, i + 9);
    EXPECT_LT(i, *vptr);
    EXPECT_LE(i, *vptr);
    EXPECT_GT(*vptr, i);
    EXPECT_GE(*vptr, i);

    *vptr -= 9;
    EXPECT_EQ(*vptr, i);

    i *= 10;
    *vptr *= 10;
    EXPECT_EQ(*vptr, i);

    i /= 10;
    *vptr /= 10;
    EXPECT_EQ(*vptr, i);

    i = *vptr = start;
    EXPECT_EQ(!*vptr, !i);
    EXPECT_EQ(~*vptr, ~i);
    EXPECT_EQ(*vptr | 10, i | 10);
    EXPECT_EQ(*vptr & 10, i & 10);
    EXPECT_EQ(*vptr ^ 10, i ^ 10);
    EXPECT_EQ((int)*vptr << 10, i << 10); // this cast is necessary for google test (ambiguous otherwise)
    EXPECT_EQ(*vptr >> 10, i >> 10);
}

TEST_F(CIntWrapFixture, MultiAllocTest)
{
    // Second allocator. NOTE: the type is slightly different (different poolsize), so should give no singleton problems
    typedef CStdioVirtMemAlloc<STestAllocProperties> TAlloc2;
    TAlloc2 valloc2;
    valloc2.start();

    this->wrapper = this->wrapper.alloc();
    CVirtPtr<int, TAlloc2> vptr2 = vptr2.alloc();

    *this->wrapper = 55;
    *vptr2 = (int)*this->wrapper;
    EXPECT_EQ(*this->wrapper, *vptr2);
    valloc.clearPages();
    valloc2.clearPages();
    EXPECT_EQ(*this->wrapper, *vptr2);

    valloc2.stop();
}

TEST_F(CStructWrapFixture, MembrAssignTest)
{
    this->wrapper = this->wrapper.alloc();
    this->wrapper->x = 55;
    EXPECT_EQ(this->wrapper->x, 55);

    // test if usage of other struct fields messes up assignment
    this->wrapper->y = this->wrapper->x * this->wrapper->x;
    EXPECT_EQ(this->wrapper->y, (55*55));

    // Test partial data structures (useful for unions or partially allocated structs)
    // The struct is partially allocated, without the char buffer, while some extra space is requested right after for a virt char buffer
    const size_t bufsize = sizeof((STestStruct *)0)->buf;
    const size_t vbufsize = sizeof((STestStruct *)0)->vbuf;
    const int vbufelements = 64;

    this->wrapper.free(this->wrapper);
    this->wrapper = this->wrapper.alloc(sizeof(STestStruct) - bufsize + vbufsize + vbufelements);
    this->wrapper->x = 55; this->wrapper->y = 66;
    this->wrapper->vbuf = (TCharVirtPtr)getMembrPtr(this->wrapper, &STestStruct::vbuf) + vbufsize; // point to end of struct

    const char *str = "Does this somewhat boring sentence compares well?";
    strcpy(this->wrapper->vbuf, str);

    // struct is still ok?
    EXPECT_EQ(this->wrapper->x, 55);
    EXPECT_EQ(this->wrapper->y, 66);
    // vbuf ok?
    EXPECT_EQ(strcmp(this->wrapper->vbuf, str), 0);

    // UNDONE: test data in partial struct with larger datatype than char
}

TEST_F(CStructWrapFixture, MembrDiffTest)
{
    this->wrapper = this->wrapper.alloc();

    const size_t offset_x = offsetof(STestStruct, x);
    const size_t offset_y = offsetof(STestStruct, y);
    const size_t offset_buf = offsetof(STestStruct, buf);
    const size_t offset_vbuf = offsetof(STestStruct, vbuf);

    const TUCharVirtPtr vptr_x = static_cast<TUCharVirtPtr>(getMembrPtr(this->wrapper, &STestStruct::x));
    const TUCharVirtPtr vptr_y = static_cast<TUCharVirtPtr>(getMembrPtr(this->wrapper, &STestStruct::y));
    const TUCharVirtPtr vptr_buf = static_cast<TUCharVirtPtr>(getMembrPtr(this->wrapper, &STestStruct::buf));
    const TUCharVirtPtr vptr_vbuf = static_cast<TUCharVirtPtr>(getMembrPtr(this->wrapper, &STestStruct::vbuf));

    const TUCharVirtPtr base = static_cast<TUCharVirtPtr>(this->wrapper);
    EXPECT_EQ(vptr_x - base, offset_x);
    EXPECT_EQ(vptr_y - base, offset_y);
    EXPECT_EQ(vptr_buf - base, offset_buf);
    EXPECT_EQ(vptr_vbuf - base, offset_vbuf);

    // Sub structure
    const size_t offset_sub = offsetof(STestStruct, sub);
    const size_t offset_sub_x = offset_sub + offsetof(STestStruct::SSubStruct, x);
    const size_t offset_sub_y = offset_sub + offsetof(STestStruct::SSubStruct, y);

    const TUCharVirtPtr vptr_sub_x = static_cast<TUCharVirtPtr>(getMembrPtr(this->wrapper, &STestStruct::sub, &STestStruct::SSubStruct::x));
    const TUCharVirtPtr vptr_sub_y = static_cast<TUCharVirtPtr>(getMembrPtr(this->wrapper, &STestStruct::sub, &STestStruct::SSubStruct::y));
    EXPECT_EQ(vptr_sub_x - base, offset_sub_x);
    EXPECT_EQ(vptr_sub_y - base, offset_sub_y);
}
