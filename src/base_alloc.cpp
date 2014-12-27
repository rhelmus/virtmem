/*
 * Memory allocator based on memmgr, by Eli Bendersky
 * https://github.com/eliben/code-for-blog/tree/master/2008/memmgr
 */

#include "alloc.h"
#include "utils.h"

#include <algorithm>
#include <iomanip>
#include <iostream>

#include <assert.h>
#include <string.h>

#ifdef PRINTF_STATS
#include <stdio.h>
#endif

CBaseVirtMemAlloc::CBaseVirtMemAlloc(CBaseVirtMemAlloc::SMemPage *mp, const uint8_t pc, const TVirtPtrSize ps,
                                     const TVirtPtrSize pgs)
    : memPageList(mp), pageCount(pc), poolSize(ps), pageSize(pgs), freePointer(0), nextPageToSwap(0)
{
    baseFreeList.s.next = 0;
    baseFreeList.s.size = 0;
    poolFreePos = START_OFFSET + sizeof(UMemHeader);
}

TVirtPointer CBaseVirtMemAlloc::getMem(TVirtPtrSize size)
{
    size = std::max(size, (TVirtPtrSize)MIN_ALLOC_SIZE); // UNDONE
    const TVirtPtrSize totalsize = size * sizeof(UMemHeader);

    if ((poolFreePos + totalsize) <= poolSize)
    {
//        std::cout << "new mem at " << poolFreePos << "/" << (poolFreePos + sizeof(UMemHeader)) << std::endl;

        UMemHeader h;
        h.s.size = size;
        h.s.next = 0;
        write(poolFreePos, &h, sizeof(UMemHeader));
        free(poolFreePos + sizeof(UMemHeader));
        poolFreePos += totalsize;
    }
    else
        return 0;

    return freePointer;
}

void CBaseVirtMemAlloc::syncPage(SMemPage *page)
{
    if (page->dirty)
    {
//        std::cout << "dirty page\n";
        doWrite(page->pool, page->start, pageSize);
        page->dirty = false;
        page->cleanSkips = 0;
    }
}

void *CBaseVirtMemAlloc::pullRawData(TVirtPointer p, TVirtPtrSize size, bool readonly, bool forcestart)
{
    assert(p && p < poolSize);

    /* If a page is found which fits within the pointer: take that and abort search; no overlap can occur
     * If a page partially overlaps take that, as it has to be cleared out anyway. Keep searching for
     * other overlapping pages.
     * Otherwise if an empty page is found use it but keep searching for the above.
     * Otherwise if a 'clean' page is found use that but keep searching for the above.
     * Otherwise look for dirty pages in a FIFO way. */

    SMemPage *page = 0;
    const TVirtPointer pend = p + size, newpageend = p + pageSize;
    enum { STATE_GOTFULL, STATE_GOTPARTIAL, STATE_GOTEMPTY, STATE_GOTCLEAN, STATE_GOTDIRTY, STATE_GOTNONE } pagefindstate = STATE_GOTNONE;

    // Start by looking for fitting pages, the ideal situation
    for (uint8_t i=0; i<pageCount; ++i)
    {
        if (memPageList[i].start != 0 && ((forcestart && memPageList[i].start == p) ||
            (!forcestart && p >= memPageList[i].start && pend <= (memPageList[i].start + pageSize))))
        {
            page = &memPageList[i];
            pagefindstate = STATE_GOTFULL;
            break;
        }
    }

    if (pagefindstate != STATE_GOTFULL)
    {
        for (uint8_t i=0; i<pageCount; ++i)
        {
            if (memPageList[i].locks)
            {
                if (i == nextPageToSwap)
                    ++nextPageToSwap;
                continue;
            }

            if (memPageList[i].start != 0)
            {
                const TVirtPointer pageend = memPageList[i].start + pageSize;
                if ((p >= memPageList[i].start && p < pageend) ||
                    (newpageend >= memPageList[i].start && newpageend <= pageend))
                {
                    // partial overlap. Clean page and use it later.
                    page = &memPageList[i];
                    syncPage(page);
//                    std::cout << "invalidate page " << page->start << std::endl;
                    page->start = 0; // invalidate
                    pagefindstate = STATE_GOTPARTIAL;
                }
            }
            else if (pagefindstate != STATE_GOTPARTIAL)
            {
                page = &memPageList[i];
                pagefindstate = STATE_GOTEMPTY;
            }

            if (pagefindstate > STATE_GOTCLEAN)
            {
                if (!memPageList[i].dirty || (++memPageList[i].cleanSkips) >= PAGE_MAX_CLEAN_SKIPS)
                {
                    page = &memPageList[i];
                    pagefindstate = STATE_GOTCLEAN;
                }
                else if (pagefindstate != STATE_GOTDIRTY && i == nextPageToSwap)
                {
                    page = &memPageList[i];
                    pagefindstate = STATE_GOTDIRTY;
                }
            }
        }
    }

    // 'page' should now point to page which is within or closest to pointer range
    assert(page);

    // do we need to swap a page?
    if (pagefindstate != STATE_GOTFULL)
    {
//        std::cout << "getPool switches " << (page - memPageList) << " from: " << page->start << " to " << p << std::endl;

        if (page->start != 0)
            syncPage(page);

        if (pagefindstate == STATE_GOTDIRTY)
        {
            ++nextPageToSwap;
            if (nextPageToSwap >= pageCount)
                nextPageToSwap = 0;
        }
        else
            nextPageToSwap = 0;

        // Load in page
        doRead(page->pool, p, pageSize);

        page->start = p;
    }

    if (!readonly)
        page->dirty = true;

    assert(p >= page->start);

    return &((uint8_t *)page->pool)[p - page->start];
}

