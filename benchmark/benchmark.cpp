#include <virtmem.h>
#include <stdio_alloc.h>

#include <chrono>
#include <iostream>

using namespace virtmem;

enum
{
    STDIO_POOLSIZE = 1024 * 128 + 128,
    STDIO_BUFSIZE = 1024 * 128,
    STDIO_REPEATS = 50
};

int main()
{
    StdioVAlloc<> valloc(STDIO_POOLSIZE);

    valloc.start();

    TStdioVirtPtr<char>::type buf = buf.alloc(STDIO_BUFSIZE);

    auto time = std::chrono::high_resolution_clock::now();
    for (int i=0; i<STDIO_REPEATS; ++i)
    {
        for (int j=0; j<STDIO_BUFSIZE; ++j)
            buf[j] = (char)j;
    }

    const unsigned difftime =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - time).count();

    std::cout << "Finished in " << difftime << " ms\n";
    std::cout << "Speed: " << STDIO_REPEATS * STDIO_BUFSIZE / difftime * 1000 / 1024 << " kB/s\n";

    return 0;
}

