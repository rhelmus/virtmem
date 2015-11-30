#include "virtmem.h"
#include "stdio_alloc.h"
#include "test.h"

#include <vector>


TEST_F(VAllocFixture, SimpleAllocTest)
{
    const VPtrNum ptr = valloc.allocRaw(sizeof(int));
    ASSERT_NE(ptr, 0);

    int val = 55;
    valloc.write(ptr, &val, sizeof(val));
    EXPECT_EQ(*(int *)valloc.read(ptr, sizeof(val)), val);
    valloc.flush();
    EXPECT_EQ(*(int *)valloc.read(ptr, sizeof(val)), val);
    valloc.clearPages();
    EXPECT_EQ(*(int *)valloc.read(ptr, sizeof(val)), val);

    valloc.freeRaw(ptr);
}

TEST_F(VAllocFixture, ReadOnlyTest)
{
    const VPtrNum ptr = valloc.allocRaw(sizeof(int));
    int val = 55;
    valloc.write(ptr, &val, sizeof(val));
    valloc.flush();

    // Read only read, shouldn't modify actual memory
    int *pval = (int *)valloc.read(ptr, sizeof(val));
    *pval = 66;
    valloc.flush();
    EXPECT_EQ(*(int *)valloc.read(ptr, sizeof(val)), 66);
    valloc.clearPages();
    EXPECT_EQ(*(int *)valloc.read(ptr, sizeof(val)), val);
}

TEST_F(VAllocFixture, MultiAllocTest)
{
    std::vector<VPtrNum> ptrlist;
    for (int i=0; i<(int)valloc.getBigPageCount(); ++i)
    {
        ptrlist.push_back(valloc.allocRaw(valloc.getBigPageSize()));
        valloc.write(ptrlist[i], &i, sizeof(int));
        EXPECT_EQ(*(int *)valloc.read(ptrlist[i], sizeof(int)), i);
    }

    for (int i=0; i<(int)valloc.getBigPageCount(); ++i)
    {
        EXPECT_EQ(*(int *)valloc.read(ptrlist[i], sizeof(int)), i);
    }

    valloc.clearPages();

    for (int i=0; i<(int)valloc.getBigPageCount(); ++i)
    {
        EXPECT_EQ(*(int *)valloc.read(ptrlist[i], sizeof(int)), i);
    }
}

TEST_F(VAllocFixture, SimplePageTest)
{
    EXPECT_EQ(valloc.getFreeBigPages(), valloc.getBigPageCount());

    std::vector<VPtrNum> ptrlist;
    for (int i=0; i<(int)valloc.getBigPageCount(); ++i)
    {
        ptrlist.push_back(valloc.allocRaw(valloc.getBigPageSize()));
        valloc.write(ptrlist[i], &i, sizeof(int));
    }

    EXPECT_EQ(valloc.getFreeBigPages(), 0);

    valloc.flush();

    for (int i=0; i<(int)valloc.getBigPageCount(); ++i)
    {
        EXPECT_EQ(*(int *)valloc.read(ptrlist[i], sizeof(int)), i);
    }

    valloc.clearPages();
    EXPECT_EQ(valloc.getFreeBigPages(), valloc.getBigPageCount());

    for (int i=0; i<(int)valloc.getBigPageCount(); ++i)
    {
        EXPECT_EQ(*(int *)valloc.read(ptrlist[i], sizeof(int)), i);
    }
}

TEST_F(VAllocFixture, PageLockTest)
{
    EXPECT_EQ(valloc.getUnlockedSmallPages(), valloc.getSmallPageCount());
    EXPECT_EQ(valloc.getUnlockedMediumPages(), valloc.getMediumPageCount());
    EXPECT_EQ(valloc.getUnlockedBigPages(), valloc.getBigPageCount());

    // 10 is an arbitrary number, just make sure that numbers are unique, don't start at the beginning
    // and don't overlap
    auto genptr = [this](uint8_t p) { return p * valloc.getBigPageSize() + 10; };

    // Lock all big pages
    for (uint8_t p=0; p<valloc.getBigPageCount(); ++p)
    {
        valloc.makeDataLock(genptr(p), valloc.getBigPageSize());
        EXPECT_EQ(valloc.getUnlockedBigPages(), (valloc.getBigPageCount() - (p+1)));
    }

    EXPECT_EQ(valloc.getUnlockedBigPages(), 0);

    // lock smaller pages, shouldn't depend on big pages (anymore)
    valloc.makeDataLock(genptr(valloc.getBigPageCount() + 1), valloc.getSmallPageSize());
    EXPECT_EQ(valloc.getUnlockedSmallPages(), (valloc.getSmallPageCount()-1));
    valloc.makeDataLock(genptr(valloc.getBigPageCount() + 2), valloc.getMediumPageSize());
    EXPECT_EQ(valloc.getUnlockedMediumPages(), (valloc.getMediumPageCount()-1));


    // Unlock all big pages
    for (uint8_t p=0; p<valloc.getBigPageCount(); ++p)
    {
        valloc.releaseLock(genptr(p));
        EXPECT_EQ(valloc.getUnlockedBigPages(), (p+1));
    }

    valloc.releaseLock(genptr(valloc.getBigPageCount() + 1));
    EXPECT_EQ(valloc.getUnlockedSmallPages(), valloc.getSmallPageCount());
    valloc.releaseLock(genptr(valloc.getBigPageCount() + 2));
    EXPECT_EQ(valloc.getUnlockedMediumPages(), valloc.getMediumPageCount());
}

TEST_F(VAllocFixture, LargeDataTest)
{
    const VPtrSize size = 1024 * 1024 * 8; // 8 mb data block

    const VPtrNum vbuffer = valloc.allocRaw(size);
    for (VPtrSize i=0; i<size; ++i)
    {
        char val = size - i;
        valloc.write(vbuffer + i, &val, sizeof(val));
    }

    valloc.clearPages();

    // Linear access check
    for (VPtrSize i=0; i<size; ++i)
    {
        char val = size - i;
        ASSERT_EQ(*(char *)valloc.read(vbuffer + i, sizeof(val)), val);
    }

    valloc.clearPages();

    // Random access check
    for (VPtrSize i=0; i<200; ++i)
    {
        const VPtrSize index = (rand() % size);
        char val = size - index;
        ASSERT_EQ(*(char *)valloc.read(vbuffer + index, sizeof(val)), val);
    }
}

TEST_F(VAllocFixture, LargeRandomDataTest)
{
    const VPtrSize size = 1024 * 1024 * 8; // 8 mb data block

    srand(::testing::UnitTest::GetInstance()->random_seed());
    std::vector<char> buffer;
    buffer.reserve(size);
    for (size_t s=0; s<size; ++s)
        buffer.push_back(rand());

    const VPtrNum vbuffer = valloc.allocRaw(size);
    for (VPtrSize i=0; i<size; ++i)
    {
        char val = buffer[i];
        valloc.write(vbuffer + i, &val, sizeof(val));
    }

    valloc.clearPages();

    // Linear access check
    for (VPtrSize i=0; i<size; ++i)
    {
        ASSERT_EQ(*(char *)valloc.read(vbuffer + i, sizeof(char)), buffer[i]);
    }

    valloc.clearPages();

    // Random access check
    for (VPtrSize i=0; i<200; ++i)
    {
        const VPtrSize index = (rand() % size);
        ASSERT_EQ(*(char *)valloc.read(vbuffer + index, sizeof(char)), buffer[index]);
    }
}

