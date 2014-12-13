#include "virtmem.h"
#include "stdioalloc.h"

#include <assert.h>
#include <iomanip>
#include <iostream>

#include <string.h>

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

