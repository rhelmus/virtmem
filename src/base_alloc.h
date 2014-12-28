#ifndef BASE_ALLOC_H
#define BASE_ALLOC_H

#include <stdint.h>

typedef uint32_t TVirtPointer;
typedef uint32_t TVirtPtrSize;

class CBaseVirtMemAlloc
{
    typedef uint32_t TAlign;

    enum
    {
        MAX_PARTIAL_LOCK_PAGES = 16, // UNDONE: make amount configurable
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
        TVirtPtrSize size;
        uint8_t data[128]; // UNDONE
        uint8_t locks;
        bool readOnly;
        int8_t next;

        SPartialLockPage(void) : start(0), size(0), locks(0), readOnly(false), next(-1) { }
    };

private:
    // Stuff configured from CVirtMemAlloc
    SMemPage *memPageList;
    const uint8_t pageCount;
    const TVirtPtrSize poolSize, pageSize;
    int8_t freePageIndex, usedPageIndex;

    SPartialLockPage partialLockPages[MAX_PARTIAL_LOCK_PAGES];

    UMemHeader baseFreeList;
    TVirtPointer freePointer;
    TVirtPointer poolFreePos;
    uint8_t nextPageToSwap;

    TVirtPointer getMem(TVirtPtrSize size);
    void syncPage(SMemPage *page);
    void *pullRawData(TVirtPointer p, TVirtPtrSize size, bool readonly, bool forcestart);
    void pushRawData(TVirtPointer p, const void *d, TVirtPtrSize size);
    UMemHeader *getHeader(TVirtPointer p);
    const UMemHeader *getHeaderConst(TVirtPointer p);
    void updateHeader(TVirtPointer p, UMemHeader *h);
    void syncPartialPage(int8_t index);
    int8_t freePartialPage(int8_t index);
    int8_t findPartialLockPage(TVirtPointer p);

protected:
    CBaseVirtMemAlloc(SMemPage *mp, const uint8_t pc, const TVirtPtrSize ps, const TVirtPtrSize pgs);

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

    void *read(TVirtPointer p, TVirtPtrSize size, bool readonly=true);
    void write(TVirtPointer p, const void *d, TVirtPtrSize size);
    void flush(void);
    void clearPages(void);
    uint8_t getFreePages(void) const;

    void *lock(TVirtPointer p, bool ro=false);
    void unlock(TVirtPointer p);
    uint8_t getUnlockedPages(void) const;
    TVirtPtrSize getMaxLockSize(TVirtPointer p, TVirtPtrSize reqsize, TVirtPtrSize *blockedsize) const;

    void *makePartialLock(TVirtPointer ptr, TVirtPtrSize size, void *data, bool ro=false);
    void releasePartialLock(TVirtPointer ptr);

    uint8_t getPageCount(void) const { return pageCount; }
    TVirtPtrSize getPageSize(void) const { return pageSize; }
    TVirtPtrSize getPoolSize(void) const { return poolSize; }

    void printStats(void);
};

#endif // BASE_ALLOC_H
