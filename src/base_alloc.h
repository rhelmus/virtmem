#ifndef VIRTMEM_BASE_ALLOC_H
#define VIRTMEM_BASE_ALLOC_H

/**
  @file
  @brief Base virtual memory class header
*/

#define NVALGRIND

#ifndef NVALGRIND
# include <valgrind/memcheck.h>
#endif

#include "config.h"

#include <stdint.h>

namespace virtmem {

typedef uint32_t VPtrNum; //!< Numeric type used to store raw virtual pointer addresses
typedef uint32_t VPtrSize; //!< Numeric type used to store the size of a virtual memory block
typedef uint16_t VirtPageSize; //!< Numeric type used to store the size of a virtual memory page

/**
 * @brief Base class for virtual memory allocators.
 *
 * This class defines methods to (de)initialize the allocator (see \ref start() and \ref stop()).
 * In addition, this class can be used for 'raw' memory access.
 */
class BaseVAlloc
{
protected:
    // \cond HIDDEN_SYMBOLS
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
            VPtrNum next;
            VPtrSize size;
        } s;

        TAlign alignDummy;
    };

protected:
#ifndef NVALGRIND
    static const int valgrindPad = 12;
#endif

    // \cond HIDDEN_SYMBOLS
    struct LockPage
    {
        VPtrNum start;
        VirtPageSize size;
        uint8_t *pool;
        uint8_t locks, cleanSkips;
        bool dirty;
        int8_t next;

        LockPage(void) : start(0), size(0), pool(0), locks(0), cleanSkips(0), dirty(false), next(-1) { }
    };
    // \endcond

private:
    struct PageInfo
    {
        LockPage *pages;
        VirtPageSize size;
        uint8_t count;
        int8_t freeIndex, lockedIndex;
    };

    // Stuff configured from VAlloc
    VPtrSize poolSize;
    PageInfo smallPages, mediumPages, bigPages;

    UMemHeader baseFreeList;
    VPtrNum freePointer;
    VPtrNum poolFreePos;
    int8_t nextPageToSwap;

#ifdef VIRTMEM_TRACE_STATS
    VPtrSize memUsed, maxMemUsed;
    uint32_t bigPageReads, bigPageWrites, bytesRead, bytesWritten;
#endif

    void initPages(PageInfo *info, LockPage *pages, uint8_t *pool, uint8_t pcount, VirtPageSize psize);
    VPtrNum getMem(VPtrSize size);
    void syncBigPage(LockPage *page);
    void copyRawData(void *dest, VPtrNum p, VPtrSize size);
    void saveRawData(void *src, VPtrNum p, VPtrSize size);
    void *pullRawData(VPtrNum p, VPtrSize size, bool readonly, bool forcestart);
    void pushRawData(VPtrNum p, const void *d, VPtrSize size);
    const UMemHeader *getHeaderConst(VPtrNum p);
    void updateHeader(VPtrNum p, UMemHeader *h);
    int8_t findFreePage(PageInfo *pinfo, VPtrNum p, VPtrSize size, bool atstart);
    int8_t findUnusedLockedPage(PageInfo *pinfo);
    void syncLockedPage(LockPage *page);
    int8_t lockPage(PageInfo *pinfo, VPtrNum ptr, VirtPageSize size);
    int8_t freeLockedPage(PageInfo *pinfo, int8_t index);
    int8_t findLockedPage(PageInfo *pinfo, VPtrNum p);
    LockPage *findLockedPage(VPtrNum p);
    uint8_t getFreePages(const PageInfo *pinfo) const;
    uint8_t getUnlockedPages(const PageInfo *pinfo) const;

protected:
    BaseVAlloc(void) : poolSize(0) { }

    // \cond HIDDEN_SYMBOLS
    void initSmallPages(LockPage *pages, uint8_t *pool, uint8_t pcount, VirtPageSize psize) { initPages(&smallPages, pages, pool, pcount, psize); }
    void initMediumPages(LockPage *pages, uint8_t *pool, uint8_t pcount, VirtPageSize psize) { initPages(&mediumPages, pages, pool, pcount, psize); }
    void initBigPages(LockPage *pages, uint8_t *pool, uint8_t pcount, VirtPageSize psize) { initPages(&bigPages, pages, pool, pcount, psize); }
    // \endcond

    void writeZeros(VPtrNum start, VPtrSize n); // NOTE: only call this in doStart()

    virtual void doStart(void) = 0;
    virtual void doStop(void) = 0;
    virtual void doRead(void *data, VPtrSize offset, VPtrSize size) = 0;
    virtual void doWrite(const void *data, VPtrSize offset, VPtrSize size) = 0;

