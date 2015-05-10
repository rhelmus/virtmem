/*
 * Memory allocator based on memmgr, by Eli Bendersky
 * https://github.com/eliben/code-for-blog/tree/master/2008/memmgr
 */

#include "base_alloc.h"
#include "utils.h"

//#include <algorithm>
//#include <iomanip>
//#include <iostream>

#include <string.h>

//#define PRINTF_STATS

#ifdef PRINTF_STATS
#include <stdio.h>
#endif

#undef min
#undef max

void CBaseVirtMemAlloc::initPages(SPageInfo *info, SLockPage *pages, uint8_t *pool, uint8_t pcount, TVirtPageSize psize)
{
    info->pages = pages;
    info->count = pcount;
    info->size = psize;
    info->freeIndex = 0;
    info->lockedIndex = -1;

    for (uint8_t i=0; i<pcount; ++i)
    {
#ifndef NVALGRIND
        const int start = i * (psize + valgrindPad * 2);
        info->pages[i].pool = &pool[start + valgrindPad];
        VALGRIND_MAKE_MEM_NOACCESS(&pool[start], valgrindPad); VALGRIND_MAKE_MEM_NOACCESS(&pool[start + psize + valgrindPad], valgrindPad);
#else
        info->pages[i].pool = &pool[i * psize];
#endif
    }
}

TVirtPointer CBaseVirtMemAlloc::getMem(TVirtPtrSize size)
{
    size = private_utils::maximal(size, (TVirtPtrSize)MIN_ALLOC_SIZE);
    const TVirtPtrSize totalsize = size * sizeof(UMemHeader);

    if ((poolFreePos + totalsize) <= poolSize)
    {
//        std::cout << "new mem at " << poolFreePos << "/" << (poolFreePos + sizeof(UMemHeader)) << std::endl;

        UMemHeader h;
        h.s.size = size;
        h.s.next = 0;
        write(poolFreePos, &h, sizeof(UMemHeader));
#ifdef VIRTMEM_TRACE_STATS
        // HACK: increase here to balance the subtraction by free()
        memUsed += totalsize;
#endif
        free(poolFreePos + sizeof(UMemHeader));
        poolFreePos += totalsize;
    }
    else
        return 0;

    return freePointer;
}

void CBaseVirtMemAlloc::syncBigPage(SLockPage *page)
{
    ASSERT(page->start != 0);

    if (page->dirty)
    {
//        std::cout << "dirty page\n";
        const TVirtPageSize wrsize = private_utils::minimal((poolSize - page->start), (TVirtPtrSize)bigPages.size);
        doWrite(page->pool, page->start, wrsize);
        page->dirty = false;
        page->cleanSkips = 0;
#ifdef VIRTMEM_TRACE_STATS
        ++bigPageWrites;
        bytesWritten += wrsize;
#endif
    }
}

void CBaseVirtMemAlloc::copyRawData(void *dest, TVirtPointer p, TVirtPtrSize size)
{
    // First check if we should copy data from loaded big pages
    // Note that the size of these pages are never smaller than the copy size,
    // so it is impossible that more than two pages overlap

    for (int8_t i=bigPages.freeIndex; i!=-1 && size; i=bigPages.pages[i].next)
    {
        if (!bigPages.pages[i].start)
            continue;

        const TVirtPointer pageend = bigPages.pages[i].start + bigPages.size;
        if (p >= bigPages.pages[i].start && p < pageend) // start address within this page?
        {
            const TVirtPtrSize offset = p - bigPages.pages[i].start;
            const TVirtPtrSize copysize = private_utils::minimal(size, bigPages.pages[i].size - offset);
            memcpy(dest, bigPages.pages[i].pool + offset, copysize);

            // move start to end of this page
            dest = (uint8_t *)dest + copysize;
            p += copysize;
            size -= copysize;
        }
        // end overlaps?
        else if (p < bigPages.pages[i].start && (p + size) > bigPages.pages[i].start)
        {
            const TVirtPtrSize offset = bigPages.pages[i].start - p;
            const TVirtPtrSize copysize = private_utils::minimal(size - offset, (TVirtPtrSize)bigPages.pages[i].size);
            memcpy((uint8_t *)dest + offset, bigPages.pages[i].pool, copysize);
            size = offset;
        }
    }

    if (size > 0)
    {
        // read in rest of the data
        doRead(dest, p, size);
#ifdef VIRTMEM_TRACE_STATS
        bytesRead += size;
#endif
    }
}

