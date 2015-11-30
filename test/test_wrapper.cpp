#include "virtmem.h"
#include "static_alloc.h"
#include "stdio_alloc.h"
#include "test.h"

#include <iostream>

#include <stddef.h>

using namespace virtmem;

// helper function, can't use assertions directly
template <typename T, typename A>::testing::AssertionResult wrapperIsNull(const VPtr<T, A> &p)
{
    if (p == 0)
        return ::testing::AssertionSuccess();
    else
        return ::testing::AssertionFailure() << "ptr wrapper not null: " << (uint64_t)p.getRawNum();
}

struct TestStruct
{
    int x, y;

    // For membr ptr tests
    struct SubStruct { int x, y; } sub;
    CharVirtPtr vbuf;
    char buf[10]; // NOTE: this has to be last for MembrAssignTest
};

bool operator==(const TestStruct &s1, const TestStruct &s2) { return s1.x == s2.x && s1.y == s2.y; }
bool operator!=(const TestStruct &s1, const TestStruct &s2) { return s1.x != s2.x && s1.y != s2.y; }

std::ostream &operator<<(std::ostream &os, const TestStruct& s)
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

//typedef ::testing::Types<char, int, long, float, double, TestStruct, int *, void *(*)(void)> WrapTypes;
typedef ::testing::Types<int> WrapTypes;
TYPED_TEST_CASE(VPtrFixture, WrapTypes);

template <typename T> class LimitedWrapFixture: public VPtrFixture<T> { };
typedef ::testing::Types<char, int, long, float, double> LimitedWrapTypes;
TYPED_TEST_CASE(LimitedWrapFixture, LimitedWrapTypes);

typedef VPtrFixture<int> IntWrapFixture;
typedef VPtrFixture<CTestClass> ClassWrapFixture;
typedef VPtrFixture<TestStruct> StructWrapFixture;

TYPED_TEST(VPtrFixture, SimpleWrapTest)
{
    EXPECT_TRUE(wrapperIsNull(this->vptr));
    EXPECT_EQ(this->vptr, NILL);

    this->vptr = VAllocFixture::valloc.alloc<TypeParam>();
    EXPECT_FALSE(wrapperIsNull(this->vptr));
    ASSERT_NE(this->vptr, NILL);

    TypeParam val;
    memset(&val, 10, sizeof(val));
    *this->vptr = val;
    EXPECT_EQ((TypeParam)*this->vptr, val);
    this->valloc.clearPages();
    EXPECT_EQ((TypeParam)*this->vptr, val);

    typename TStdioVirtPtr<TypeParam>::type wrp2 = VAllocFixture::valloc.alloc<TypeParam>();
    *wrp2 = *this->vptr;
    EXPECT_EQ((TypeParam)*this->vptr, (TypeParam)*wrp2);

    VAllocFixture::valloc.free(this->vptr);
    EXPECT_TRUE(wrapperIsNull(this->vptr));
    EXPECT_EQ(this->vptr, NILL);
}

TYPED_TEST(VPtrFixture, ConstWrapTest)
{
    this->vptr = VAllocFixture::valloc.alloc<TypeParam>();
    TypeParam val;
    memset(&val, 10, sizeof(val));
    *this->vptr = val;

    typename TStdioVirtPtr<TypeParam>::type wrp2 = VAllocFixture::valloc.alloc<TypeParam>();
    *wrp2 = *this->vptr;
    typename TStdioVirtPtr<const TypeParam>::type cwrp2 = wrp2;
    EXPECT_EQ((TypeParam)*wrp2, (TypeParam)*cwrp2);
    EXPECT_EQ((TypeParam)*cwrp2, (TypeParam)*wrp2);

    memset(&val, 20, sizeof(val));
    *this->vptr = val;
    // fails if assignment yielded shallow copy
    EXPECT_NE((TypeParam)*this->vptr, (TypeParam)*cwrp2) << "ptrs: " << (uint64_t)this->vptr.getRawNum() << "/" << (uint64_t)cwrp2.getRawNum();

    *this->vptr = *cwrp2;
    EXPECT_EQ((TypeParam)*this->vptr, (TypeParam)*cwrp2);
}

TYPED_TEST(VPtrFixture, BaseWrapTest)
{
    this->vptr = VAllocFixture::valloc.alloc<TypeParam>();

    BaseVPtr basewrp = this->vptr;
    EXPECT_EQ(basewrp, this->vptr);
    typename TStdioVirtPtr<TypeParam>::type wrp = static_cast<typename TStdioVirtPtr<TypeParam>::type>(basewrp);
    EXPECT_EQ(basewrp, wrp);
    EXPECT_EQ(wrp, this->vptr);
}

