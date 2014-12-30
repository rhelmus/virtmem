#include "virtmem.h"
#include "stdioalloc.h"
#include "test.h"

#include <vector>


TEST_F(CAllocFixture, SimpleAllocTest)
{
    const TVirtPointer ptr = valloc.alloc(sizeof(int));
    ASSERT_NE(ptr, 0);

    int val = 55;
    valloc.write(ptr, &val, sizeof(val));
    EXPECT_EQ(*(int *)valloc.read(ptr, sizeof(val)), val);
    valloc.flush();
    EXPECT_EQ(*(int *)valloc.read(ptr, sizeof(val)), val);
    valloc.clearPages();
    EXPECT_EQ(*(int *)valloc.read(ptr, sizeof(val)), val);

    valloc.free(ptr);
}

TEST_F(CAllocFixture, ReadOnlyTest)
{
    const TVirtPointer ptr = valloc.alloc(sizeof(int));
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

TEST_F(CAllocFixture, MultiAllocTest)
{
    std::vector<TVirtPointer> ptrlist;
    for (int i=0; i<(int)valloc.getPageCount(); ++i)
    {
        ptrlist.push_back(valloc.alloc(valloc.getPageSize()));
        valloc.write(ptrlist[i], &i, sizeof(int));
        EXPECT_EQ(*(int *)valloc.read(ptrlist[i], sizeof(int)), i);
    }

    for (int i=0; i<(int)valloc.getPageCount(); ++i)
    {
        EXPECT_EQ(*(int *)valloc.read(ptrlist[i], sizeof(int)), i);
    }

    valloc.clearPages();

    for (int i=0; i<(int)valloc.getPageCount(); ++i)
    {
        EXPECT_EQ(*(int *)valloc.read(ptrlist[i], sizeof(int)), i);
    }
}

TEST_F(CAllocFixture, SimplePageTest)
{
    EXPECT_EQ(valloc.getFreePages(), valloc.getPageCount());

    std::vector<TVirtPointer> ptrlist;
    for (int i=0; i<(int)valloc.getPageCount(); ++i)
    {
        ptrlist.push_back(valloc.alloc(valloc.getPageSize()));
        valloc.write(ptrlist[i], &i, sizeof(int));
    }

    EXPECT_EQ(valloc.getFreePages(), 0);

    valloc.flush();

    for (int i=0; i<(int)valloc.getPageCount(); ++i)
    {
        EXPECT_EQ(*(int *)valloc.read(ptrlist[i], sizeof(int)), i);
    }

    valloc.clearPages();
    EXPECT_EQ(valloc.getFreePages(), valloc.getPageCount());

    for (int i=0; i<(int)valloc.getPageCount(); ++i)
    {
        EXPECT_EQ(*(int *)valloc.read(ptrlist[i], sizeof(int)), i);
    }
}

TEST_F(CAllocFixture, PageLockTest)
{
    EXPECT_EQ(valloc.getUnlockedPages(), valloc.getPageCount());

    // Lock all pages
    for (uint8_t p=0; p<valloc.getPageCount(); ++p)
    {
        // 10 is an arbitrary number, just make sure that numbers are unique and don't start at the beginning
        const TVirtPointer ptr = p * 10 + 10;
        valloc.lock(ptr);
        EXPECT_EQ(valloc.getUnlockedPages(), (valloc.getPageCount() - (p+1)));
    }

    EXPECT_EQ(valloc.getUnlockedPages(), 0);

    // Unlock all pages
    for (uint8_t p=0; p<valloc.getPageCount(); ++p)
    {
        // 10 is an arbitrary number, just make sure that numbers are unique and don't start at the beginning
        const TVirtPointer ptr = p * 10 + 10;
        valloc.unlock(ptr);
        EXPECT_EQ(valloc.getUnlockedPages(), (p+1));
    }
}

TEST_F(CAllocFixture, LargeDataTest)
{
    const TVirtPtrSize size = 1024 * 1024 * 8; // 8 mb data block

    const TVirtPointer vbuffer = valloc.alloc(size);
    for (TVirtPtrSize i=0; i<size; ++i)
    {
        char val = size - i;
        valloc.write(vbuffer + i, &val, sizeof(val));
    }

    valloc.clearPages();

    // Linear access check
    for (TVirtPtrSize i=0; i<size; ++i)
    {
        char val = size - i;
        ASSERT_EQ(*(char *)valloc.read(vbuffer + i, sizeof(val)), val);
    }

    valloc.clearPages();

    // Random access check
    for (TVirtPtrSize i=0; i<200; ++i)
    {
        const TVirtPtrSize index = (rand() % size);
        char val = size - index;
        ASSERT_EQ(*(char *)valloc.read(vbuffer + index, sizeof(val)), val);
    }
}

TEST_F(CAllocFixture, LargeRandomDataTest)
{
    const TVirtPtrSize size = 1024 * 1024 * 8; // 8 mb data block

    srand(::testing::UnitTest::GetInstance()->random_seed());
    std::vector<char> buffer;
    buffer.reserve(size);
    for (size_t s=0; s<size; ++s)
        buffer.push_back(rand());

    const TVirtPointer vbuffer = valloc.alloc(size);
    for (TVirtPtrSize i=0; i<size; ++i)
    {
        char val = buffer[i];
        valloc.write(vbuffer + i, &val, sizeof(val));
    }

    valloc.clearPages();

    // Linear access check
    for (TVirtPtrSize i=0; i<size; ++i)
    {
        ASSERT_EQ(*(char *)valloc.read(vbuffer + i, sizeof(char)), buffer[i]);
    }

    valloc.clearPages();

    // Random access check
    for (TVirtPtrSize i=0; i<200; ++i)
    {
        const TVirtPtrSize index = (rand() % size);
        ASSERT_EQ(*(char *)valloc.read(vbuffer + index, sizeof(char)), buffer[index]);
    }
}

