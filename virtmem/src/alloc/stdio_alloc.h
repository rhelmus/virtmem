#ifndef VIRTMEM_STDIO_ALLOC_H
#define VIRTMEM_STDIO_ALLOC_H

/**
  * @file
  * @brief This file contains the stdio virtual memory allocator (for debug)
  */

#include "internal/alloc.h"
#include "config/config.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

namespace virtmem {

/**
 * @brief Virtual memory allocator that uses a regular file (via stdio) as memory pool.
 *
 * This class is meant for debugging and can only be used on systems supporting stdio (e.g. PCs).
 *
 * @tparam Properties Allocator properties, see DefaultAllocProperties
 *
 * @sa @ref bUsing
 */
template <typename Properties = DefaultAllocProperties>
class StdioVAllocP : public VAlloc<Properties, StdioVAllocP<Properties> >
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
    void doRead(void *data, VPtrSize offset, VPtrSize size)
    {
        if (fseek(ramFile, offset, SEEK_SET) != 0)
            fprintf(stderr, "fseek error: %s\n", strerror(errno));

        fread(data, size, 1, ramFile);
        if (ferror(ramFile))
            fprintf(stderr, "didn't read correctly: %s\n", strerror(errno));

    }

    void doWrite(const void *data, VPtrSize offset, VPtrSize size)
    {
        if (fseek(ramFile, offset, SEEK_SET) != 0)
            fprintf(stderr, "fseek error: %s\n", strerror(errno));

        fwrite(data, size, 1, ramFile);
        if (ferror(ramFile))
            fprintf(stderr, "didn't write correctly: %s\n", strerror(errno));
    }

public:
    /**
     * @brief Constructs (but not initializes) the allocator.
     * @param ps Total amount of bytes of the memory pool.
     * @sa setPoolSize
     */
    StdioVAllocP(VPtrSize ps=VIRTMEM_DEFAULT_POOLSIZE) : ramFile(0) { this->setPoolSize(ps); }
    ~StdioVAllocP(void) { doStop(); }
};

typedef StdioVAllocP<> StdioVAlloc; //!< Shortcut to StdioVAllocP with default template arguments

}

#endif // VIRTMEM_STDIO_ALLOC_H
