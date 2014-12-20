#ifndef STDIOALLOC_H
#define STDIOALLOC_H

#include "alloc.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

template <TVirtPtrSize, TVirtPtrSize, uint8_t> class CStdioVirtMemAlloc;

template <TVirtPtrSize POOL_SIZE = 1024*1024*10, TVirtPtrSize PAGE_SIZE=512, uint8_t PAGE_COUNT=4>
class CStdioVirtMemAlloc : public CVirtMemAlloc<POOL_SIZE, PAGE_SIZE, PAGE_COUNT,
        CStdioVirtMemAlloc<POOL_SIZE, PAGE_SIZE, PAGE_COUNT> >
{
    FILE *ramFile;

    void doStart(void)
    {
        ramFile = tmpfile();
        if (!ramFile)
            fprintf(stderr, "Unable to open ram file!");
    }

    void doSuspend(void) { }
    void doStop(void) { if (ramFile) { fclose(ramFile); ramFile = 0; } }
    void doRead(void *data, TVirtPtrSize offset, TVirtPtrSize size)
    {
        if (fseek(ramFile, offset, SEEK_SET) != 0)
            fprintf(stderr, "fseek error: %s\n", strerror(errno));

        fread(data, size, 1, ramFile);
        if (ferror(ramFile))
            fprintf(stderr, "didn't read correctly: %s\n", strerror(errno));

    }

    void doWrite(const void *data, TVirtPtrSize offset, TVirtPtrSize size)
    {
        if (fseek(ramFile, offset, SEEK_SET) != 0)
            fprintf(stderr, "fseek error: %s\n", strerror(errno));

        fwrite(data, size, 1, ramFile);
        if (ferror(ramFile))
            fprintf(stderr, "didn't write correctly: %s\n", strerror(errno));
    }

public:
    CStdioVirtMemAlloc(void) : ramFile(0) { }
    ~CStdioVirtMemAlloc(void) { doStop(); }
};

template <typename, typename> class CVirtPtr;
template <typename T> struct TStdioVirtPtr { typedef CVirtPtr<T, CStdioVirtMemAlloc<> > type; };

#endif // DISKALLOC_H