// This function is the reverse of copyRawData()
void CBaseVirtMemAlloc::saveRawData(void *src, TVirtPointer p, TVirtPtrSize size)
{
    for (int8_t i=bigPages.freeIndex; i!=-1 && size; i=bigPages.pages[i].next)
    {
        if (!bigPages.pages[i].start)
            continue;

        const TVirtPointer pageend = bigPages.pages[i].start + bigPages.size;
        if (p >= bigPages.pages[i].start && p < pageend) // start address within this page?
        {
            const TVirtPtrSize offset = p - bigPages.pages[i].start;
            const TVirtPtrSize copysize = private_utils::minimal(size, bigPages.pages[i].size - offset);

            // only copy data if regular page is already dirty or data changed
            if (bigPages.pages[i].dirty || memcmp(bigPages.pages[i].pool + offset, src, copysize) != 0)
            {
                memcpy(bigPages.pages[i].pool + offset, src, copysize);
                bigPages.pages[i].dirty = true;
            }

            // move start to end of this page
            src = (uint8_t *)src + copysize;
            p += copysize;
            size -= copysize;
        }
        // end overlaps?
        else if (p < bigPages.pages[i].start && (p + size) > bigPages.pages[i].start)
        {
            const TVirtPtrSize offset = bigPages.pages[i].start - p;
            const TVirtPtrSize copysize = private_utils::minimal(size - offset, (TVirtPtrSize)bigPages.pages[i].size);

            // only copy data if regular page is already dirty or data changed
            if (bigPages.pages[i].dirty || memcmp(bigPages.pages[i].pool, (uint8_t *)src + offset, copysize) != 0)
            {
                memcpy(bigPages.pages[i].pool, (uint8_t *)src + offset, copysize);
                bigPages.pages[i].dirty = true;
            }

            size = offset;
        }
    }

    if (size > 0)
    {
        // read in rest of the data
        doWrite(src, p, size);
#ifdef VIRTMEM_TRACE_STATS
        bytesWritten += size;
#endif
    }
}

void *CBaseVirtMemAlloc::pullRawData(TVirtPointer p, TVirtPtrSize size, bool readonly, bool forcestart)
{
    ASSERT(p && p < poolSize);

    /* If a page is found which fits within the pointer: take that and abort search; no overlap can occur
     * If a page partially overlaps take that, as it has to be cleared out anyway. Keep searching for
     * other overlapping pages.
     * Otherwise if an empty page is found use it but keep searching for the above.
     * Otherwise if a 'clean' page is found use that but keep searching for the above.
     * Otherwise look for dirty pages in a FIFO way. */

    int8_t pageindex = -1;
    enum { STATE_GOTFULL, STATE_GOTPARTIAL, STATE_GOTEMPTY, STATE_GOTCLEAN, STATE_GOTDIRTY, STATE_GOTNONE } pagefindstate = STATE_GOTNONE;

    // Start by looking for fitting pages, the ideal situation
    if ((pageindex = findFreePage(&bigPages, p, size, forcestart)) != -1)
        pagefindstate = STATE_GOTFULL;
    else
    {
        const TVirtPointer newpageend = p + bigPages.size;

        for (int8_t i=bigPages.freeIndex; i!=-1; i=bigPages.pages[i].next)
        {
            if (bigPages.pages[i].start != 0)
            {
                const TVirtPointer pageend = bigPages.pages[i].start + bigPages.size;
                if ((p >= bigPages.pages[i].start && p < pageend) ||
                    (newpageend >= bigPages.pages[i].start && newpageend <= pageend))
                {
                    pageindex = i;
                    syncBigPage(&bigPages.pages[pageindex]);
                    bigPages.pages[i].start = 0; // invalidate
                    pagefindstate = STATE_GOTPARTIAL;
                }
            }
            else if (pagefindstate != STATE_GOTPARTIAL)
            {
                pageindex = i;
                pagefindstate = STATE_GOTEMPTY;
            }

            if (pagefindstate > STATE_GOTCLEAN)
            {
                if (!bigPages.pages[i].dirty || (++bigPages.pages[i].cleanSkips) >= PAGE_MAX_CLEAN_SKIPS)
                {
                    pageindex = i;
                    pagefindstate = STATE_GOTCLEAN;
                }
                else if (pagefindstate != STATE_GOTDIRTY && i == nextPageToSwap)
                {
                    pageindex = i;
                    pagefindstate = STATE_GOTDIRTY;
                }
            }
        }
    }

    // 'pageindex' should now point to page which is within or closest to pointer range
    ASSERT(pageindex != -1);

    // do we need to swap a page?
    if (pagefindstate != STATE_GOTFULL)
    {
//        std::cout << "getPool switches " << (page - memPageList) << " from: " << page->start << " to " << p << std::endl;

        if (bigPages.pages[pageindex].start != 0)
            syncBigPage(&bigPages.pages[pageindex]);

        if (pagefindstate == STATE_GOTDIRTY)
        {
            nextPageToSwap = bigPages.pages[pageindex].next;
            if (nextPageToSwap == -1)
                nextPageToSwap = bigPages.freeIndex;
        }
        else
            nextPageToSwap = bigPages.freeIndex;

        // Load in page
        if (/*!forcestart*/false) // check alignment UNDONE
        {
            const TVirtPointer alignp = p - (p & (sizeof(TAlign) - 1));
            if ((alignp + bigPages.size) >= (p + size))
                bigPages.pages[pageindex].start = alignp;
            else
                bigPages.pages[pageindex].start = p;
        }
        else
            bigPages.pages[pageindex].start = p;

//        std::cout << "start: " << bigPages.pages[pageindex].start <<"/" << p << std::endl;

        const TVirtPageSize rdsize = private_utils::minimal((poolSize - bigPages.pages[pageindex].start), (TVirtPtrSize)bigPages.size);
        doRead(bigPages.pages[pageindex].pool, bigPages.pages[pageindex].start, rdsize);

#ifdef VIRTMEM_TRACE_STATS
        ++bigPageReads;
        bytesRead += rdsize;
#endif
    }

    if (!readonly)
        bigPages.pages[pageindex].dirty = true;

    ASSERT(p >= bigPages.pages[pageindex].start);

    /*if (size >= sizeof(TAlign) && (bigPages.pages[pageindex].start & (sizeof(TAlign)-1)))
        std::cout << "unaligned pull: " << p << "/" << (p - bigPages.pages[pageindex].start) << std::endl;*/

//    std::cout << "pullRawData returning: " << p << "/" << (int)pageindex << "/" << bigPages.pages[pageindex].start << "/" << (void *)&((uint8_t *)bigPages.pages[pageindex].pool)[p - bigPages.pages[pageindex].start] << std::endl;

    return &((uint8_t *)bigPages.pages[pageindex].pool)[p - bigPages.pages[pageindex].start];
}

