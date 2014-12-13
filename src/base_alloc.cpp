/*
 * Memory allocator based on memmgr, by Eli Bendersky
 * https://github.com/eliben/code-for-blog/tree/master/2008/memmgr
 */

#include "alloc.h"

#include <algorithm>
#include <iomanip>
#include <iostream>

#include <assert.h>
#include <string.h>

#ifdef PRINTF_STATS
#include <stdio.h>
#endif

CBaseVirtMemAlloc *CBaseVirtMemAlloc::instance = 0;

CBaseVirtMemAlloc::CBaseVirtMemAlloc(CBaseVirtMemAlloc::SMemPage *mp, const uint8_t mc, const TVirtSizeType ps,
                                     const TVirtSizeType pgs)
    : memPageList(mp), pageCount(mc), poolSize(ps), pageSize(pgs), freePointer(0), nextPageToSwap(0)
{
    assert(!instance);
    instance = this;

    baseFreeList.s.next = 0;
    baseFreeList.s.size = 0;
    poolFreePos = START_OFFSET + sizeof(UMemHeader);
}

TVirtPointer CBaseVirtMemAlloc::getMem(TVirtSizeType size)
{
    size = std::max(size, (TVirtSizeType)MIN_ALLOC_SIZE); // UNDONE
    const TVirtSizeType totalsize = size * sizeof(UMemHeader);

    if ((poolFreePos + totalsize) <= poolSize)
    {
        std::cout << "new mem at " << poolFreePos << "/" << (poolFreePos + sizeof(UMemHeader)) << std::endl;

        UMemHeader h;
        h.s.size = size;
        h.s.next = 0;
        pushData(poolFreePos, &h, sizeof(UMemHeader));
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
        std::cout << "dirty page\n";
        doWrite(page->pool, page->start, pageSize);
        page->dirty = false;
        page->cleanSkips = 0;
    }
}

void *CBaseVirtMemAlloc::pullData(TVirtPointer p, TVirtSizeType size, bool readonly, bool forcestart)
{
    assert(p < poolSize);

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
            (!forcestart && p >= memPageList[i].start && pend < (memPageList[i].start + pageSize))))
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
            if (memPageList[i].locked)
                continue;

            if (memPageList[i].start != 0)
            {
                const TVirtPointer pageend = memPageList[i].start + pageSize;
                if ((p >= memPageList[i].start && p < pageend) ||
                    (newpageend >= memPageList[i].start && newpageend < pageend))
                {
                    // partial overlap. Clean page and use it later.
                    page = &memPageList[i];
                    syncPage(page);
                    std::cout << "invalidate page " << page->start << std::endl;
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
        std::cout << "getPool switches " << (page - memPageList) << " from: " << page->start << " to " << p << std::endl;

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

void CBaseVirtMemAlloc::pushData(TVirtPointer p, const void *d, TVirtSizeType size)
{
    void *pool = pullData(p, size, false, false);
    memcpy(pool, d, size);
}

CBaseVirtMemAlloc::UMemHeader *CBaseVirtMemAlloc::getHeader(TVirtPointer p)
{
    if (p == BASE_INDEX)
        return &baseFreeList;
    return reinterpret_cast<UMemHeader *>(pullData(p, sizeof(UMemHeader), false, false));
}

const CBaseVirtMemAlloc::UMemHeader *CBaseVirtMemAlloc::getHeaderConst(TVirtPointer p)
{
    if (p == BASE_INDEX)
        return &baseFreeList;
    return reinterpret_cast<UMemHeader *>(pullData(p, sizeof(UMemHeader), true, false));
}

void CBaseVirtMemAlloc::start()
{
    doStart();

    // Initialize ram file with zeros
    const uint8_t zero[16] = { 0 };
    for (TVirtSizeType i=0; i<poolSize; i+=sizeof(zero))
        doWrite(&zero, i, sizeof(zero));
}

TVirtPointer CBaseVirtMemAlloc::alloc(TVirtSizeType size)
{
    const TVirtSizeType quantity = (size + sizeof(UMemHeader) - 1) / sizeof(UMemHeader) + 1;
    TVirtPointer prevp = freePointer;

    // First alloc call, and no free list yet ? Use 'base' for an initial
    // denegerate block of size 0, which points to itself
    if (prevp == 0)
    {
        baseFreeList.s.next = freePointer = prevp = BASE_INDEX;
        baseFreeList.s.size = 0;
    }

    UMemHeader *prevh = getHeader(prevp);
    TVirtPointer p = prevh->s.next;
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

                prevh->s.next = h->s.next;
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
        prevh = h;
        p = h->s.next;
    }
}

// Scans the free list, starting at freePointer, looking the the place to insert the
// free block. This is either between two existing blocks or at the end of the
// list. In any case, if the block being freed is adjacent to either neighbor,
// the adjacent blocks are combined.
void CBaseVirtMemAlloc::free(TVirtPointer ptr)
{
    // acquire pointer to block header
    const TVirtPointer hdrptr = ptr - sizeof(UMemHeader);
    UMemHeader *header = getHeader(hdrptr);

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

    UMemHeader *h = getHeader(p); // now get a writable copy (so searching won't mark every page dirty)

    // Try to combine with the higher neighbor
    if ((hdrptr + header->s.size) == h->s.next)
    {
        const UMemHeader *nexth = getHeaderConst(h->s.next);
        header->s.size += nexth->s.size;
        header->s.next = nexth->s.next;
    }
    else
        header->s.next = h->s.next;

    // Try to combine with the lower neighbor
    if (p + h->s.size == hdrptr)
    {
        h->s.size += header->s.size;
        h->s.next = header->s.next;
    }
    else
        h->s.next = hdrptr;

    assert(p);
    freePointer = p;
}

void *CBaseVirtMemAlloc::lock(TVirtPointer p, bool ro)
{
    void *ret = pullData(p, pageSize, ro, true);

    // Find which page was used and mark it as locked
    for (uint8_t i=0; i<pageCount; ++i)
    {
        if (memPageList[i].start == p)
            memPageList[i].locked = true;
    }

    return ret;
}

void CBaseVirtMemAlloc::unlock(TVirtPointer p)
{
    for (uint8_t i=0; i<pageCount; ++i)
    {
        if (memPageList[i].start == p)
        {
            assert(memPageList[i].locked);
            memPageList[i].locked = false;
        }
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
