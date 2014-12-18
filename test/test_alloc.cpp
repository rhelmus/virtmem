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
    int *pval = (int *)valloc.read(ptr, sizeof(val), true);
    *pval = 66;
    valloc.flush();
    EXPECT_EQ(*(int *)valloc.read(ptr, sizeof(val)), 66);
    valloc.clearPages();
    EXPECT_EQ(*(int *)valloc.read(ptr, sizeof(val)), val);

    // "Writable read", should sync page
    pval = (int *)valloc.read(ptr, sizeof(val), false);
    *pval = 66;
    valloc.clearPages();
    EXPECT_EQ(*(int *)valloc.read(ptr, sizeof(val)), 66);
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

TEST_F(CAllocFixture, PageLockTests)
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

// Big file and/or other large data

#if 0
struct STest { int x, y; };

int main()
{
    CStdioVirtMemAlloc valloc;
    valloc.start();

    //CVirtPtr<int, CStdioVirtMemAlloc> iptr = iptr.alloc();
    TStdioVirtPtr<int>::type iptr = iptr.alloc();
    *iptr = 55;
    std::cout << "iptr: " << (int)*iptr << std::endl;
    iptr.free(iptr);

    TStdioVirtPtr<char>::type buf = buf.alloc(128);
    memset(*makePtrWrapLock(buf), 'h', 128);
    valloc.clearPages();

    std::cout << "c: " << (char)buf[127] << std::endl;

    TStdioVirtPtr<STest>::type test = test.alloc();
    valloc.clearPages();
    test->x = 55;
    test->y = 33;
    valloc.clearPages();
    std::cout << "test: " << test->x << ", " << test->y << std::endl;

    const TStdioVirtPtr<STest>::type ctest = test;
    std::cout << "ctest: " << ctest->x << ", " << ctest->y << std::endl;

    STest sttest = { 22, 11 };
    const TStdioVirtPtr<STest>::type ctest2 = ctest2.wrap(&sttest);
    std::cout << "ctest2: " << ctest2->x << ", " << ctest2->y << std::endl;
    assert(ctest2.unwrap() == &sttest);

    TStdioVirtPtr<STest>::type test2 = test2.alloc();
    memcpy(test2, test, sizeof(STest));
    std::cout << "test2: " << test2->x << ", " << test2->y << std::endl;

    const char *teststr = "Howdy!";
    TStdioVirtPtr<char>::type teststrw = teststrw.alloc(strlen(teststr) + 1);
    strncpy(teststrw, teststr, 3);
    teststrw[3] = 0;
    std::cout << "teststrw: " << (char)teststrw[0] << (char)teststrw[1] << (char)teststrw[2] << std::endl;
    std::cout << "len: " << strlen(teststrw) << std::endl;
    assert(strncmp(teststrw, teststr, 3) == 0);

#if 0
#if 0
    int val = 33, val2 = 42;

    diskAlloc.write(0, &val);
    diskAlloc.write(1000, &val2);

    val = *diskAlloc.read<int>(0);
    val2 = *diskAlloc.read<int>(1000);
    std::cout << "val = " << val << "\nval2 = " << val2 << std::endl;
#endif

#if 0
    CDiskPointerWrapper<int> pval1 = diskAlloc.alloc(sizeof(int) * 1000);
    CDiskPointerWrapper<int> pval2 = diskAlloc.alloc(sizeof(int));

    std::cout << "-------- begin write...----------\n";
    *pval1 = 22;
    assert(*pval1 == 22);
    std::cout << "-------- end write...---------\n";
    *pval2 = 33;
    assert(*pval2 == 33);

    std::cout << "vals: " << *pval1 << "/" << *pval2 << std::endl;
    std::cout << "pvals: " << pval1 << "/" << pval2 << std::endl;

#else

    int val = 33;
    CDiskPointerWrapper<int> pval = virtMemAllocator.alloc(sizeof(int));
    virtMemAllocator.write(pval, &val, sizeof(val));

    int *har = &*pval;
    *har = 34;

    std::cout << "pval/val: " << (TVirtPointer)pval << " " << std::dec << *pval << std::endl;

    virtMemAllocator.printStats();

    CDiskPointerWrapper<int> pval2 = virtMemAllocator.alloc(sizeof(int));
    std::cout << "pval2: " << (TVirtPointer)pval2 << std::endl;

    virtMemAllocator.free(pval);
    virtMemAllocator.printStats();
    virtMemAllocator.free(pval2);
    virtMemAllocator.printStats();

    pval = virtMemAllocator.alloc(sizeof(int) * 15);
    std::cout << "pval: " << (TVirtPointer)pval << std::endl;
    for (int i=0; i<15; ++i)
        pval[i] = i * 1000;

    std::cout << "array values:\n";
    for (int i=0; i<15; ++i)
        std::cout << pval[i] << "\n";

    virtMemAllocator.free(pval);

    virtMemAllocator.printStats();

    pval = virtMemAllocator.alloc(sizeof(int) * 1500);
    std::cout << "big array pval: " << (TVirtPointer)pval << std::endl;
    for (int i=0; i<1500; ++i)
    {
        pval[i] = i;
        assert(pval[i] == i);
    }

    std::cout << "------- start check ---------\n";
    for (int i=0; i<1500; ++i)
        assert(pval[i] == i);

    virtMemAllocator.printStats();
    virtMemAllocator.free(pval);
    virtMemAllocator.printStats();

    pval = virtMemAllocator.alloc(sizeof(int));
    std::cout << "pval: " << (TVirtPointer)pval << std::endl;
    virtMemAllocator.printStats();

    CDiskPointerWrapper<short> ps1 = virtMemAllocator.alloc(sizeof(short));
    virtMemAllocator.free(ps1);
#endif
#endif

    return 0;
}
#endif