void CBaseVirtMemAlloc::pushRawData(TVirtPointer p, const void *d, TVirtPtrSize size)
{
    void *pool = pullRawData(p, size, false, false);
    memcpy(pool, d, size);
}

CBaseVirtMemAlloc::UMemHeader *CBaseVirtMemAlloc::getHeader(TVirtPointer p)
{
    if (p == BASE_INDEX)
        return &baseFreeList;
    return reinterpret_cast<UMemHeader *>(read(p, sizeof(UMemHeader), false));
}

const CBaseVirtMemAlloc::UMemHeader *CBaseVirtMemAlloc::getHeaderConst(TVirtPointer p)
{
    if (p == BASE_INDEX)
        return &baseFreeList;
    return reinterpret_cast<UMemHeader *>(read(p, sizeof(UMemHeader), true));
}

void CBaseVirtMemAlloc::updateHeader(TVirtPointer p, UMemHeader *h)
{
    if (p == BASE_INDEX)
        memcpy(&baseFreeList, h, sizeof(UMemHeader));
    else
        write(p, h, sizeof(UMemHeader));
}

CBaseVirtMemAlloc::SPartialLockPage *CBaseVirtMemAlloc::findPartialLockPage(TVirtPointer p)
{
    for (int i=0; i<MAX_PARTIAL_LOCK_PAGES; ++i)
    {
        const TVirtPointer offset = p - partialLockPages[i].start;

        if (partialLockPages[i].locks && p >= partialLockPages[i].start && offset < partialLockPages[i].size)
            return &partialLockPages[i];
    }

    return 0;
}

void CBaseVirtMemAlloc::start()
{
    doStart();

    // Initialize ram file with zeros. Use zeroed page as buffer.
    memset(memPageList[0].pool, 0, pageSize);
    for (TVirtPtrSize i=0; i<poolSize; i+=pageSize)
    {
        const TVirtPtrSize s = (i + pageSize) < poolSize ? (pageSize) : (poolSize - i);
        doWrite(memPageList[0].pool, i, s);
    }
}

