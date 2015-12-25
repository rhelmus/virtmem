#ifndef BENCHMARK_H
#define BENCHMARK_H

using namespace virtmem;

// arduino template functions must be in headers... :-(

inline void printBenchStart(uint32_t bufsize, uint8_t repeats)
{
    Serial.print("Writing "); Serial.print((int)repeats); Serial.print(" x ");
    Serial.print(bufsize); Serial.println(" bytes");
}

inline void printBenchEnd(uint32_t time, uint32_t bufsize, uint8_t repeats)
{
    Serial.print("Finished in "); Serial.print(time); Serial.println(" ms");
    // bytes / ms is roughly kB/s, * 1000 / 1024 to get kB/s
    Serial.print("Speed: "); Serial.print(repeats * bufsize / time * 1000 / 1024);
    Serial.println(" kB/s");
}

template <typename TA> void printVAStats(TA &valloc)
{
#ifdef VIRTMEM_TRACE_STATS
    Serial.println("allocator stats:");
    Serial.print("Page reads:"); Serial.println(valloc.getBigPageReads());
    Serial.print("Page writes:"); Serial.println(valloc.getBigPageWrites());
    Serial.print("kB read:"); Serial.println(valloc.getBytesRead() / 1024);
    Serial.print("kB written:"); Serial.println(valloc.getBytesWritten() / 1024);
    valloc.resetStats();
#endif
}

template <typename TA> void runBenchmarks(TA &valloc, uint32_t bufsize, uint8_t repeats)
{
    typedef VPtr<char, TA> TVirtPtr;
    valloc.start();

    TVirtPtr buf = valloc.template alloc<char>(bufsize);

    printBenchStart(bufsize, repeats);

    uint32_t time = millis();
    for (uint8_t i=0; i<repeats; ++i)
    {
        for (uint32_t j=0; j<bufsize; ++j)
            buf[j] = (char)j;
    }

    valloc.clearPages();
    printBenchEnd(millis() - time, bufsize, repeats);
    printVAStats(valloc);

    Serial.println("Reading data");

    time = millis();
    for (uint8_t i=0; i<repeats; ++i)
    {
        for (uint32_t j=0; j<bufsize; ++j)
        {
            if (buf[j] != (char)j)
            {
                Serial.print("Mismatch! index/value/actual: "); Serial.print(j);
                Serial.print("/"); Serial.print((int)(char)j);
                Serial.print("/"); Serial.println((int)buf[j]);
            }
        }
    }

    valloc.clearPages();
    printBenchEnd(millis() - time, bufsize, repeats);
    printVAStats(valloc);

    Serial.println("Repeating tests with locks...");
    time = millis();

    for (uint8_t i=0; i<repeats; ++i)
    {
        uint32_t sizeleft = bufsize;
        TVirtPtr p = buf;
        uint8_t counter = 0;

        while (sizeleft)
        {
            uint16_t lsize = min(valloc.getBigPageSize(), sizeleft);
            VPtrLock<TVirtPtr> l = makeVirtPtrLock(p, lsize);
            lsize = l.getLockSize();
            char *b = *l;
            for (uint16_t j=0; j<lsize; ++j)
                b[j] = counter++;
            p += lsize; sizeleft -= lsize;
        }
    }

    valloc.clearPages();
    printBenchEnd(millis() - time, bufsize, repeats);
    printVAStats(valloc);

    Serial.println("Reading data");

    time = millis();
    for (uint8_t i=0; i<repeats; ++i)
    {
        uint32_t sizeleft = bufsize;
        TVirtPtr p = buf;
        uint8_t counter = 0;

        while (sizeleft)
        {
            uint16_t lsize = min(valloc.getBigPageSize(), sizeleft);
            VPtrLock<TVirtPtr> l = makeVirtPtrLock(p, lsize, true);
            lsize = l.getLockSize();
            char *b = *l;
            for (uint16_t j=0; j<lsize; ++j, ++counter)
            {
                if (b[j] != counter)
                {
                    Serial.print("Mismatch! index/value/actual: "); Serial.print(j);
                    Serial.print("/"); Serial.print((int)counter);
                    Serial.print("/"); Serial.println((int)buf[j]);
                }
            }
            p += lsize; sizeleft -= lsize;
        }
    }

    valloc.clearPages();
    printBenchEnd(millis() - time, bufsize, repeats);
    printVAStats(valloc);

    valloc.stop();
}


#endif // BENCHMARK_H
