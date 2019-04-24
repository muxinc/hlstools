#pragma once
#include "json.hpp"
#include <sstream>

// https://tools.ietf.org/html/rfc8216
// https://video-dev.github.io/hlsjs-rfcs/docs/0001-lhls

class m3u8 {
public:
    static nlohmann::json parse(const char* m3u8);
    static nlohmann::json parse(const std::string& m3u8) { return parse(m3u8.c_str()); }
};
