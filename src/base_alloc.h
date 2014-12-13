#ifndef BASE_ALLOC_H
#define BASE_ALLOC_H

#include <stdint.h>

typedef uint32_t TVirtPointer;
typedef uint32_t TVirtSizeType;

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
            TVirtSizeType size;
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
    const TVirtSizeType poolSize, pageSize;

    UMemHeader baseFreeList;
    TVirtPointer freePointer;
    TVirtPointer poolFreePos;
    uint8_t nextPageToSwap;

    TVirtPointer getMem(TVirtSizeType size);
    void syncPage(SMemPage *page);
    void *pullData(TVirtPointer p, TVirtSizeType size, bool readonly, bool forcestart);
    void pushData(TVirtPointer p, const void *d, TVirtSizeType size);
    UMemHeader *getHeader(TVirtPointer p);
    const UMemHeader *getHeaderConst(TVirtPointer p);

protected:
    CBaseVirtMemAlloc(SMemPage *mp, const uint8_t mc, const TVirtSizeType ps, const TVirtSizeType pgs);
    ~CBaseVirtMemAlloc(void) { }

    virtual void doStart(void) = 0;
    virtual void doSuspend(void) = 0;
    virtual void doStop(void) = 0;
    virtual void doRead(void *data, TVirtSizeType offset, TVirtSizeType size) = 0;
    virtual void doWrite(const void *data, TVirtSizeType offset, TVirtSizeType size) = 0;

public:
    void start(void);
    void stop(void) { doStop(); }

    TVirtPointer alloc(TVirtSizeType size);
    void free(TVirtPointer ptr);

    void *read(TVirtPointer p, TVirtSizeType size, bool ro=true) { return pullData(p, size, ro, false); }
    void write(TVirtPointer p, const void *d, TVirtSizeType size) { pushData(p, d, size); }
    void *lock(TVirtPointer p, bool ro=true);
    void unlock(TVirtPointer p);
    void flush(void);
    void clearPages(void);

    static CBaseVirtMemAlloc *getInstance(void) { return instance; }

    void printStats(void);
};

#endif // BASE_ALLOC_H