TYPED_TEST(VPtrFixture, WrapWrapTest)
{
    TypeParam val;
    memset(&val, 10, sizeof(val));
    this->vptr = this->vptr.wrap(&val);

    EXPECT_EQ(this->vptr.unwrap(), &val);
    EXPECT_EQ((TypeParam)*this->vptr, val);

    this->valloc.clearPages();

    EXPECT_EQ(this->vptr.unwrap(), &val);
    EXPECT_EQ((TypeParam)*this->vptr, val);
}

TYPED_TEST(LimitedWrapFixture, ArithmeticTest)
{
    const int size = 10, start = 10; // Offset from zero to avoid testing to zero initialized values

    this->vptr = VAllocFixture::valloc.alloc<TypeParam>(size * sizeof(TypeParam));
    typename TStdioVirtPtr<TypeParam>::type wrpp = this->vptr;
    for (int i=start; i<size+start; ++i)
    {
        *wrpp = i;
        ++wrpp;
    }

    wrpp = this->vptr;
    this->valloc.clearPages();

    for (int i=start; i<size+start; ++i)
    {
        EXPECT_EQ(*wrpp, i);
        ++wrpp;
    }

    wrpp = this->vptr;
    EXPECT_EQ(wrpp, this->vptr);
    wrpp += 2; wrpp += -2;
    EXPECT_EQ(wrpp, this->vptr);
    EXPECT_EQ(*wrpp, *this->vptr);

    typename TStdioVirtPtr<TypeParam>::type wrpp2 = this->vptr;
    EXPECT_EQ(*wrpp, *(wrpp2++));
    EXPECT_EQ(*(++wrpp), *wrpp2);
    EXPECT_EQ(wrpp, wrpp2);
    --wrpp;
    EXPECT_EQ(*wrpp, *this->vptr);
    EXPECT_NE(wrpp, wrpp2--);
    EXPECT_EQ(wrpp, wrpp2);

    wrpp2 = wrpp + 2;
    EXPECT_GT(wrpp2, wrpp);
    EXPECT_LT(wrpp, wrpp2);
    EXPECT_EQ(wrpp + 2, wrpp2);

    EXPECT_EQ(wrpp2 - 2, wrpp);
    EXPECT_EQ((wrpp2 - wrpp), 2);

    EXPECT_EQ((wrpp += 2), wrpp2);
    EXPECT_EQ((wrpp2 -= 2), this->vptr);

    wrpp = this->vptr;
    wrpp2 = wrpp + 1;
    EXPECT_LE(wrpp, this->vptr);
    EXPECT_LE(wrpp, wrpp2);
    EXPECT_GE(wrpp2, this->vptr);
    EXPECT_GE(wrpp2, wrpp);
}

TYPED_TEST(LimitedWrapFixture, WrappedArithmeticTest)
{
    const int size = 10, start = 10;

    TypeParam *buf = new TypeParam[size];
    this->vptr = this->vptr.wrap(buf);
    TypeParam *bufp = buf;
    typename TStdioVirtPtr<TypeParam>::type wrpp = this->vptr;

    for (int i=start; i<size+start; ++i)
    {
        *wrpp = i;
        ++wrpp;
    }

    wrpp = this->vptr;
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
    EXPECT_EQ((wrpp - this->vptr), (bufp - buf));

    delete [] buf;
}

TEST_F(IntWrapFixture, AlignmentTest)
{
    const int bufsize = 17;

    this->vptr = valloc.alloc<int>();
    CharVirtPtr buf = valloc.alloc<char>(bufsize);
    CharVirtPtr unalignedbuf = &buf[1];
    valloc.clearPages();
    volatile char c = *unalignedbuf; // force load of unaligned address
    ASSERT_EQ(reinterpret_cast<intptr_t>(valloc.read(this->vptr.getRawNum(), sizeof(int))) & (sizeof(int)-1), 0);

    // check if int is still aligned after locking a big page
    valloc.clearPages();
    valloc.makeDataLock(unalignedbuf.getRawNum(), valloc.getBigPageSize(), true);
    valloc.releaseLock(unalignedbuf.getRawNum());

    ASSERT_EQ(reinterpret_cast<intptr_t>(valloc.read(this->vptr.getRawNum(), sizeof(int))) & (sizeof(int)-1), 0);
}

TEST_F(ClassWrapFixture, ClassAllocTest)
{
    ASSERT_EQ(CTestClass::constructedClasses, 0);

    TStdioVirtPtr<CTestClass>::type cptr = valloc.newClass<CTestClass>();
    EXPECT_EQ(CTestClass::constructedClasses, 1);
    valloc.deleteClass(cptr);
    EXPECT_EQ(CTestClass::constructedClasses, 0);
}