public:
    void start(void);
    void stop(void);

    /**
     * @brief Sets the total size of the memory pool.
     * @param ps size of the memory pool.
     * @note The poolsize can also be set via the constructor of most allocators.
     * @note This function is unavailable for MultiSPIRAMVAlloc and StaticVAlloc.
     * @note This function should always called before \ref start().
     */
    void setPoolSize(VPtrSize ps) { poolSize = ps; }

    VPtrNum allocRaw(VPtrSize size);
    void freeRaw(VPtrNum ptr);

    void *read(VPtrNum p, VPtrSize size);
    void write(VPtrNum p, const void *d, VPtrSize size);
    void flush(void);
    void clearPages(void);
    uint8_t getFreeBigPages(void) const;
    uint8_t getUnlockedSmallPages(void) const { return getUnlockedPages(&smallPages); } //!< Returns amount of *small* pages which are not locked.
    uint8_t getUnlockedMediumPages(void) const { return getUnlockedPages(&mediumPages); } //!< Returns amount of *medium* pages which are not locked.
    uint8_t getUnlockedBigPages(void) const { return getUnlockedPages(&bigPages); } //!< Returns amount of *big* pages which are not locked.

    // \cond HIDDEN_SYMBOLS
    void *makeDataLock(VPtrNum ptr, VirtPageSize size, bool ro=false);
    void *makeFittingLock(VPtrNum ptr, VirtPageSize &size, bool ro=false);
    void releaseLock(VPtrNum ptr);
    // \endcond

    uint8_t getSmallPageCount(void) const { return smallPages.count; } //!< Returns total amount of *small* pages.
    uint8_t getMediumPageCount(void) const { return mediumPages.count; } //!< Returns total amount of *medium* pages.
    uint8_t getBigPageCount(void) const { return bigPages.count; } //!< Returns total amount of *big* pages.
    VirtPageSize getSmallPageSize(void) const { return smallPages.size; } //!< Returns the size of a *small* page.
    VirtPageSize getMediumPageSize(void) const { return mediumPages.size; } //!< Returns the size of a *medium* page.
    VirtPageSize getBigPageSize(void) const { return bigPages.size; } //!< Returns the size of a *big* page.

    /**
     * @brief Returns the size the memory pool.
     * @note Some memory is used for bookkeeping, therefore, the amount returned by this function
     * does *not* equate the amount that can be allocated.
     */
    VPtrSize getPoolSize(void) const { return poolSize; }

    // \cond HIDDEN_SYMBOLS
    void printStats(void);
    // \endcond

#ifdef VIRTMEM_TRACE_STATS
    /**
     * @anchor statf
     * @name Statistics functions.
     * The following functions are only available when VIRTMEM_TRACE_STATS is defined (in config.h).
     */
    //@{
    VPtrSize getMemUsed(void) const { return memUsed; } //!< Returns total memory used.
    VPtrSize getMaxMemUsed(void) const { return maxMemUsed; } //!< Returns the maximum memory used so far.
    uint32_t getBigPageReads(void) const { return bigPageReads; } //!< Returns the times *big* pages were read (swapped).
    uint32_t getBigPageWrites(void) const { return bigPageWrites; } //!< Returns the times *big* pages written (synchronized).
    uint32_t getBytesRead(void) const { return bytesRead; } //!< Returns the amount of bytes read as a result of page swaps.
    uint32_t getBytesWritten(void) const { return bytesWritten; } //!< Returns the amount of bytes written as a results of page swaps.
    void resetStats(void) { memUsed = maxMemUsed = 0; bigPageReads = bigPageWrites = bytesRead = bytesWritten = 0; } //!< Reset all statistics. Called by \ref start()
    //@}
#endif
};

}

#endif // VIRTMEM_BASE_ALLOC_H
