#ifndef VIRTMEM_BASE_ALLOC_H
#define VIRTMEM_BASE_ALLOC_H

#define NVALGRIND

#ifndef NVALGRIND
# include <valgrind/memcheck.h>
#endif

#include "config.h"

#include <stdint.h>

typedef uint32_t TVirtPointer;
typedef uint32_t TVirtPtrSize;
typedef uint16_t TVirtPageSize;

class CBaseVirtMemAlloc
{
protected:
#if defined(__x86_64__) || defined(_M_X64)
    typedef __uint128_t TAlign;
#else
    typedef double TAlign;
#endif

private:
    enum
    {
        PAGE_MAX_CLEAN_SKIPS = 5, // if page is dirty: max tries for finding another clean page when swapping
        START_OFFSET = sizeof(TAlign), // don't start at zero so we can have NULL pointers
        BASE_INDEX = 1, // Special pointer to baseFreeList, not actually stored in file
        MIN_ALLOC_SIZE = 16
    };

    union UMemHeader
    {
        struct
        {
            TVirtPointer next;
            TVirtPtrSize size;
        } s;

        TAlign alignDummy;
    };

protected:
#ifndef NVALGRIND
    static const int valgrindPad = 12;
#endif

    struct SLockPage
    {
        TVirtPointer start;
        TVirtPageSize size;
        uint8_t *pool;
        uint8_t locks, cleanSkips;
        bool dirty;
        int8_t next;

        SLockPage(void) : start(0), size(0), pool(0), locks(0), cleanSkips(0), dirty(false), next(-1) { }
    };

private:
    struct SPageInfo
    {
        SLockPage *pages;
        TVirtPageSize size;
        uint8_t count;
        int8_t freeIndex, lockedIndex;
    };

    // Stuff configured from CVirtMemAlloc
    const TVirtPtrSize poolSize;
    SPageInfo smallPages, mediumPages, bigPages;

    UMemHeader baseFreeList;
    TVirtPointer freePointer;
    TVirtPointer poolFreePos;
    int8_t nextPageToSwap;

#ifdef VIRTMEM_TRACE_STATS
    TVirtPtrSize memUsed, maxMemUsed;
    uint32_t bigPageReads, bigPageWrites, bytesRead, bytesWritten;
#endif

    void initPages(SPageInfo *info, SLockPage *pages, uint8_t *pool, uint8_t pcount, TVirtPageSize psize);
    TVirtPointer getMem(TVirtPtrSize size);
    void syncBigPage(SLockPage *page);
    void copyRawData(void *dest, TVirtPointer p, TVirtPtrSize size);
    void saveRawData(void *src, TVirtPointer p, TVirtPtrSize size);
    void *pullRawData(TVirtPointer p, TVirtPtrSize size, bool readonly, bool forcestart);
    void pushRawData(TVirtPointer p, const void *d, TVirtPtrSize size);
    const UMemHeader *getHeaderConst(TVirtPointer p);
    void updateHeader(TVirtPointer p, UMemHeader *h);
    int8_t findFreePage(SPageInfo *pinfo, TVirtPointer p, TVirtPtrSize size, bool atstart);
    int8_t findUnusedLockedPage(SPageInfo *pinfo);
    void syncLockedPage(SLockPage *page);
    int8_t lockPage(SPageInfo *pinfo, TVirtPointer ptr, TVirtPageSize size);
    int8_t freeLockedPage(SPageInfo *pinfo, int8_t index);
    int8_t findLockedPage(SPageInfo *pinfo, TVirtPointer p);
    SLockPage *findLockedPage(TVirtPointer p);
    uint8_t getFreePages(const SPageInfo *pinfo) const;
    uint8_t getUnlockedPages(const SPageInfo *pinfo) const;

protected:
    CBaseVirtMemAlloc(const TVirtPtrSize ps) : poolSize(ps) { }

    void initSmallPages(SLockPage *pages, uint8_t *pool, uint8_t pcount, TVirtPageSize psize) { initPages(&smallPages, pages, pool, pcount, psize); }
    void initMediumPages(SLockPage *pages, uint8_t *pool, uint8_t pcount, TVirtPageSize psize) { initPages(&mediumPages, pages, pool, pcount, psize); }
    void initBigPages(SLockPage *pages, uint8_t *pool, uint8_t pcount, TVirtPageSize psize) { initPages(&bigPages, pages, pool, pcount, psize); }

    void writeZeros(uint32_t start, uint32_t n); // NOTE: only call this in doStart()

    virtual void doStart(void) = 0;
    virtual void doSuspend(void) = 0;
    virtual void doStop(void) = 0;
    virtual void doRead(void *data, TVirtPtrSize offset, TVirtPtrSize size) = 0;
    virtual void doWrite(const void *data, TVirtPtrSize offset, TVirtPtrSize size) = 0;

public:
    void start(void);
    void stop(void);

    TVirtPointer alloc(TVirtPtrSize size);
    void free(TVirtPointer ptr);

    void *read(TVirtPointer p, TVirtPtrSize size);
    void write(TVirtPointer p, const void *d, TVirtPtrSize size);
    void flush(void);
    void clearPages(void);
    uint8_t getFreeBigPages(void) const;
    uint8_t getUnlockedSmallPages(void) const { return getUnlockedPages(&smallPages); }
    uint8_t getUnlockedMediumPages(void) const { return getUnlockedPages(&mediumPages); }
    uint8_t getUnlockedBigPages(void) const { return getUnlockedPages(&bigPages); }

    void *makeDataLock(TVirtPointer ptr, TVirtPageSize size, bool ro=false);
    void *makeFittingLock(TVirtPointer ptr, TVirtPageSize &size, bool ro=false);
    void releaseLock(TVirtPointer ptr);

    uint8_t getSmallPageCount(void) const { return smallPages.count; }
    uint8_t getMediumPageCount(void) const { return mediumPages.count; }
    uint8_t getBigPageCount(void) const { return bigPages.count; }
    TVirtPtrSize getSmallPageSize(void) const { return smallPages.size; }
    TVirtPtrSize getMediumPageSize(void) const { return mediumPages.size; }
    TVirtPtrSize getBigPageSize(void) const { return bigPages.size; }
    TVirtPtrSize getPoolSize(void) const { return poolSize; }

    void printStats(void);

#ifdef VIRTMEM_TRACE_STATS
    TVirtPtrSize getMemUsed(void) const { return memUsed; }
    TVirtPtrSize getMaxMemUsed(void) const { return maxMemUsed; }
    uint32_t getBigPageReads(void) const { return bigPageReads; }
    uint32_t getBigPageWrites(void) const { return bigPageWrites; }
    uint32_t getBytesRead(void) const { return bytesRead; }
    uint32_t getBytesWritten(void) const { return bytesWritten; }
#endif
};

#endif // VIRTMEM_BASE_ALLOC_H
