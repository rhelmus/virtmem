#include "virtmem.h"
#include "stdioalloc.h"
#include "test.h"

typedef CWrapFixture<char> CCharWrapFixture;
typedef TStdioVirtPtr<uint8_t>::type TUCharVirtPtr;

// Clamp between -1 - +1
inline int clampOne(int n)
{
    return (n < 0) ? -1 : (n == 0) ? 0 : 1;
}

TEST_F(CAllocFixture, memcmpTest)
{
    const int bufsize = 10;
    TUCharVirtPtr vbuf1 = vbuf1.alloc(bufsize);
    TUCharVirtPtr vbuf2 = vbuf2.alloc(bufsize);

    uint8_t buf1[bufsize], buf2[bufsize];

    for (int i=0; i<bufsize; ++i)
        vbuf1[i] = vbuf2[i] = buf1[i] = buf2[i] = bufsize - i;

    valloc.clearPages();
    EXPECT_EQ(memcmp(vbuf1, vbuf2, bufsize), 0);
    EXPECT_EQ(memcmp(vbuf1, buf1, bufsize), 0);
    EXPECT_EQ(memcmp(buf1, vbuf1, bufsize), 0);
    EXPECT_EQ(clampOne(memcmp(vbuf1, vbuf2, bufsize)), clampOne(memcmp(buf1, buf2, bufsize)));
    EXPECT_EQ(clampOne(memcmp(vbuf2, vbuf1, bufsize)), clampOne(memcmp(buf2, buf1, bufsize)));

    vbuf2[2] = buf2[2] = 33;
    valloc.clearPages();
    EXPECT_EQ(clampOne(memcmp(vbuf1, vbuf2, bufsize)), clampOne(memcmp(buf1, buf2, bufsize)));
    EXPECT_EQ(clampOne(memcmp(buf1, vbuf2, bufsize)), clampOne(memcmp(buf1, buf2, bufsize)));
    EXPECT_EQ(clampOne(memcmp(vbuf1, buf2, bufsize)), clampOne(memcmp(buf1, buf2, bufsize)));
    EXPECT_EQ(clampOne(memcmp(vbuf2, vbuf1, bufsize)), clampOne(memcmp(buf2, buf1, bufsize)));
    EXPECT_EQ(clampOne(memcmp(buf2, vbuf1, bufsize)), clampOne(memcmp(buf2, buf1, bufsize)));
    EXPECT_EQ(clampOne(memcmp(vbuf2, buf1, bufsize)), clampOne(memcmp(buf2, buf1, bufsize)));

    vbuf1[3] = buf1[3] = 66;
    valloc.clearPages();
    EXPECT_EQ(clampOne(memcmp(vbuf1, vbuf2, bufsize)), clampOne(memcmp(buf1, buf2, bufsize)));
    EXPECT_EQ(clampOne(memcmp(buf1, vbuf2, bufsize)), clampOne(memcmp(buf1, buf2, bufsize)));
    EXPECT_EQ(clampOne(memcmp(vbuf1, buf2, bufsize)), clampOne(memcmp(buf1, buf2, bufsize)));
    EXPECT_EQ(clampOne(memcmp(vbuf2, vbuf1, bufsize)), clampOne(memcmp(buf2, buf1, bufsize)));
    EXPECT_EQ(clampOne(memcmp(buf2, vbuf1, bufsize)), clampOne(memcmp(buf2, buf1, bufsize)));
    EXPECT_EQ(clampOne(memcmp(vbuf2, buf1, bufsize)), clampOne(memcmp(buf2, buf1, bufsize)));
}

TEST_F(CAllocFixture, memcpyTest)
{
    const int bufsize = 10;
    TUCharVirtPtr vbuf1 = vbuf1.alloc(bufsize);
    TUCharVirtPtr vbuf2 = vbuf2.alloc(bufsize);
    uint8_t buf[bufsize];

    for (int i=0; i<bufsize; ++i)
        vbuf1[i] = bufsize - i;

    valloc.clearPages();

    memcpy(vbuf2, vbuf1, bufsize);
    valloc.clearPages();
    EXPECT_EQ(memcmp(vbuf1, vbuf2, bufsize), 0);

    memset(buf, 0, bufsize);
    memcpy(buf, vbuf1, bufsize);
    EXPECT_EQ(memcmp(vbuf1, buf, bufsize), 0);

    buf[3] = 65;
    memcpy(vbuf2, buf, bufsize);
    valloc.clearPages();
    EXPECT_EQ(memcmp(buf, vbuf2, bufsize), 0);
}

TEST_F(CAllocFixture, memcpyLargeTest)
{
    const int bufsize = valloc.getPoolSize() / 3;
    std::vector<uint8_t> buf;

    buf.reserve(bufsize);
    for (int i=0; i<bufsize; ++i)
        buf[i] = bufsize - i;

    TUCharVirtPtr vbuf = vbuf.alloc(bufsize);
    memcpy(vbuf, &buf[0], bufsize);
    valloc.clearPages();
    EXPECT_EQ(memcmp(&buf[0], vbuf, bufsize), 0);

    TUCharVirtPtr vbuf2 = vbuf2.alloc(bufsize);
    memcpy(vbuf2, vbuf, bufsize);
    valloc.clearPages();
    EXPECT_EQ(memcmp(vbuf, vbuf2, bufsize), 0);
}
