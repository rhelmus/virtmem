#ifndef VIRTMEM_STDIO_ALLOC_H
#define VIRTMEM_STDIO_ALLOC_H

/**
  * @file
  * @brief This file contains the stdio virtual memory allocator (for debug)
  */

#include "alloc.h"
#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

namespace virtmem {

template <typename TProperties> class CStdioVirtMemAlloc;

template <typename TProperties = SDefaultAllocProperties>
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
    CStdioVirtMemAlloc(TVirtPtrSize ps=DEFAULT_POOLSIZE) : ramFile(0) { this->setPoolSize(ps); }
    ~CStdioVirtMemAlloc(void) { doStop(); }
};

template <typename, typename> class CVirtPtr;
template <typename T> struct TStdioVirtPtr { typedef CVirtPtr<T, CStdioVirtMemAlloc<> > type; };

}

#endif // VIRTMEM_STDIO_ALLOC_H