void CBaseVirtMemAlloc::pushRawData(TVirtPointer p, const void *d, TVirtPtrSize size)
{
    void *pool = pullRawData(p, size, false, false);
    memcpy(pool, d, size);
}

const CBaseVirtMemAlloc::UMemHeader *CBaseVirtMemAlloc::getHeaderConst(TVirtPointer p)
{
    if (p == BASE_INDEX)
        return &baseFreeList;
    return reinterpret_cast<UMemHeader *>(read(p, sizeof(UMemHeader)));
}

void CBaseVirtMemAlloc::updateHeader(TVirtPointer p, UMemHeader *h)
{
    if (p == BASE_INDEX)
        memcpy(&baseFreeList, h, sizeof(UMemHeader));
    else
        write(p, h, sizeof(UMemHeader));
}

int8_t CBaseVirtMemAlloc::findFreePage(CBaseVirtMemAlloc::SPageInfo *pinfo, TVirtPointer p, TVirtPtrSize size, bool atstart)
{
    const TVirtPointer pend = p + size;
    for (int8_t i=pinfo->freeIndex; i!=-1; i=pinfo->pages[i].next)
    {
        if (pinfo->pages[i].start != 0 && ((atstart && pinfo->pages[i].start == p) ||
            (!atstart && p >= pinfo->pages[i].start && pend <= (pinfo->pages[i].start + pinfo->pages[i].size))))
            return i;
    }

    return -1;
}

int8_t CBaseVirtMemAlloc::findUnusedLockedPage(SPageInfo *pinfo)
{
    for (int8_t i=pinfo->lockedIndex; i!=-1; i=pinfo->pages[i].next)
    {
        if (pinfo->pages[i].locks == 0)
            return i;
    }

    return -1;
}

void CBaseVirtMemAlloc::syncLockedPage(CBaseVirtMemAlloc::SLockPage *page)
{
    ASSERT(page->start != 0);
    if (page->dirty)
    {
#if 1
        saveRawData(page->pool, page->start, page->size);
#else
        void *data = pullRawData(page->start, page->size, true, false);
        const int8_t pageindex = findFreePage(&bigPages, page->start, page->size, false);
        ASSERT(pageindex != -1);

        // only copy data if regular page is already dirty or data changed
        if (bigPages.pages[pageindex].dirty || memcmp(data, page->pool, page->size) != 0)
        {
            memcpy(data, page->pool, page->size);
            bigPages.pages[pageindex].dirty = true;
        }

        // UNDONE: unset dirty?
#endif
    }
}

int8_t CBaseVirtMemAlloc::lockPage(SPageInfo *pinfo, TVirtPointer ptr, TVirtPageSize size)
{
    int8_t index;

    if (pinfo == &bigPages)
    {
        // read in data and lock the page that was used
        // NOTE: set readonly here, the eventual ro flag should be set afterwards
        pullRawData(ptr, size, true, true);
        index = findFreePage(pinfo, ptr, size, true);
        if (size < pinfo->size)
            syncBigPage(&bigPages.pages[index]); // synchronize if there is data outside lock range
    }
    else
        index = pinfo->freeIndex;

    if (index == pinfo->freeIndex)
        pinfo->freeIndex = pinfo->pages[pinfo->freeIndex].next;
    else
    {
        // find previous
        int8_t previ = pinfo->freeIndex;
        for (; index!=pinfo->pages[previ].next; previ=pinfo->pages[previ].next)
            ;
        pinfo->pages[previ].next = pinfo->pages[index].next;
    }

    if (pinfo == &bigPages && nextPageToSwap == index)
        nextPageToSwap = pinfo->freeIndex; // locked page, can't swap it anymore

    pinfo->pages[index].next = pinfo->lockedIndex;
    pinfo->lockedIndex = index;

    return index;
}

int8_t CBaseVirtMemAlloc::freeLockedPage(CBaseVirtMemAlloc::SPageInfo *pinfo, int8_t index)
{
    if (pinfo != &bigPages)
        syncLockedPage(&pinfo->pages[index]);
    else if (pinfo->pages[index].size < pinfo->size /*|| (pinfo->pages[index].start & (sizeof(TAlign)-1)) != 0*/)
    {
        // only synchronize shrunk big pages as they cannot be used for regular IO or unaligned pages
        syncLockedPage(&pinfo->pages[index]);
        // restore as regular unused free page
        pinfo->pages[index].start = 0;
        pinfo->pages[index].size = pinfo->size;
    }

    const int8_t ret = pinfo->pages[index].next;

    if (index == pinfo->lockedIndex)
        pinfo->lockedIndex = pinfo->pages[index].next;
    else
    {
        int prevind = pinfo->lockedIndex;
        for (; pinfo->pages[prevind].next != index; prevind=pinfo->pages[prevind].next)
            ;
        pinfo->pages[prevind].next = pinfo->pages[index].next;
    }
    pinfo->pages[index].next = pinfo->freeIndex;
    pinfo->freeIndex = index;
//    printf("freeing page %d - free/used: %d/%d\n", index, pinfo->freeIndex, pinfo->usedIndex);

    if (pinfo == &bigPages && nextPageToSwap == -1)
        nextPageToSwap = pinfo->freeIndex;

    pinfo->pages[index].locks = 0;

    return ret;
}