TEST_F(ClassWrapFixture, ClassArrayAllocTest)
{
    const int elements = 10;

    TStdioVirtPtr<CTestClass>::type cptr = valloc.newArray<CTestClass>(elements);
    EXPECT_EQ(CTestClass::constructedClasses, elements);
    valloc.deleteArray(cptr);
    EXPECT_EQ(CTestClass::constructedClasses, 0);
}

// Tests for VPtr::ValueWrapper
TEST_F(IntWrapFixture, OperatorTest)
{
    const int start = 100;

    TStdioVirtPtr<int>::type vptr = valloc.alloc<int>();
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

TEST_F(IntWrapFixture, MultiAllocTest)
{
    // Second allocator
    typedef StaticVAlloc<1024*1024> Alloc2;
    Alloc2 valloc2;
    valloc2.start();

    this->vptr = valloc.alloc<int>();
    VPtr<int, Alloc2> vptr2 = valloc2.alloc<int>();

    *this->vptr = 55;
    *vptr2 = (int)*this->vptr;
    EXPECT_EQ(*this->vptr, *vptr2);
    valloc.clearPages();
    valloc2.clearPages();
    EXPECT_EQ(*this->vptr, *vptr2);

    valloc2.stop();
}

TEST_F(StructWrapFixture, MembrAssignTest)
{
    this->vptr = valloc.alloc<TestStruct>();
    this->vptr->x = 55;
    EXPECT_EQ(this->vptr->x, 55);

    // test if usage of other struct fields messes up assignment
    this->vptr->y = this->vptr->x * this->vptr->x;
    EXPECT_EQ(this->vptr->y, (55*55));

    // Test partial data structures (useful for unions or partially allocated structs)
    // The struct is partially allocated, without the char buffer, while some extra space is requested right after for a virt char buffer
    const size_t bufsize = sizeof((TestStruct *)0)->buf;
    const size_t vbufsize = sizeof((TestStruct *)0)->vbuf;
    const int vbufelements = 64;

    valloc.free(this->vptr);
    this->vptr = valloc.alloc<TestStruct>(sizeof(TestStruct) - bufsize + vbufsize + vbufelements);
    this->vptr->x = 55; this->vptr->y = 66;
    this->vptr->vbuf = (CharVirtPtr)getMembrPtr(this->vptr, &TestStruct::vbuf) + vbufsize; // point to end of struct

    const char *str = "Does this somewhat boring sentence compares well?";
    strcpy(this->vptr->vbuf, str);

    // struct is still ok?
    EXPECT_EQ(this->vptr->x, 55);
    EXPECT_EQ(this->vptr->y, 66);
    // vbuf ok?
    EXPECT_EQ(strcmp(this->vptr->vbuf, str), 0);

    // UNDONE: test data in partial struct with larger datatype than char
}

TEST_F(StructWrapFixture, MembrDiffTest)
{
    this->vptr = valloc.alloc<TestStruct>();

    const size_t offset_x = offsetof(TestStruct, x);
    const size_t offset_y = offsetof(TestStruct, y);
    const size_t offset_buf = offsetof(TestStruct, buf);
    const size_t offset_vbuf = offsetof(TestStruct, vbuf);

    const UCharVirtPtr vptr_x = static_cast<UCharVirtPtr>(getMembrPtr(this->vptr, &TestStruct::x));
    const UCharVirtPtr vptr_y = static_cast<UCharVirtPtr>(getMembrPtr(this->vptr, &TestStruct::y));
    const UCharVirtPtr vptr_buf = static_cast<UCharVirtPtr>(getMembrPtr(this->vptr, &TestStruct::buf));
    const UCharVirtPtr vptr_vbuf = static_cast<UCharVirtPtr>(getMembrPtr(this->vptr, &TestStruct::vbuf));

    const UCharVirtPtr base = static_cast<UCharVirtPtr>(this->vptr);
    EXPECT_EQ(vptr_x - base, offset_x);
    EXPECT_EQ(vptr_y - base, offset_y);
    EXPECT_EQ(vptr_buf - base, offset_buf);
    EXPECT_EQ(vptr_vbuf - base, offset_vbuf);

    // Sub structure
    const size_t offset_sub = offsetof(TestStruct, sub);
    const size_t offset_sub_x = offset_sub + offsetof(TestStruct::SubStruct, x);
    const size_t offset_sub_y = offset_sub + offsetof(TestStruct::SubStruct, y);

    const UCharVirtPtr vptr_sub_x = static_cast<UCharVirtPtr>(getMembrPtr(this->vptr, &TestStruct::sub, &TestStruct::SubStruct::x));
    const UCharVirtPtr vptr_sub_y = static_cast<UCharVirtPtr>(getMembrPtr(this->vptr, &TestStruct::sub, &TestStruct::SubStruct::y));
    EXPECT_EQ(vptr_sub_x - base, offset_sub_x);
    EXPECT_EQ(vptr_sub_y - base, offset_sub_y);
}
