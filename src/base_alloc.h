#ifndef BASE_ALLOC_H
#define BASE_ALLOC_H

#include <stdint.h>

typedef uint32_t TVirtPointer;
typedef uint32_t TVirtPtrSize;
typedef uint16_t TVirtPageSize;

class CBaseVirtMemAlloc
{
    typedef uint32_t TAlign;

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
    struct SMemPage
    {
        TVirtPointer start;
        void *pool;
        bool dirty;
        uint8_t locks, cleanSkips;

        SMemPage(void) : start(0), dirty(false), locks(0), cleanSkips(0) { }
    };

    struct SPartialLockPage
    {
        TVirtPointer start;
        TVirtPageSize size;
        uint8_t *pool;
        uint8_t locks;
        bool dirty;
        int8_t next;

        SPartialLockPage(void) : start(0), size(0), pool(0), locks(0), dirty(false), next(-1) { }
    };

private:
    struct SPageInfo
    {
        SPartialLockPage *pages;
        TVirtPageSize size;
        uint8_t count;
        int8_t freeIndex, usedIndex;
    };

    // Stuff configured from CVirtMemAlloc
    SMemPage *memPageList;
    const uint8_t pageCount;
    const TVirtPtrSize poolSize, pageSize;
    SPageInfo smallPages, mediumPages, bigPages;

    UMemHeader baseFreeList;
    TVirtPointer freePointer;
    TVirtPointer poolFreePos;
    uint8_t nextPageToSwap;

    void initPages(SPageInfo *info, SPartialLockPage *pages, uint8_t *pool, uint8_t pcount, TVirtPageSize psize);
    TVirtPointer getMem(TVirtPtrSize size);
    void syncPage(SMemPage *page);
    void *pullRawData(TVirtPointer p, TVirtPtrSize size, bool readonly, bool forcestart);
    void pushRawData(TVirtPointer p, const void *d, TVirtPtrSize size);
    const UMemHeader *getHeaderConst(TVirtPointer p);
    void updateHeader(TVirtPointer p, UMemHeader *h);
    void syncPartialPage(SPartialLockPage *page);
    int8_t freePartialPage(SPageInfo *pinfo, int8_t index);
    int8_t findPartialLockPage(SPageInfo *pinfo, TVirtPointer p);
    SPartialLockPage *findPartialLockPage(TVirtPointer p);
    void removePartialLockFrom(SPageInfo *pinfo, TVirtPointer p);

protected:
    CBaseVirtMemAlloc(SMemPage *mp, const uint8_t pc, const TVirtPtrSize ps, const TVirtPtrSize pgs);

    void initSmallPages(SPartialLockPage *pages, uint8_t *pool, uint8_t pcount, TVirtPageSize psize) { initPages(&smallPages, pages, pool, pcount, psize); }
    void initMediumPages(SPartialLockPage *pages, uint8_t *pool, uint8_t pcount, TVirtPageSize psize) { initPages(&mediumPages, pages, pool, pcount, psize); }
    void initBigPages(SPartialLockPage *pages, uint8_t *pool, uint8_t pcount, TVirtPageSize psize) { initPages(&bigPages, pages, pool, pcount, psize); }

    virtual void doStart(void) = 0;
    virtual void doSuspend(void) = 0;
    virtual void doStop(void) = 0;
    virtual void doRead(void *data, TVirtPtrSize offset, TVirtPtrSize size) = 0;
    virtual void doWrite(const void *data, TVirtPtrSize offset, TVirtPtrSize size) = 0;

public:
    void start(void);
    void stop(void) { doStop(); }

    TVirtPointer alloc(TVirtPtrSize size);
    void free(TVirtPointer ptr);

    void *read(TVirtPointer p, TVirtPtrSize size);
    void write(TVirtPointer p, const void *d, TVirtPtrSize size);
    void flush(void);
    void clearPages(void);
    uint8_t getFreePages(void) const;

    void *lock(TVirtPointer p, bool ro=false);
    void unlock(TVirtPointer p);
    uint8_t getUnlockedPages(void) const;
    TVirtPtrSize getMaxLockSize(TVirtPointer p, TVirtPtrSize reqsize, TVirtPtrSize *blockedsize) const;

    void *makePartialLock(TVirtPointer ptr, TVirtPageSize size, bool ro=false);
    void releasePartialLock(TVirtPointer ptr);

    uint8_t getPageCount(void) const { return pageCount; }
    TVirtPtrSize getPageSize(void) const { return pageSize; }
    TVirtPtrSize getPoolSize(void) const { return poolSize; }

    void printStats(void);
};

#endif // BASE_ALLOC_H
