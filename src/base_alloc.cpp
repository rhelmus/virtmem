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

void CBaseVirtMemAlloc::initPages(SPageInfo *info, SPartialLockPage *pages, uint8_t *pool, uint8_t pcount, TVirtPageSize psize)
{
    info->pages = pages;
    info->count = pcount;
    info->size = psize;
    info->freeIndex = 0;
    info->usedIndex = -1;

    for (uint8_t i=0; i<pcount; ++i)
    {
        info->pages[i].pool = &pool[i * psize];
        if (i == (pcount - 1))
            info->pages[i].next = -1;
        else
            info->pages[i].next = i + 1;
    }
}

TVirtPointer CBaseVirtMemAlloc::getMem(TVirtPtrSize size)
{
    size = private_utils::max(size, (TVirtPtrSize)MIN_ALLOC_SIZE);
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

void CBaseVirtMemAlloc::syncPartialPage(CBaseVirtMemAlloc::SPartialLockPage *page)
{
    if (page->dirty)
    {
        pushRawData(page->start, page->pool, page->size); // UNDONE: make this more efficient
        // UNDONE: unset dirty?
    }
}

int8_t CBaseVirtMemAlloc::freePartialPage(CBaseVirtMemAlloc::SPageInfo *pinfo, int8_t index)
{
    syncPartialPage(&pinfo->pages[index]);

    const int8_t ret = pinfo->pages[index].next;

    if (index == pinfo->usedIndex)
        pinfo->usedIndex = pinfo->pages[index].next;
    else
    {
        int prevind = pinfo->usedIndex;
        for (; pinfo->pages[prevind].next != index; prevind=pinfo->pages[prevind].next)
            ;
        pinfo->pages[prevind].next = pinfo->pages[index].next;
    }
    pinfo->pages[index].next = pinfo->freeIndex;
    pinfo->freeIndex = index;
//    printf("freeing page %d - free/used: %d/%d\n", index, pinfo->freeIndex, pinfo->usedIndex);

    pinfo->pages[index].locks = 0;

    return ret;
}

int8_t CBaseVirtMemAlloc::findPartialLockPage(SPageInfo *pinfo, TVirtPointer p)
{
    for (int8_t i=pinfo->usedIndex; i!=-1; i=pinfo->pages[i].next)
    {
        if (p >= pinfo->pages[i].start && (p - pinfo->pages[i].start) < pinfo->pages[i].size)
            return i;
    }

    return -1;
}

CBaseVirtMemAlloc::SPartialLockPage *CBaseVirtMemAlloc::findPartialLockPage(TVirtPointer p)
{
    int8_t index = findPartialLockPage(&smallPages, p);
    if (index != -1)
        return &smallPages.pages[index];

    index = findPartialLockPage(&mediumPages, p);
    if (index != -1)
        return &mediumPages.pages[index];

    index = findPartialLockPage(&bigPages, p);
    if (index != -1)
        return &bigPages.pages[index];

    return 0;
}

void CBaseVirtMemAlloc::removePartialLockFrom(SPageInfo *pinfo, TVirtPointer p)
{
    const int8_t index = findPartialLockPage(pinfo, p);
    if (index != -1)
        freePartialPage(pinfo, index);
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

    TVirtPointer p = getHeaderConst(prevp)->s.next;
    while (true)
    {
        const UMemHeader *consth = getHeaderConst(p);

        // big enough ?
        if (consth->s.size >= quantity)
        {
            // exactly ?
            if (consth->s.size == quantity)
            {
                // just eliminate this block from the free list by pointing
                // its prev's next to its next

                TVirtPointer next = consth->s.next;
                UMemHeader prevh = *getHeaderConst(prevp);
                prevh.s.next = next;
                updateHeader(prevp, &prevh);
                // NOTE: getHeaderConst might invalidate h from here ----
            }
            else // too big
            {
                UMemHeader h = *consth;
                h.s.size -= quantity;
                updateHeader(p, &h);
                p += (h.s.size * sizeof(UMemHeader));
                h = *getHeaderConst(p);
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
                std::cout << "!! Memory allocation failed !!\n";
                assert(false);
                return 0;
            }
            consth = getHeaderConst(p);
        }

        prevp = p;
        p = consth->s.next;
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

    assert(p);
    assert(stath.s.next);
    freePointer = p;
}

void *CBaseVirtMemAlloc::read(TVirtPointer p, TVirtPtrSize size)
{
    // check if data is in a partial locked page first
    SPartialLockPage *page = findPartialLockPage(p);
    if (page)
    {
        const TVirtPointer offset = p - page->start;

        // data fits in this page?
        if ((offset + size) <= page->size)
        {
//            std::cout << "using temp lock page " << (int)(pageindex) << ", " << p << std::endl;
            return (char *)page->pool + offset;
        }

        // only fits partially... mirror data to normal page so a continuous block can be returned
        pushRawData(page->start, page->pool, page->size); // UNDONE
//        std::cout << "mirrored partial page " << (int)(page) << std::endl;
    }
//    std::cout << "read in regular page " << p << "/" << size << std::endl;

    // not in or too big for partial page, use regular paged memory
    return pullRawData(p, size, true, false);
}

void CBaseVirtMemAlloc::write(TVirtPointer p, const void *d, TVirtPtrSize size)
{
    // check if data is in a partial locked page first
    SPartialLockPage *page = findPartialLockPage(p);
    if (page)
    {
        const TVirtPointer offset = p - page->start;

        if (!page->dirty)
            page->dirty = true;

        // data fits in this page?
        if ((offset + size) <= page->size)
        {
            memcpy((char *)page->pool + offset, d, size);
//            std::cout << "write in part locked page " << (int)(pageindex) << ", " << p << "/" << size << ": " << (int)*(char *)d << std::endl;
        }
        else
        {
            // partial fit, copy stuff that fits in partial page and rest to regular memory
            const TVirtPtrSize partsize = page->size - offset;
            memcpy((char *)page->pool + offset, d, partsize);
            pushRawData(p + partsize, (char *)d + partsize, size - partsize);
//            std::cout << "partial write in page " << (int)(page) << std::endl;
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
    TVirtPageSize retsize = private_utils::min(reqsize, (TVirtPtrSize)bigPages.size);
    TVirtPageSize blsize = 0;
    const SPageInfo *pinfos[3] = { &smallPages, &mediumPages, &bigPages };

    for (uint8_t pindex=0; pindex<3; ++pindex)
    {
        for (int8_t i=pinfos[pindex]->usedIndex; i!=-1; i=pinfos[pindex]->pages[i].next)
        {
            // start block within partial page?
            if (p >= pinfos[pindex]->pages[i].start && p < (pinfos[pindex]->pages[i].start + pinfos[pindex]->pages[i].size))
            {
                retsize = 0;
                blsize = private_utils::max(blsize, pinfos[pindex]->pages[i].size);
            }
            // end block within partial page?
            else if (p < pinfos[pindex]->pages[i].start && (p + retsize) > pinfos[pindex]->pages[i].start)
            {
                retsize = private_utils::min(retsize, (TVirtPageSize)(pinfos[pindex]->pages[i].start - p));
                blsize = private_utils::max(blsize, pinfos[pindex]->pages[i].size);
            }
        }
    }

    if (blockedsize)
        *blockedsize = blsize;

    assert(retsize <= reqsize);

    return retsize;
}

void *CBaseVirtMemAlloc::makePartialLock(TVirtPointer ptr, TVirtPageSize size, bool ro)
{
    assert(ptr != 0);
    assert(size <= bigPages.size);

    SPageInfo *pinfo, *secpinfo = 0;
    if (size <= smallPages.size)
    {
        pinfo = &smallPages;
//        std::cout << "use small page for " << ptr << std::endl;
//        removePartialLockFrom(&mediumPages, ptr);
//        removePartialLockFrom(&bigPages, ptr);
    }
    else if (size <= mediumPages.size)
    {
        pinfo = &mediumPages;
//        std::cout << "use medium page for " << ptr << std::endl;
//        removePartialLockFrom(&smallPages, ptr);
//        removePartialLockFrom(&bigPages, ptr);
    }
    else
    {
        pinfo = &bigPages;
//        std::cout << "use big page for " << ptr << std::endl;
//        removePartialLockFrom(&smallPages, ptr);
//        removePartialLockFrom(&mediumPages, ptr);
    }

    SPageInfo *plist[3] = { &smallPages, &mediumPages, &bigPages };
    int8_t pageindex = -1, oldlockindex = -1, secoldlockindex = -1;
    bool fixbeginningoverlap = false, done = false;
    for (uint8_t pindex=0; pindex<3 && !done; ++pindex)
    {
        for (int8_t i=plist[pindex]->usedIndex; i!=-1;)
        {
            // already there?
            if (plist[pindex]->pages[i].start == ptr)
            {
                if (pinfo != plist[pindex])
                {
                    if (plist[pindex]->pages[i].locks == 0)
                    {
                        // lock was previously created with different size, remove it
                        i = freePartialPage(plist[pindex], i);
                        continue;
                    }
                    else // still locked, using a different, presumably larger, page size
                    {
                        assert(plist[pindex]->size > pinfo->size);
                        pinfo = plist[pindex];
//                        std::cout << "use secondary locked page\n";

                        // ...fallthrough...
                    }
                }
                else
                    assert(plist[pindex]->pages[i].size <= size);

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
                        size = plist[pindex]->pages[i].start - ptr; // Shrink so it fits
                    else if (beginoverlaps)
                        fixbeginningoverlap = true;
                }
                else if (endoverlaps || beginoverlaps)
                {
                    // don't bother with unused pages. It is possible that they are never be used again, meaning they will always be in the way
                    i = freePartialPage(plist[pindex], i);
                    continue;
                }

                if (oldlockindex == -1 && plist[pindex]->pages[i].locks == 0)
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

            i = plist[pindex]->pages[i].next;
        }
    }

    assert(pageindex == -1 || size >= pinfo->pages[pageindex].size);
    assert(pageindex == -1 || size == pinfo->pages[pageindex].size || !pinfo->pages[pageindex].locks);
    assert(pageindex == -1 || !fixbeginningoverlap);

    // make new or reuse old lock?
    if (pageindex == -1)
    {
        if (pinfo->freeIndex == -1) // no space left in chosen page size?
        {
            // space left in bigger page?
            if (pinfo->size < mediumPages.size && mediumPages.freeIndex != -1)
                pinfo = &mediumPages;
            else if (pinfo->size < bigPages.size && bigPages.freeIndex != -1)
                pinfo = &bigPages;
        }

        if (pinfo->freeIndex != -1)
        {
            pageindex = pinfo->freeIndex;
            pinfo->freeIndex = pinfo->pages[pinfo->freeIndex].next;
            if (pinfo->usedIndex == -1)
                pinfo->pages[pageindex].next = -1;
            else
                pinfo->pages[pageindex].next = pinfo->usedIndex;
            pinfo->usedIndex = pageindex;
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
                syncPartialPage(&pinfo->pages[oldlockindex]);
                pageindex = oldlockindex;
            }
            else
                return 0; // no space left
        }

        TVirtPtrSize copyoffset = 0;
        bool fixed = false; // UNDONE
        if (fixbeginningoverlap)
        {
            // check pages that overlap in the beginning and resize them if necessary
            // NOTE: we couldn't do this earlier since it was unknown which data is going to be used
            for (uint8_t pindex=0; pindex<3; ++pindex)
            {
                for (int8_t i=plist[pindex]->usedIndex; i!=-1; i=plist[pindex]->pages[i].next)
                {
                    if ((i != pageindex || plist[pindex] != pinfo) && ptr > plist[pindex]->pages[i].start &&
                        ptr < (plist[pindex]->pages[i].start + plist[pindex]->pages[i].size))
                    {
                        assert(!fixed);

                        // copy their overlapping data (assume this is the most up to date)
                        const TVirtPtrSize offsetold = ptr - plist[pindex]->pages[i].start;
                        const TVirtPageSize copysize = private_utils::min((TVirtPageSize)(plist[pindex]->pages[i].size - offsetold), size);
                        memcpy(pinfo->pages[pageindex].pool, (char *)plist[pindex]->pages[i].pool + offsetold, copysize);
                        copyoffset = copysize;
                        plist[pindex]->pages[i].size = offsetold; // shrink other so this one fits
                        fixed = true;
                    }
                }
            }
        }

        // copy (rest of) data
        if (copyoffset < size)
        {
            const TVirtPtrSize copysize = size - copyoffset;
            memcpy(pinfo->pages[pageindex].pool + copyoffset, pullRawData(ptr + copyoffset, copysize, true, false), copysize); // UNDONE: make this more efficient
        }

        pinfo->pages[pageindex].start = ptr;
    }
    else
    {
        // size increased? (this can happen when either this page was used for a smaller data type, or other pages don't overlap anymore
        if (size > pinfo->pages[pageindex].size)
        {
            const TVirtPtrSize offset = pinfo->pages[pageindex].size;
            const TVirtPtrSize copysize = size - offset;
            // copy excess data to page
            // UNDONE: make this more efficient
            memcpy(pinfo->pages[pageindex].pool + offset, pullRawData(ptr + offset, copysize, true, false), copysize);
        }
    }

    /*
    if (pinfo->pages[pageindex].locks == 0)
        printf("%s page %d (%d) - free/used: %d/%d\n", (pageindex == oldlockindex) ? "re-using" : "locking", pageindex, ptr, pinfo->freeIndex, pinfo->usedIndex);
    else
        printf("using existing page %d (%d) - free/used: %d/%d\n", pageindex, ptr, pinfo->freeIndex, pinfo->usedIndex);*/

    if (!pinfo->pages[pageindex].dirty || pinfo->pages[pageindex].locks == 0)
        pinfo->pages[pageindex].dirty = !ro;

    ++pinfo->pages[pageindex].locks;
    pinfo->pages[pageindex].size = size;
//    std::cout << "temp lock page: " << (int)(page - pinfo->pages) << ", " << ptr << "/" << size << std::endl;
    assert(size <= pinfo->size);
    return pinfo->pages[pageindex].pool;
}

void CBaseVirtMemAlloc::releasePartialLock(TVirtPointer ptr)
{
    SPartialLockPage *page = findPartialLockPage(ptr);
    assert(page && page->locks);
    --page->locks;
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
