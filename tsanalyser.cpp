#include "tsanalyser.hpp"
#include <cassert>
#include <memory.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

class safe_access {
public:
    size_t size = 0;
    uint8_t* data = nullptr;
    safe_access& operator+=(size_t s)
    {
        assert(s <= size);
        s = std::min(s, size);
        data += s, size -= s;
        return *this;
    }

    uint8_t operator[](size_t i) const
    {
        assert(i < size);
        static const uint8_t zero = 0;
        return size <= i ? zero : data[i];
    }

    safe_access(const uint8_t* data, size_t size)
        : data(const_cast<uint8_t*>(data))
        , size(size)
    {
    }
};

int64_t decode_timestap(const safe_access& d, size_t off)
{
    int64_t t = 0;
    // mmmmxxx1 xxxxxxxx xxxxxxx1 xxxxxxxx xxxxxxx1
    t |= d[off + 0] << 29 & 0x1c0000000;
    t |= d[off + 1] << 22 & 0x03fc00000;
    t |= d[off + 2] << 14 & 0x0003f8000;
    t |= d[off + 3] << 7 & 0x000007f10;
    t |= d[off + 4] >> 1 & 0x00000007f;
    return t;
}

uint16_t transportstream::analysePacket(const uint8_t* data, size_t size)
{
    assert(188 == size && 0x47 == data[0]);
    safe_access d(data, size);
    bool payload = d[3] & 0x10;
    bool adaptationfield = d[3] & 0x20;
    uint16_t pid = ((d[1] & 0x1F) << 8) | d[2]; // PacketId
    uint8_t cc = 0x0F & d[3]; // Continuity Counter
    stats[pid].payloadunitstart = 0x40 & d[1]; // Pyaload Unit Start Indicator
    ++stats[pid].packets;

    d += 4;
    if (adaptationfield) { // Adaption Field
        d += d[0] + 1;
    }

    if (0 == pid) {
        // PAT
        pmtpid = (d[11] << 8 & 0x1f00) | d[12];
    } else if (pmtpid == pid) {
        bool current = d[6] & 0x01;
        int16_t sectionlength = (d[2] & 0x03 << 8) | d[3];
        int16_t pcrpid = (d[9] & 0x1f << 8) | d[10];
        sectionlength -= 13;
        d += 13;
        while (5 <= sectionlength) {
            uint8_t streamtype = d[0];
            int16_t streampid = (d[1] << 8 & 0x1f00) | d[2];
            int16_t infolength = (d[3] << 8 & 0x0300) | d[4];
            sectionlength -= 5 + infolength;
            stats[streampid].streamtype = streamtype;
            d += 5 + infolength;
        }
    } else if (stats[pid].streamtype == 3 || stats[pid].streamtype == 15 || stats[pid].streamtype == 27) {
        // 3 = mp3, 15 = aac, 27 =avc
        if (stats[pid].payloadunitstart) {
            assert(0 == d[0] && 0 == d[1] && 1 == d[2]);
            stats[pid].streamid = d[3]; // StreamID
            uint16_t pespacketsize = (d[4] << 8) | d[5];
            uint16_t flags = (d[6] << 8) | d[7];
            int16_t pesheadersize = 9 + d[8];

            if (flags & 0x80) {
                stats[pid].previousdecodetimestamp = stats[pid].decodetimestamp;
                stats[pid].presentationtimestamp = decode_timestap(d, 9);
                if (flags & 0x40) {
                    stats[pid].decodetimestamp = decode_timestap(d, 14);
                } else {
                    stats[pid].decodetimestamp = stats[pid].presentationtimestamp;
                }
            }

            d += pesheadersize;

            // If not a video stream. record payload size
            if (!(stats[pid].streamid >= 0xe0 && stats[pid].streamid <= 0xeF)) {
                stats[pid].payloadsize += pespacketsize - pesheadersize;
            }
        }

        // if this is a video stream, count the bytes remaining in the packet
        if (payload && stats[pid].streamid >= 0xe0 && stats[pid].streamid <= 0xeF) {
            stats[pid].payloadsize += d.size;
        }
    }

    return pid;
}

void transportstream::printStats()
{
    size_t totalpackets = 0, totalpayloadsize = 0;
    for (int pid = 0; pid < stats.size(); ++pid) {
        if (0 < stats[pid].packets) {
            totalpackets += stats[pid].packets;
            totalpayloadsize += stats[pid].payloadsize;
            double overhead = 0 >= stats[pid].payloadsize ? 1.0 : 1 - ((double)stats[pid].payloadsize / (188 * stats[pid].packets));
            printf("PID %d: SID %02x, size %zu, overhead %0.2f%%\n", pid, stats[pid].streamid, 188 * stats[pid].packets, 100 * overhead);
        }
    }

    double overhead = 1 - ((double)totalpayloadsize / (188 * totalpackets));
    printf("-------------------------------------------------------------\n");
    printf("TOTALS: size %zu, overhead %0.2f%%\n", 188 * totalpackets, 100 * overhead);
}
