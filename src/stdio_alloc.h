#ifndef STDIO_ALLOC_H
#define STDIO_ALLOC_H

#include "alloc.h"
#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

struct SStdioMemAllocProperties
{
    static const uint8_t smallPageCount = 4, smallPageSize = 32;
    static const uint8_t mediumPageCount = 4, mediumPageSize = 128;
    static const uint8_t bigPageCount = 4;
    static const uint16_t bigPageSize = 1024;
    static const uint32_t poolSize = DEFAULT_POOLSIZE;
};

template <typename TProperties> class CStdioVirtMemAlloc;

template <typename TProperties = SStdioMemAllocProperties>
class CStdioVirtMemAlloc : public CVirtMemAlloc<TProperties, CStdioVirtMemAlloc<TProperties> >
{
    FILE *ramFile;

    void doStart(void)
    {
        ramFile = tmpfile();
        if (!ramFile)
            fprintf(stderr, "Unable to open ram file!");
        this->writeZeros(0, this->getPoolSize()); // make sure it gets the right size
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

#endif // STDIO_ALLOC_H
