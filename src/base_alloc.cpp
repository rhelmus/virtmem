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
    : memPageList(mp), pageCount(pc), poolSize(ps), pageSize(pgs), freePageIndex(0), usedPageIndex(-1), freePointer(0), nextPageToSwap(0)
{
    baseFreeList.s.next = 0;
    baseFreeList.s.size = 0;
    poolFreePos = START_OFFSET + sizeof(UMemHeader);

    for (uint8_t i=0; i<(MAX_PARTIAL_LOCK_PAGES-1); ++i)
        partialLockPages[i].next = i + 1;
    partialLockPages[pageCount-1].next = -1;
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

void CBaseVirtMemAlloc::syncPartialPage(int8_t index)
{
    if (!partialLockPages[index].readOnly)
        pushRawData(partialLockPages[index].start, partialLockPages[index].data, partialLockPages[index].size); // UNDONE: make this more efficient
}

int8_t CBaseVirtMemAlloc::freePartialPage(int8_t index)
{
    syncPartialPage(index);

    const int8_t ret = partialLockPages[index].next;

    if (index == usedPageIndex)
        usedPageIndex = partialLockPages[index].next;
    else
    {
        int prevind = usedPageIndex;
        for (; partialLockPages[prevind].next != index; prevind=partialLockPages[prevind].next)
            ;
        partialLockPages[prevind].next = partialLockPages[index].next;
    }
    partialLockPages[index].next = freePageIndex;
    freePageIndex = index;
//    printf("freeing page %d - free/used: %d/%d\n", index, freePageIndex, usedPageIndex);

    partialLockPages[index].locks = 0;

    return ret;
}

int8_t CBaseVirtMemAlloc::findPartialLockPage(TVirtPointer p)
{
    for (int8_t i=usedPageIndex; i!=-1; i=partialLockPages[i].next)
    {
        if (p >= partialLockPages[i].start && (p - partialLockPages[i].start) < partialLockPages[i].size)
            return i;
    }

    return -1;
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
    const int8_t pageindex = findPartialLockPage(p);
    if (pageindex != -1)
    {
        const TVirtPointer offset = p - partialLockPages[pageindex].start;

        // data fits in this page?
        if ((offset + size) <= partialLockPages[pageindex].size)
        {
            if (!readonly && partialLockPages[pageindex].readOnly)
                partialLockPages[pageindex].readOnly = false;
//            std::cout << "using temp lock page " << (int)(pageindex) << ", " << p << std::endl;

            return (char *)partialLockPages[pageindex].data + offset;
        }

        // only fits partially... mirror data to normal page so a continuous block can be returned
        pushRawData(partialLockPages[pageindex].start, partialLockPages[pageindex].data, partialLockPages[pageindex].size); // UNDONE
//        std::cout << "mirrored partial page " << (int)(pageindex) << std::endl;
    }

//    std::cout << "read in regular page " << p << "/" << size << std::endl;

    // not in or too big for partial page, use regular paged memory
    return pullRawData(p, size, readonly, false);
}

void CBaseVirtMemAlloc::write(TVirtPointer p, const void *d, TVirtPtrSize size)
{
    // check if data is in a partial locked page first
    const int8_t pageindex = findPartialLockPage(p);
    if (pageindex != -1)
    {
        const TVirtPointer offset = p - partialLockPages[pageindex].start;

        if (partialLockPages[pageindex].readOnly)
            partialLockPages[pageindex].readOnly = false;

        // data fits in this page?
        if ((offset + size) <= partialLockPages[pageindex].size)
        {
            memcpy((char *)partialLockPages[pageindex].data + offset, d, size);
//            std::cout << "write in part locked page " << (int)(pageindex) << ", " << p << "/" << size << ": " << (int)*(char *)d << std::endl;
        }
        else
        {
            // partial fit, copy stuff that fits in partial page and rest to regular memory
            const TVirtPtrSize partsize = partialLockPages[pageindex].size - offset;
            memcpy((char *)partialLockPages[pageindex].data + offset, d, partsize);
            pushRawData(p + partsize, (char *)d + partsize, size - partsize);
//            std::cout << "partial write in page " << (int)(pageindex) << std::endl;
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

    for (int i=usedPageIndex; i!=-1; i=partialLockPages[i].next)
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

    if (blockedsize)
        *blockedsize = blsize;

    assert(retsize <= reqsize);

    return retsize;
}

void *CBaseVirtMemAlloc::makePartialLock(TVirtPointer ptr, TVirtPtrSize size, void *, bool ro) // UNDONE
{
    assert(ptr != 0);
    assert(size <= 128); // UNDONE

    int8_t pageindex = -1, oldlockindex = -1;
    bool fixbeginningoverlap = false;

    for (int8_t i=usedPageIndex; i!=-1;)
    {
        if (partialLockPages[i].start == ptr) // same data, add to lock count
        {
            assert(partialLockPages[i].size <= size);
            pageindex = i;
            if (partialLockPages[i].size == size)
                break; // we're finished searching, no need to look for overlapping pages as size is already OK
        }
        else
        {
            const bool endoverlaps = (ptr < partialLockPages[i].start && (ptr + size) >= partialLockPages[i].start);
            const bool beginoverlaps = (ptr > partialLockPages[i].start && ptr < (partialLockPages[i].start + partialLockPages[i].size));

            if (partialLockPages[i].locks)
            {
                if (endoverlaps) // end overlaps
                    size = partialLockPages[i].start - ptr; // Shrink so it fits
                else if (beginoverlaps)
                    fixbeginningoverlap = true;
#if 0
                else if (ptr > partialLockPages[i].start && ptr < (partialLockPages[i].start + partialLockPages[i].size))
                {
                    // copy their overlapping data (assume this is the most up to date)
                    const TVirtPtrSize offsetold = ptr - partialLockPages[i].start, copysize = private_utils::min(partialLockPages[i].size - offsetold, size);
                    memcpy(data, (char *)partialLockPages[i].data + offsetold, copysize);
                    copyoffset = copysize;
                    partialLockPages[i].size = offsetold; // shrink other so this one fits
                }
#endif
            }
            else if (endoverlaps || beginoverlaps)
            {
                // don't bother with unused pages. It is possible that they are never be used again, meaning they will always be in the way
                i = freePartialPage(i);
                continue;
            }

            if (oldlockindex == -1 && partialLockPages[i].locks == 0)
                oldlockindex = i;
        }

        i = partialLockPages[i].next;
    }

    assert(pageindex == -1 || size >= partialLockPages[pageindex].size);
    assert(pageindex == -1 || size == partialLockPages[pageindex].size || !partialLockPages[pageindex].locks);
    assert(pageindex == -1 || !fixbeginningoverlap);

    // make new or reuse old lock?
    if (pageindex == -1)
    {
        if (freePageIndex != -1)
        {
            pageindex = freePageIndex;
            freePageIndex = partialLockPages[freePageIndex].next;
            if (usedPageIndex == -1)
                partialLockPages[pageindex].next = -1;
            else
                partialLockPages[pageindex].next = usedPageIndex;
            usedPageIndex = pageindex;
        }
        else if (oldlockindex != -1)
        {
            syncPartialPage(oldlockindex);
            pageindex = oldlockindex;
        }
        else
            return 0; // no space left

        TVirtPtrSize copyoffset = 0;
        if (fixbeginningoverlap)
        {
            // check pages that overlap in the beginning and resize them if necessary
            // NOTE: we couldn't do this earlier since it was unknown which data is going to be used
            for (int8_t i=usedPageIndex; i!=-1; i=partialLockPages[i].next)
            {
                if (i != pageindex && ptr > partialLockPages[i].start && ptr < (partialLockPages[i].start + partialLockPages[i].size))
                {
                    // copy their overlapping data (assume this is the most up to date)
                    const TVirtPtrSize offsetold = ptr - partialLockPages[i].start, copysize = private_utils::min(partialLockPages[i].size - offsetold, size);
                    memcpy(partialLockPages[pageindex].data, (char *)partialLockPages[i].data + offsetold, copysize);
                    copyoffset = copysize;
                    partialLockPages[i].size = offsetold; // shrink other so this one fits
                }
            }
        }

        // copy (rest of) data
        if (copyoffset < size)
        {
            const TVirtPtrSize copysize = size - copyoffset;
            memcpy(partialLockPages[pageindex].data + copyoffset, pullRawData(ptr + copyoffset, copysize, true, false), copysize); // UNDONE: make this more efficient
        }

        partialLockPages[pageindex].start = ptr;
    }
    else
    {
        // size increased? (this can happen when either this page was used for a smaller data type, or other pages don't overlap anymore
        if (size > partialLockPages[pageindex].size)
        {
            const TVirtPtrSize offset = partialLockPages[pageindex].size;
            const TVirtPtrSize copysize = size - offset;
            // copy excess data to page
            // UNDONE: make this more efficient
            memcpy(partialLockPages[pageindex].data + offset, pullRawData(ptr + offset, copysize, true, false), copysize);
        }
    }

    /*
    if (partialLockPages[pageindex].locks == 0)
        printf("%s page %d (%d) - free/used: %d/%d\n", (pageindex == oldlockindex) ? "re-using" : "locking", pageindex, ptr, freePageIndex, usedPageIndex);
    else
        printf("using existing page %d (%d) - free/used: %d/%d\n", pageindex, ptr, freePageIndex, usedPageIndex);*/

    if (partialLockPages[pageindex].readOnly || partialLockPages[pageindex].locks == 0)
        partialLockPages[pageindex].readOnly = ro;

    ++partialLockPages[pageindex].locks;

    partialLockPages[pageindex].size = size;

//    std::cout << "temp lock page: " << (int)(page - partialLockPages) << ", " << ptr << "/" << size << std::endl;

    return partialLockPages[pageindex].data;

#if 0
    SPartialLockPage *page = 0;
    TVirtPtrSize copyoffset = 0;

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
                size = partialLockPages[i].start - ptr; // Shrink so it fits
            else if (ptr > partialLockPages[i].start && ptr < (partialLockPages[i].start + partialLockPages[i].size))
            {
                // copy their overlapping data (assume this is the most up to date)
                const TVirtPtrSize offsetold = ptr - partialLockPages[i].start, copysize = private_utils::min(partialLockPages[i].size - offsetold, size);
                memcpy(data, (char *)partialLockPages[i].data + offsetold, copysize);
                copyoffset = copysize;
                partialLockPages[i].size = offsetold; // shrink other so this one fits
            }
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
#endif
}

void CBaseVirtMemAlloc::releasePartialLock(TVirtPointer ptr)
{
    int8_t index = usedPageIndex;
    for (; index != -1; index = partialLockPages[index].next)
    {
        if (partialLockPages[index].start == ptr)
            break;
    }

    assert(index != -1 && partialLockPages[index].locks);

    if (index == -1 || !partialLockPages[index].locks)
        return;

    --partialLockPages[index].locks;

//    std::cout << "unlocking page " << (int)index << "/" << (int)partialLockPages[index].locks << "/" << ptr << std::endl;

#if 0
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
#endif
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
