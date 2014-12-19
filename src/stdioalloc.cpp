#include "stdioalloc.h"


#include <string.h>

#if 0
void CStdioVirtMemAlloc::doStart()
{
    ramFile = fopen("ramdisk", "wb+");

    if (!ramFile)
        fprintf(stderr, "Unable to open ram file!");
}

void CStdioVirtMemAlloc::doSuspend()
{

}

void CStdioVirtMemAlloc::doStop()
{
    if (ramFile)
    {
        fclose(ramFile);
        remove("ramdisk");
        ramFile = 0;
    }
}

void CStdioVirtMemAlloc::doRead(void *data, TVirtPtrSize offset, TVirtPtrSize size)
{
    if (fseek(ramFile, offset, SEEK_SET) != 0)
        fprintf(stderr, "fseek error: %s\n", strerror(errno));

    fread(data, size, 1, ramFile);
    if (ferror(ramFile))
        fprintf(stderr, "didn't read correctly: %s\n", strerror(errno));

}

void CStdioVirtMemAlloc::doWrite(const void *data, TVirtPtrSize offset, TVirtPtrSize size)
{
    if (fseek(ramFile, offset, SEEK_SET) != 0)
        fprintf(stderr, "fseek error: %s\n", strerror(errno));

    fwrite(data, size, 1, ramFile);
    if (ferror(ramFile))
        fprintf(stderr, "didn't write correctly: %s\n", strerror(errno));
}
#endif