TVirtPointer CBaseVirtMemAlloc::alloc(TVirtPtrSize size)
{
    const TVirtPtrSize quantity = (size + sizeof(UMemHeader) - 1) / sizeof(UMemHeader) + 1;
    TVirtPointer prevp = freePointer;

    assert(size && quantity);

    // First alloc call, and no free list yet ? Use 'base' for an initial
    // denegerate block of size 0, which points to itself
    if (prevp == 0)
    {
        baseFreeList.s.next = freePointer = prevp = BASE_INDEX;
        baseFreeList.s.size = 0;
    }

    TVirtPointer p = getHeader(prevp)->s.next;
    while (true)
    {
        UMemHeader *h = getHeader(p);

        // big enough ?
        if (h->s.size >= quantity)
        {
            // exactly ?
            if (h->s.size == quantity)
            {
                // just eliminate this block from the free list by pointing
                // its prev's next to its next

                TVirtPointer next = h->s.next;
                getHeader(prevp)->s.next = next;
                // NOTE: getHeader might invalidate h from here ----
            }
            else // too big
            {
                h->s.size -= quantity;
                p += (h->s.size * sizeof(UMemHeader));
                h = getHeader(p);
                h->s.size = quantity;
            }

            freePointer = prevp;
            return p + sizeof(UMemHeader);
        }

        // Reached end of free list ?
        // Try to allocate the block from the pool. If that succeeds,
        // getMem adds the new block to the free list and
        // it will be found in the following iterations. If the call
        // to getMem doesn't succeed, we've run out of
        // memory
        else if (p == freePointer)
        {
            if ((p = getMem(quantity)) == 0)
            {
                std::cout << "!! Memory allocation failed !!\n";
                assert(false);
                return 0;
            }
            h = getHeader(p);
        }

        prevp = p;
        p = h->s.next;
        assert(p);
    }
}

// Scans the free list, starting at freePointer, looking the the place to insert the
// free block. This is either between two existing blocks or at the end of the
// list. In any case, if the block being freed is adjacent to either neighbor,
// the adjacent blocks are combined.
void CBaseVirtMemAlloc::free(TVirtPointer ptr)
{
    if (!ptr)
        return;

    // acquire pointer to block header
    const TVirtPointer hdrptr = ptr - sizeof(UMemHeader);
    UMemHeader statheader;
    memcpy(&statheader, getHeaderConst(hdrptr), sizeof(UMemHeader));

    // Find the correct place to place the block in (the free list is sorted by
    // address, increasing order)
    /*for (TFilePointer p=freePointer, UMemHeader *h=getHeader(p); !(header > p && header < h->s.next); p=h->s.next, h=getHeader(p))*/
    TVirtPointer p = freePointer;
    const UMemHeader *consth = getHeaderConst(p);
    while (!(hdrptr > p && hdrptr < consth->s.next))
    {
        // Since the free list is circular, there is one link where a
        // higher-addressed block points to a lower-addressed block.
        // This condition checks if the block should be actually
        // inserted between them
        if (p >= consth->s.next && (hdrptr > p || hdrptr < consth->s.next))
            break;

        p = consth->s.next;
        consth = getHeaderConst(p);
    }

    UMemHeader stath;
    memcpy(&stath, consth, sizeof(UMemHeader));

    // Try to combine with the higher neighbor
    if ((hdrptr + statheader.s.size) == stath.s.next)
    {
        const UMemHeader *nexth = getHeaderConst(stath.s.next);
        statheader.s.size += nexth->s.size;
        statheader.s.next = nexth->s.next;
    }
    else
        statheader.s.next = stath.s.next;

    // update blockheader
    updateHeader(hdrptr, &statheader);

    UMemHeader *h = getHeader(p); // now get a writable copy (so searching won't mark every page dirty)

    // Try to combine with the lower neighbor
    if (p + h->s.size == hdrptr)
    {
        h->s.size += statheader.s.size;
        h->s.next = statheader.s.next;
    }
    else
        h->s.next = hdrptr;

    assert(p);
    assert(h->s.next);
    freePointer = p;
}

void *CBaseVirtMemAlloc::read(TVirtPointer p, TVirtPtrSize size, bool readonly)
{
    // check if data is in a partial locked page first
    // UNDONE: does this mess up regular page locking?
    SPartialLockPage *page = findPartialLockPage(p);
    if (page)
    {
        const TVirtPointer offset = p - page->start;

        // data fits in this page?
        if ((offset + size) <= page->size)
        {
            if (!readonly && page->readOnly)
                page->readOnly = false;
//            std::cout << "using temp lock page " << (int)(page - partialLockPages) << ", " << p << std::endl;

            return (char *)page->data + offset;
        }

        // only fits partially... mirror data to normal page so a continuous block can be returned
        pushRawData(page->start, page->data, page->size); // UNDONE
//        std::cout << "mirrored partial page " << (int)(page - partialLockPages) << std::endl;
    }

    // not in or too big for partial page, use regular paged memory
    return pullRawData(p, size, readonly, false);
}

