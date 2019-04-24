#pragma once
#include <array>
#include <cinttypes>
#include <cstddef>
#include <vector>

class transportstream {
public:
    uint16_t analysePacket(const uint8_t* data, size_t size);
    void printStats();
    struct pidstats {
        bool payloadunitstart = false;
        size_t packets = 0;
        uint8_t streamtype = 0;
        int16_t streamid = 0;
        size_t payloadsize = 0;
        int16_t pespayloadtsize = 0;
        int64_t decodetimestamp = -1;
        int64_t presentationtimestamp = -1;
        int64_t previousdecodetimestamp = -1;
    };

    int pmtpid;
    std::array<struct pidstats, 8192> stats;

    std::vector<uint8_t> m_data;
    void analyse(const uint8_t* data, size_t size)
    {
        m_data.insert(m_data.end(), data, data + size);
        data = m_data.data(), size = m_data.size();
        for (; 188 <= size; data += 188, size -= 188) {
            analysePacket(data, 188);
        }

        m_data = std::vector<uint8_t>(data, data + size);
    }
};