int8_t CBaseVirtMemAlloc::findLockedPage(SPageInfo *pinfo, TVirtPointer p)
{
    for (int8_t i=pinfo->lockedIndex; i!=-1; i=pinfo->pages[i].next)
    {
        if (p >= pinfo->pages[i].start && (p - pinfo->pages[i].start) < pinfo->pages[i].size)
            return i;
    }

    return -1;
}

CBaseVirtMemAlloc::SLockPage *CBaseVirtMemAlloc::findLockedPage(TVirtPointer p)
{
    int8_t index = findLockedPage(&smallPages, p);
    if (index != -1)
        return &smallPages.pages[index];

    index = findLockedPage(&mediumPages, p);
    if (index != -1)
        return &mediumPages.pages[index];

    index = findLockedPage(&bigPages, p);
    if (index != -1)
        return &bigPages.pages[index];

    return 0;
}

uint8_t CBaseVirtMemAlloc::getUnlockedPages(const SPageInfo *pinfo) const
{
    uint8_t ret = 0;

    for (int8_t i=pinfo->freeIndex; i!=-1; i=pinfo->pages[i].next)
        ++ret;

    // also include unused locked pages
    for (int8_t i=pinfo->lockedIndex; i!=-1; i=pinfo->pages[i].next)
    {
        if (pinfo->pages[i].locks == 0)
            ++ret;
    }

    return ret;
}

void CBaseVirtMemAlloc::writeZeros(uint32_t start, uint32_t n)
{
    ASSERT(bigPages.pages[0].start == 0);

    // Use zeroed page as buffer
    memset(bigPages.pages[0].pool, 0, bigPages.size);
    for (TVirtPtrSize i=0; i<n; i+=bigPages.size)
        doWrite(bigPages.pages[0].pool, start + i, private_utils::minimal(n - i, (TVirtPtrSize)bigPages.size));
}

void CBaseVirtMemAlloc::start()
{
    freePointer = 0;
    nextPageToSwap = 0;
    baseFreeList.s.next = 0;
    baseFreeList.s.size = 0;
    poolFreePos = START_OFFSET + sizeof(UMemHeader);
#ifdef VIRTMEM_TRACE_STATS
    memUsed = maxMemUsed = 0;
    bigPageReads = bigPageWrites = bytesRead = bytesWritten = 0;
#endif

    SPageInfo *plist[3] = { &smallPages, &mediumPages, &bigPages };
    for (uint8_t pindex=0; pindex<3; ++pindex)
    {
        plist[pindex]->freeIndex = 0;
        plist[pindex]->lockedIndex = -1;

        for (uint8_t i=0; i<plist[pindex]->count; ++i)
        {
            if (i == (plist[pindex]->count - 1))
                plist[pindex]->pages[i].next = -1;
            else
                plist[pindex]->pages[i].next = i + 1;

            if (plist[pindex] == &bigPages)
                plist[pindex]->pages[i].size = plist[pindex]->size;
            plist[pindex]->pages[i].start = 0;
            plist[pindex]->pages[i].locks = 0;
            plist[pindex]->pages[i].cleanSkips = 0;
            plist[pindex]->pages[i].dirty = false;
        }
    }

    doStart();
}

void CBaseVirtMemAlloc::stop()
{
    doStop();
}