void CBaseVirtMemAlloc::write(TVirtPointer p, const void *d, TVirtPtrSize size)
{
    // check if data is in a partial locked page first
    // UNDONE: does this mess up regular page locking?
    SPartialLockPage *page = findPartialLockPage(p);
    if (page)
    {
        const TVirtPointer offset = p - page->start;

        if (page->readOnly)
            page->readOnly = false;

        // data fits in this page?
        if ((offset + size) <= page->size)
        {
            memcpy((char *)page->data + offset, d, size);
//            std::cout << "write in part locked page " << (int)(page - partialLockPages) << ", " << p << "/" << size << ": " << (int)*(char *)d << std::endl;
        }
        else
        {
            // partial fit, copy stuff that fits in partial page and rest to regular memory
            const TVirtPtrSize partsize = page->size - offset;
            memcpy((char *)page->data + offset, d, partsize);
            pushRawData(p + partsize, (char *)d + partsize, size - partsize);
//            std::cout << "partial write in page " << (int)(page - partialLockPages) << std::endl;
        }
    }
    else
    {
        pushRawData(p, d, size);
//        std::cout << "write in regular page " << p << "/" << size << ": " << (int)*(char *)d << std::endl;
    }
}

void CBaseVirtMemAlloc::flush()
{
    for (uint8_t i=0; i<pageCount; ++i)
        syncPage(&memPageList[i]);
}

void CBaseVirtMemAlloc::clearPages()
{
    flush();
    // wipe all pages
    for (uint8_t i=0; i<pageCount; ++i)
        memPageList[i].start = 0;
}

uint8_t CBaseVirtMemAlloc::getFreePages() const
{
    uint8_t ret = pageCount;

    for (uint8_t i=0; i<pageCount; ++i)
    {
        if (memPageList[i].start != 0)
            --ret;
    }

    return ret;
}

void *CBaseVirtMemAlloc::lock(TVirtPointer p, bool ro)
{
    void *ret = pullRawData(p, pageSize, ro, true);

    // Find which page was used and mark it as locked
    for (uint8_t i=0; i<pageCount; ++i)
    {
        if (memPageList[i].start == p)
        {
//            if (!memPageList[i].locks)
//                std::cout << "lock page: " << (int)i << std::endl;
            ++memPageList[i].locks;
            assert(ret == memPageList[i].pool);
            return ret;
        }
    }

    assert(false);
}

void CBaseVirtMemAlloc::unlock(TVirtPointer p)
{
    for (uint8_t i=0; i<pageCount; ++i)
    {
        if (memPageList[i].start == p)
        {
            assert(memPageList[i].locks);
            --memPageList[i].locks;
//            if (!memPageList[i].locks)
//                std::cout << "unlock page: " << (int)i << std::endl;
            break;
        }
    }
}


uint8_t CBaseVirtMemAlloc::getUnlockedPages() const
{
    uint8_t ret = pageCount;

    for (uint8_t i=0; i<pageCount; ++i)
    {
        if (memPageList[i].locks)
            --ret;
    }

    return ret;
}

TVirtPtrSize CBaseVirtMemAlloc::getMaxLockSize(TVirtPointer p, TVirtPtrSize reqsize, TVirtPtrSize *blockedsize) const
{
    TVirtPtrSize retsize = private_utils::min(reqsize, pageSize);
    TVirtPtrSize blsize = 0;

    for (int i=0; i<MAX_PARTIAL_LOCK_PAGES; ++i)
    {
        if (partialLockPages[i].locks)
        {
            // start block within partial page?
            if (p >= partialLockPages[i].start && p < (partialLockPages[i].start + partialLockPages[i].size))
            {
                retsize = 0;
                blsize = private_utils::max(blsize, partialLockPages[i].size);
            }
            // end block within partial page?
            else if (p < partialLockPages[i].start && (p + retsize) > partialLockPages[i].start)
            {
                retsize = private_utils::min(retsize, (partialLockPages[i].start - p));
                blsize = private_utils::max(blsize, partialLockPages[i].size);
            }
        }
    }

    if (blockedsize)
        *blockedsize = blsize;

    assert(retsize <= reqsize);

    return retsize;
}

