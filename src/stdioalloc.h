#ifndef STDIOALLOC_H
#define STDIOALLOC_H

#include "alloc.h"

#include <stdio.h>

class CStdioVirtMemAlloc : public CVirtMemAlloc<>
{
    void doStart(void);
    void doSuspend(void);
    void doStop(void);
    void doRead(void *data, TVirtSizeType offset, TVirtSizeType size);
    void doWrite(const void *data, TVirtSizeType offset, TVirtSizeType size);

    FILE *ramFile;

public:
    CStdioVirtMemAlloc(void) : ramFile(0) { }
    ~CStdioVirtMemAlloc(void) { doStop(); }
};

template <typename, typename> class CVirtPtr;
template <typename T> struct TStdioVirtPtr { typedef CVirtPtr<T, CStdioVirtMemAlloc> type; };

#endif // DISKALLOC_H