TVirtPointer CBaseVirtMemAlloc::alloc(TVirtPtrSize size)
{
    const TVirtPtrSize quantity = (size + sizeof(UMemHeader) - 1) / sizeof(UMemHeader) + 1;
    TVirtPointer prevp = freePointer;

    ASSERT(size && quantity);

    // First alloc call, and no free list yet ? Use 'base' for an initial
    // denegerate block of size 0, which points to itself
    if (prevp == 0)
    {
        baseFreeList.s.next = freePointer = prevp = BASE_INDEX;
        baseFreeList.s.size = 0;
    }

    TVirtPointer p = getHeaderConst(prevp)->s.next;
    while (true)
    {
        const UMemHeader *consth = getHeaderConst(p);

        // big enough ?
        if (consth->s.size >= quantity)
        {
#ifdef VIRTMEM_TRACE_STATS
            memUsed += (quantity * sizeof(UMemHeader));
            maxMemUsed = private_utils::maximal(maxMemUsed, memUsed);
#endif

            // exactly ?
            if (consth->s.size == quantity)
            {
                // just eliminate this block from the free list by pointing
                // its prev's next to its next
                TVirtPointer next = consth->s.next;
//                UMemHeader prevh = *getHeaderConst(prevp); // UNDONE: this seems to sometimes crash while memcpy doesn't?!?
                UMemHeader prevh;
                memcpy(&prevh, getHeaderConst(prevp), sizeof(UMemHeader));
                prevh.s.next = next;
                updateHeader(prevp, &prevh);
                // NOTE: getHeaderConst might invalidate h from here ----
            }
            else // too big
            {
                //UMemHeader h = *consth;
                UMemHeader h;
                memcpy(&h, consth, sizeof(UMemHeader));
                h.s.size -= quantity;
                updateHeader(p, &h);
                p += (h.s.size * sizeof(UMemHeader));
//                h = *getHeaderConst(p);
                memcpy(&h, getHeaderConst(p), sizeof(UMemHeader));
                h.s.size = quantity;
                updateHeader(p, &h);
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
//                std::cout << "!! Memory allocation failed !!\n";
                ASSERT(false);
                return 0;
            }
            consth = getHeaderConst(p);
        }

        prevp = p;
        p = consth->s.next;
        ASSERT(p);
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

#ifdef VIRTMEM_TRACE_STATS
    memUsed -= (statheader.s.size * sizeof(UMemHeader));
#endif

    // Find the correct place to place the block in (the free list is sorted by
    // address, increasing order)
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

    // Try to combine with the lower neighbor
    if (p + stath.s.size == hdrptr)
    {
        stath.s.size += statheader.s.size;
        stath.s.next = statheader.s.next;
    }
    else
        stath.s.next = hdrptr;

    updateHeader(p, &stath);

    ASSERT(p);
    ASSERT(stath.s.next);
    freePointer = p;
}

void *CBaseVirtMemAlloc::read(TVirtPointer p, TVirtPtrSize size)
{
    SPageInfo *plist[3] = { &smallPages, &mediumPages, &bigPages };
    const TVirtPointer pend = p + size;

    for (uint8_t pindex=0; pindex<3; ++pindex)
    {
        for (int8_t i=plist[pindex]->lockedIndex; i!=-1; i=plist[pindex]->pages[i].next)
        {
            const bool beginoverlaps = (p >= plist[pindex]->pages[i].start &&
                                      p < (plist[pindex]->pages[i].start + plist[pindex]->pages[i].size));
            const bool endoverlaps = (p < plist[pindex]->pages[i].start && pend > plist[pindex]->pages[i].start);

            if (beginoverlaps)
            {
                const TVirtPointer offset = p - plist[pindex]->pages[i].start;
                // data fits in this page?
                if ((offset + size) <= plist[pindex]->pages[i].size)
                {
        //            std::cout << "using temp lock page " << (int)(pageindex) << ", " << p << std::endl;
                    return (char *)plist[pindex]->pages[i].pool + offset;
                }
            }

            if (beginoverlaps || endoverlaps)
            {
                // only fits partially... mirror data to normal page so a continuous block can be returned
                pushRawData(plist[pindex]->pages[i].start, plist[pindex]->pages[i].pool,
                            plist[pindex]->pages[i].size); // UNDONE: partial copy, check dirty?

//                std::cout << "mirrored partial page: " << (int)pindex << "/" << (int)(i) << std::endl;
            }
        }
    }

    // not in or too big for partial page, use regular paged memory
    return pullRawData(p, size, true, false);
}

void CBaseVirtMemAlloc::write(TVirtPointer p, const void *d, TVirtPtrSize size)
{
    SPageInfo *plist[3] = { &smallPages, &mediumPages, &bigPages };
    const TVirtPointer pend = p + size;

    for (uint8_t pindex=0; pindex<3; ++pindex)
    {
        for (int8_t i=plist[pindex]->lockedIndex; i!=-1; i=plist[pindex]->pages[i].next)
        {
            const bool beginoverlaps = (p >= plist[pindex]->pages[i].start &&
                                      p < (plist[pindex]->pages[i].start + plist[pindex]->pages[i].size));
            const bool endoverlaps = (p < plist[pindex]->pages[i].start && pend > plist[pindex]->pages[i].start);

            if (!plist[pindex]->pages[i].dirty && (beginoverlaps || endoverlaps))
                plist[pindex]->pages[i].dirty = true;

            if (beginoverlaps)
            {
                const TVirtPointer offset = p - plist[pindex]->pages[i].start;
                // data fits in this page?
                if ((offset + size) <= plist[pindex]->pages[i].size)
                {
                    memcpy((char *)plist[pindex]->pages[i].pool + offset, d, size);
                    return;
                }
                else
                {
                    // partial fit (data too large), copy stuff that fits in page
                    memcpy((char *)plist[pindex]->pages[i].pool + offset, d, plist[pindex]->pages[i].size - offset);
                }
            }
            else if (endoverlaps)
            {
                // partial fit (data starts before), copy stuff that fits in page
                const TVirtPointer offset = plist[pindex]->pages[i].start - p;
                memcpy((char *)plist[pindex]->pages[i].pool, (uint8_t *)d + offset, size - offset);
            }
        }
    }

    // data was either not or partially in a lock if we are here
    // UNDONE: partial copy if data was partially in locks?
    pushRawData(p, d, size);
}

void CBaseVirtMemAlloc::flush()
{
    // UNDONE: also flush locked pages?
    for (int8_t i=bigPages.freeIndex; i!=-1; i=bigPages.pages[i].next)
    {
        if (bigPages.pages[i].start != 0)
            syncBigPage(&bigPages.pages[i]);
    }
}

void CBaseVirtMemAlloc::clearPages()
{
    // wipe all pages
    for (int8_t i=bigPages.freeIndex; i!=-1; i=bigPages.pages[i].next)
    {
        if (bigPages.pages[i].start != 0)
        {
            syncBigPage(&bigPages.pages[i]);
            bigPages.pages[i].start = 0;
        }
    }
}

uint8_t CBaseVirtMemAlloc::getFreeBigPages() const
{
    uint8_t ret = 0;

    for (int8_t i=bigPages.freeIndex; i!=-1; i=bigPages.pages[i].next)
    {
        if (bigPages.pages[i].start == 0)
            ++ret;
    }

    return ret;
}

void *CBaseVirtMemAlloc::makeDataLock(TVirtPointer ptr, TVirtPageSize size, bool ro)
{
    ASSERT(ptr != 0);
    ASSERT(size <= bigPages.size);

    SPageInfo *pinfo, *secpinfo = 0;
    if (size <= smallPages.size)
        pinfo = &smallPages;
    else if (size <= mediumPages.size)
        pinfo = &mediumPages;
    else
        pinfo = &bigPages;

//    std::cout << "request lock: " << ptr << "/" << size << "/" << pinfo->size << std::endl;

    SPageInfo *plist[3] = { &smallPages, &mediumPages, &bigPages };
    int8_t pageindex = -1, oldlockindex = -1, secoldlockindex = -1;
    bool fixbeginningoverlap = false, done = false, shrunk = false;
    for (uint8_t pindex=0; pindex<3 && !done; ++pindex)
    {
        for (int8_t i=plist[pindex]->lockedIndex; i!=-1;)
        {
            // already there?
            if (plist[pindex]->pages[i].start == ptr)
            {
                if (pinfo != plist[pindex])
                {
                    if (plist[pindex]->pages[i].locks == 0)
                    {
                        // lock was previously created with different size, remove it
                        i = freeLockedPage(plist[pindex], i);
                        continue;
                    }
                    else // still locked, using a different, presumably larger, page size
                    {
//                        ASSERT(plist[pindex]->size > pinfo->size);
                        // size smaller than asked?
                        // this may happen if lock was resized and put in smaller page
                        if (plist[pindex]->size < pinfo->size)
                            size = private_utils::minimal(size, plist[pindex]->size);

                        pinfo = plist[pindex];
//                        std::cout << "use secondary locked page\n";

                        // ...fallthrough...
                    }
                }
                else if (plist[pindex]->pages[i].size > size) // requested size smaller than page?
                {
                    ASSERT(plist[pindex]->pages[i].locks == 0);
                    // write excess data
#if 1
                    saveRawData(plist[pindex]->pages[i].pool + size, plist[pindex]->pages[i].start + size,
                                plist[pindex]->pages[i].size - size);
#else
                    pushRawData(plist[pindex]->pages[i].start + size, plist[pindex]->pages[i].pool + size,
                                plist[pindex]->pages[i].size - size);
#endif
                    plist[pindex]->pages[i].size = size; // shrink page
                    // NOTE: we don't have to check for overlap since we only shrunk the page
                }

                pageindex = i;
                if (plist[pindex]->pages[i].size == size)
                {
                    done = true;
                    break; // we're finished searching, no need to look for overlapping pages as size is already OK
                }
            }
            else
            {
                const bool endoverlaps = (ptr < plist[pindex]->pages[i].start && (ptr + size) > plist[pindex]->pages[i].start);
                const bool beginoverlaps = (ptr > plist[pindex]->pages[i].start && ptr < (plist[pindex]->pages[i].start + plist[pindex]->pages[i].size));

                if (plist[pindex]->pages[i].locks)
                {
                    if (endoverlaps)
                    {
                        size = plist[pindex]->pages[i].start - ptr; // Shrink so it fits
                        shrunk = true;
                    }
                    else if (beginoverlaps)
                        fixbeginningoverlap = true;
                }
                else
                {
                    if (endoverlaps || beginoverlaps)
                    {
                        // don't bother with unused pages. It is possible that they are never be used again, meaning they will always be in the way
                        i = freeLockedPage(plist[pindex], i);
                        continue;
                    }

                    if (oldlockindex == -1)
                    {
                        if (pinfo == plist[pindex])
                            oldlockindex = i;
                        else if (secoldlockindex == -1 && (pinfo->size < plist[pindex]->size))
                        {
                            // also keep record of bigger pages in case nothing is available in the preferred list
                            secoldlockindex = i;
                            secpinfo = plist[pindex];
                        }
                    }
                }
            }

            i = plist[pindex]->pages[i].next;
        }
    }

    ASSERT(pageindex == -1 || size >= pinfo->pages[pageindex].size);
//    ASSERT(pageindex == -1 || size == pinfo->pages[pageindex].size || !pinfo->pages[pageindex].locks); // UNDONE
    ASSERT(pageindex == -1 || !fixbeginningoverlap);

    // does it fit in a smaller page now?
    // NOTE: moving it to a smaller lock page means that it cannot be re-used, so only do this for big pages
    // as they are relative 'precious'.
    if (shrunk && size <= mediumPages.size && pinfo == &bigPages && (pageindex == -1 || pinfo->pages[pageindex].locks == 0))
    {
        SPageInfo *oldpinfo = pinfo;

        // first try a small page
        if (size <= smallPages.size)
        {
            if (smallPages.freeIndex != -1)
                pinfo = &smallPages;
            else
            {
                const int8_t index = findUnusedLockedPage(&smallPages);
                if (index != -1)
                {
                    pinfo = &smallPages;
                    oldlockindex = index;
                }
            }
        }

        if (oldpinfo == pinfo) // medium page then?
        {
            if (mediumPages.freeIndex != -1)
                pinfo = &mediumPages;
            else
            {
                const int8_t index = findUnusedLockedPage(&mediumPages);
                if (index != -1)
                {
                    pinfo = &mediumPages;
                    oldlockindex = index;
                }
            }
        }

        /*if (oldpinfo != pinfo)
            std::cout << "Moved new lock " << ptr << " from " << oldpinfo->size << " to " << pinfo->size << std::endl;*/

        if (pinfo != oldpinfo && pageindex != -1)
        {
            // remove old
            freeLockedPage(oldpinfo, pageindex);
            pageindex = -1;
        }
    }

    // make new or reuse old lock?
    if (pageindex == -1)
    {
        if (pinfo->freeIndex == -1 && oldlockindex == -1) // no space left in chosen page size?
        {
            // space left in bigger page?
            if (pinfo->size < mediumPages.size && mediumPages.freeIndex != -1)
                pinfo = &mediumPages;
            else if (pinfo->size < bigPages.size && bigPages.freeIndex != -1)
                pinfo = &bigPages;
        }

        TVirtPageSize copyoffset = 0;

        if (pinfo->freeIndex != -1)
        {
            if (pinfo == &bigPages)
                copyoffset = size; // no need to copy big pages as they are already copied in lockPage()
            pageindex = lockPage(pinfo, ptr, size);
        }
        else
        {
            if (oldlockindex == -1 && secoldlockindex != -1)
            {
//                std::cout << "use secondary old page\n";
                pinfo = secpinfo;
                oldlockindex = secoldlockindex;
            }

            if (oldlockindex != -1)
            {
                syncLockedPage(&pinfo->pages[oldlockindex]);
                pinfo->pages[oldlockindex].dirty = false;
                pageindex = oldlockindex;
            }
            else
            {
                ASSERT(false);
                return 0; // no space left
            }
        }

        bool fixed = false; // UNDONE
        if (fixbeginningoverlap)
        {
            // check pages that overlap in the beginning and resize them if necessary
            // NOTE: we couldn't do this earlier since it was unknown which data is going to be used
            for (uint8_t pindex=0; pindex<3; ++pindex)
            {
                for (int8_t i=plist[pindex]->lockedIndex; i!=-1; i=plist[pindex]->pages[i].next)
                {
                    if ((i != pageindex || plist[pindex] != pinfo) && ptr > plist[pindex]->pages[i].start &&
                        ptr < (plist[pindex]->pages[i].start + plist[pindex]->pages[i].size))
                    {
                        ASSERT(!fixed);

                        // copy their overlapping data (assume this is the most up to date)
                        const TVirtPtrSize offsetold = ptr - plist[pindex]->pages[i].start;
                        const TVirtPageSize copysize = private_utils::minimal((TVirtPageSize)(plist[pindex]->pages[i].size - offsetold), size);
                        memcpy(pinfo->pages[pageindex].pool, (char *)plist[pindex]->pages[i].pool + offsetold, copysize);
                        copyoffset = private_utils::maximal(copyoffset, copysize); // NOTE: take max, copyoffset might have been set earlier
                        plist[pindex]->pages[i].size = offsetold; // shrink other so this one fits
                        fixed = true;
                    }
                }
            }
        }

        // copy (rest of) data
        if (copyoffset < size)
        {
#if 0
            const TVirtPtrSize copysize = size - copyoffset;
            memcpy(pinfo->pages[pageindex].pool + copyoffset, pullRawData(ptr + copyoffset, copysize, true, false), copysize); // UNDONE: make this more efficient
#else
            copyRawData(pinfo->pages[pageindex].pool + copyoffset, ptr + copyoffset, size - copyoffset);
#endif
        }

        pinfo->pages[pageindex].start = ptr;
    }
    else
    {
        // size increased? (this can happen when either this page was used for a smaller data type, or other pages don't overlap anymore
        if (size > pinfo->pages[pageindex].size)
        {
            const TVirtPtrSize offset = pinfo->pages[pageindex].size;
#if 0
            const TVirtPtrSize copysize = size - offset;
            // copy excess data to page
            // UNDONE: make this more efficient
            memcpy(pinfo->pages[pageindex].pool + offset, pullRawData(ptr + offset, copysize, true, false), copysize);
#else
            copyRawData(pinfo->pages[pageindex].pool + offset, ptr + offset, size - offset);
#endif
        }
    }

    /*
    if (pinfo->pages[pageindex].locks == 0)
        printf("%s page %d (%d) - free/used: %d/%d\n", (pageindex == oldlockindex) ? "re-using" : "locking", pageindex, ptr, pinfo->freeIndex, pinfo->usedIndex);
    else
        printf("using existing page %d (%d) - free/used: %d/%d\n", pageindex, ptr, pinfo->freeIndex, pinfo->usedIndex);*/

    if (!pinfo->pages[pageindex].dirty)
        pinfo->pages[pageindex].dirty = !ro;

    ++pinfo->pages[pageindex].locks;
    pinfo->pages[pageindex].size = size;
//    std::cout << "temp lock page: " << (int)pageindex << ", " << ptr << "/" << size << "/" << pinfo->size << std::endl;
    ASSERT(size <= pinfo->size);
    return pinfo->pages[pageindex].pool;
}

// makes a lock that will not resize existing locks. If ptr is in an existing lock, this lock will be used.
// Otherwise a new lock is created with an apropiate size to avoid overlap
void *CBaseVirtMemAlloc::makeFittingLock(TVirtPointer ptr, TVirtPageSize &size, bool ro)
{
    ASSERT(ptr != 0);

    size = private_utils::minimal(size, bigPages.size);

    SPageInfo *plist[3] = { &smallPages, &mediumPages, &bigPages };
    int8_t unusedlist[3] = { -1, -1, -1 };
    int8_t plistindex = -1, pageindex = -1;
    bool done = false;
    for (uint8_t pindex=0; pindex<3 && !done; ++pindex)
    {
        for (int8_t i=plist[pindex]->lockedIndex; i!=-1;)
        {
//            std::cout << "pindex: " << (int)pindex << "/" << (int)i << "/" << ptr << "/" << plist[pindex]->pages[i].start << "/" << plist[pindex]->pages[i].size << std::endl;
            // lock within requested address?
            if (ptr >= plist[pindex]->pages[i].start &&
                ptr < (plist[pindex]->pages[i].start + plist[pindex]->pages[i].size))
            {
                plistindex = pindex;
                pageindex = i;
                done = true;
                break;
            }

            // end overlaps with this lock?
            if (ptr < plist[pindex]->pages[i].start && (ptr + size) > plist[pindex]->pages[i].start)
            {
                // remove unused locks
                if (plist[pindex]->pages[i].locks == 0)
                {
                    // don't bother with unused pages. It is possible that they are never be used again, meaning they will always be in the way
                    i = freeLockedPage(plist[pindex], i);
                    continue;
                }

                // else shrink to avoid overlap
                size = plist[pindex]->pages[i].start - ptr;
            }

            if (plist[pindex]->pages[i].locks == 0 && unusedlist[pindex] == -1)
                unusedlist[pindex] = i;

            i = plist[pindex]->pages[i].next;
        }
    }

#if 0
    // no existing lock found?
    if (pageindex == -1)
        return makeLock(ptr, size, ro); // create new one, with possibly shrunk size
#endif

    TVirtPtrSize offset = 0;

    // no existing lock found?
    if (pageindex == -1)
    {
//        std::cout << "fitting lock: No lock found\n";
        int8_t secpli = -1;
        for (uint8_t i=0; i<3; ++i)
        {
            if (plist[i]->freeIndex != -1 || unusedlist[i] != -1)
            {
                if (size <= plist[i]->size)
                    plistindex = i;
                else
                    secpli = i; // store in case no fitting size is found
            }
        }

        // no fitting page found, but found one which is smaller?
        if (plistindex == -1 && secpli != -1)
        {
            plistindex = secpli;
            size = plist[plistindex]->size;
        }

        if (plistindex == -1)
        {
            ASSERT(false);
            return 0;
        }

        bool syncpool = true;
        if (plist[plistindex]->freeIndex != -1)
        {
            pageindex = lockPage(plist[plistindex], ptr, size);
            syncpool = plist[plistindex] != &bigPages; // big pages are already synced when locked
        }
        else
        {
            pageindex = unusedlist[plistindex];
            syncLockedPage(&plist[plistindex]->pages[pageindex]);
            plist[plistindex]->pages[pageindex].dirty = false;
        }

#if 0
        if (syncpool)
            memcpy(plist[plistindex]->pages[pageindex].pool, pullRawData(ptr, size, true, false), size);
#else
        if (syncpool)
            copyRawData(plist[plistindex]->pages[pageindex].pool, ptr, size);
#endif

        plist[plistindex]->pages[pageindex].start = ptr;
        plist[plistindex]->pages[pageindex].size = size;
    }
    else
    {
//        std::cout << "fitting lock: Useing existing\n";

        offset = (ptr - plist[plistindex]->pages[pageindex].start);

        // fixup size as the starting address may be different than what was requested
        size = private_utils::minimal(size, static_cast<TVirtPageSize>(plist[plistindex]->pages[pageindex].size - offset));
    }

    // else add to lock count
    ++plist[plistindex]->pages[pageindex].locks;

    if (!plist[plistindex]->pages[pageindex].dirty)
        plist[plistindex]->pages[pageindex].dirty = !ro;

//    std::cout << "fitting lock page: " << (int)pageindex << "/" << ptr << "/" << size << "/" << (int)plistindex << "/" << (int)plist[plistindex]->pages[pageindex].locks << std::endl;

    return plist[plistindex]->pages[pageindex].pool + offset;
}

void CBaseVirtMemAlloc::releaseLock(TVirtPointer ptr)
{
    SLockPage *page = findLockedPage(ptr);
    ASSERT(page && page->locks);
//    std::cout << "temp unlock page: " << (int)ptr << "/" << (int)page->locks << std::endl;
    --page->locks;
    if (!page->locks)
    {
        // was it a big page? free it so that it can be re-used for non locked IO
        const int8_t index = findLockedPage(&bigPages, ptr);
        if (index != -1)
            freeLockedPage(&bigPages, index);
    }
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
    fflush(stdout);
#endif
}