void *CBaseVirtMemAlloc::makePartialLock(TVirtPointer ptr, TVirtPtrSize size, void *data, bool ro)
{
    SPartialLockPage *page = NULL;
    TVirtPtrSize copyoffset = 0;

    assert(ptr != 0);

    for (int i=0; i<MAX_PARTIAL_LOCK_PAGES; ++i)
    {
        // already locked?
        if (partialLockPages[i].locks)
        {
            if (partialLockPages[i].start == ptr) // same data, add to lock count
            {
                assert(partialLockPages[i].size <= size);
                ++partialLockPages[i].locks;
                if (!ro && partialLockPages[i].readOnly)
                    partialLockPages[i].readOnly = false;
                return partialLockPages[i].data;
            }

            if (ptr < partialLockPages[i].start && (ptr + size) >= partialLockPages[i].start) // end overlaps
            {
//                std::cout << "shrink new page from " << size << " to ";
                size = partialLockPages[i].start - ptr; // Shrink so it fits
//                std::cout << size << std::endl;
            }
            else if (ptr > partialLockPages[i].start && ptr < (partialLockPages[i].start + partialLockPages[i].size))
            {
                // copy their overlapping data (assume this is the most up to date)
                const TVirtPtrSize offsetold = ptr - partialLockPages[i].start, copysize = private_utils::min(partialLockPages[i].size - offsetold, size);
                memcpy(data, (char *)partialLockPages[i].data + offsetold, copysize);
                copyoffset = copysize;
//                std::cout << "shrink page " << i << "from " << partialLockPages[i].size << " to ";
                partialLockPages[i].size = offsetold; // shrink other so this one fits
//                std::cout << partialLockPages[i].size << std::endl;
            }

//            assert((ptr+size) <= partialLockPages[i].start || ptr >= (partialLockPages[i].start + partialLockPages[i].size));
        }
        else if (!page)
            page = &partialLockPages[i];
    }

    assert(page);

    // if we are here there was no locked page found

    if (copyoffset < size)
    {
        const TVirtPtrSize copysize = size - copyoffset;
        memcpy((char *)data + copyoffset, pullRawData(ptr + copyoffset, copysize, true, false), copysize); // UNDONE: make this more efficient
    }

    page->locks = 1;
    page->start = ptr;
    page->size = size;
    page->data = data;

//    std::cout << "temp lock page: " << (int)(page - partialLockPages) << ", " << ptr << "/" << size << std::endl;

    return page->data;
}

void CBaseVirtMemAlloc::releasePartialLock(TVirtPointer ptr)
{
    for (int i=0; i<MAX_PARTIAL_LOCK_PAGES; ++i)
    {
        if (partialLockPages[i].start == ptr)
        {
            assert(partialLockPages[i].locks);
            --partialLockPages[i].locks;

            if (partialLockPages[i].locks == 0)
            {
                partialLockPages[i].start = 0;
                if (!partialLockPages[i].readOnly)
                {
                    pushRawData(ptr, partialLockPages[i].data, partialLockPages[i].size); // UNDONE: make this more efficient
//                    std::cout << "temp unlock page: " << i << ", " << ptr << "/" << partialLockPages[i].size << std::endl;
                }
                /*else
                    std::cout << "temp unlock page: " << i << ", " << ptr << std::endl;*/
            }
            return;
        }
    }

    // Shouldn't be here
    assert(false);
}

void CBaseVirtMemAlloc::printStats()
{
#ifdef PRINTF_STATS
    printf("------ Memory manager stats ------\n\n");
    printf("Pool: free_pos = %u (%u bytes left)\n\n", poolFreePos, poolSize - poolFreePos);

    TVirtPointer p = START_OFFSET + sizeof(UMemHeader);
    while (p < poolFreePos)
    {
        const UMemHeader *h = getHeaderConst(p);
        printf("  * Addr: %8u; Size: %8u\n", p, h->s.size);
        p += (h->s.size * sizeof(UMemHeader));
        if (!h->s.size || h->s.next < p)
            break;
    }

    printf("\nFree list:\n\n");

    if (freePointer)
    {
        p = freePointer;

        while (1)
        {
            const UMemHeader *h = getHeaderConst(p);
            printf("  * Addr: %8u; Size: %8u; Next: %8u\n", p, h->s.size, h->s.next);

            p = h->s.next;

            if (p == freePointer)
                break;
        }
    }
    else
        printf("Empty\n");

    printf("\n");
#endif
}
