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
        bool dirty, locked;
        uint8_t cleanSkips;

        SMemPage(void) : start(0), dirty(false), locked(false), cleanSkips(0) { }
    };

private:
    static CBaseVirtMemAlloc *instance;

    // Stuff configured from CVirtMemAlloc
    SMemPage *memPageList;
    const uint8_t pageCount;
    const TVirtPtrSize poolSize, pageSize;

    UMemHeader baseFreeList;
    TVirtPointer freePointer;
    TVirtPointer poolFreePos;
    uint8_t nextPageToSwap;

    TVirtPointer getMem(TVirtPtrSize size);
    void syncPage(SMemPage *page);
    void *pullData(TVirtPointer p, TVirtPtrSize size, bool readonly, bool forcestart);
    void pushData(TVirtPointer p, const void *d, TVirtPtrSize size);
    UMemHeader *getHeader(TVirtPointer p);
    const UMemHeader *getHeaderConst(TVirtPointer p);

protected:
    CBaseVirtMemAlloc(SMemPage *mp, const uint8_t mc, const TVirtPtrSize ps, const TVirtPtrSize pgs);
    ~CBaseVirtMemAlloc(void) { instance = 0; }

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

    void *read(TVirtPointer p, TVirtPtrSize size, bool ro=true) { return pullData(p, size, ro, false); }
    void write(TVirtPointer p, const void *d, TVirtPtrSize size) { pushData(p, d, size); }
    void *lock(TVirtPointer p, bool ro=false);
    void unlock(TVirtPointer p);
    void flush(void);
    void clearPages(void);
    uint8_t getUnlockedPages(void) const;
    uint8_t getFreePages(void) const;

    static CBaseVirtMemAlloc *getInstance(void) { return instance; }
    uint8_t getPageCount(void) const { return pageCount; }
    TVirtPtrSize getPageSize(void) const { return pageSize; }

    void printStats(void);
};

#endif // BASE_ALLOC_H
