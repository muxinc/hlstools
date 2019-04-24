#include "m3u8.hpp"
#include "netutils.hpp"
#include "tsanalyser.hpp"
#include <chrono>
#include <functional>
#include <iostream>
#include <vector>

///////////////////////////////////////////////////////////////////////////////////////////////////
void ProcessVariant(const nlohmann::json& playlist, const uri& base_uri)
{
    transportstream ts;
    const auto& EXTINF = playlist.find("INF");

    for (const auto& seg : *EXTINF) {
        // TODO check for MAP to get init (It could contain PAT/PMT)
        // TODO check mime type
        auto segment_uri = base_uri.resolve(uri(seg["URI"]));
        auto byterange = seg.find("BYTE-RANGE");
        size_t offset = 0, size = 0;
        if (byterange != seg.end()) {
            if (2 == byterange->size()) {
                offset = byterange->at(0).get<int>();
                size = byterange->at(1).get<int>();
            }
        }

        net::download(segment_uri, offset, size, [&ts](const char* data, size_t size) -> bool {
            ts.analyse(reinterpret_cast<const uint8_t*>(data), size);
            return true;
        });
    }

    ts.printStats();
}

int main(int argc, char** argv)
{
    curl_global_init(CURL_GLOBAL_ALL);
    auto playlist_url = uri(argv[1]);

    std::string playlist;
    auto err = net::download(playlist_url, [&playlist](const char* data, unsigned int size) -> bool {
        static const size_t maxplaylistSize = 10 * 1024 * 1024;
        playlist.insert(playlist.end(), data, data + size);
        return playlist.size() < maxplaylistSize;
    });

    if (playlist.empty()) {
        std::cerr << "Could not download playlist: " << err << std::endl;
        return EXIT_FAILURE;
    }

    auto json = m3u8::parse(playlist);
    const auto X_STREAM_INF = json.find("X-STREAM-INF");

    if (json.end() != X_STREAM_INF) {
        // Master playlist. Pick a rendition
        for (const auto& inf : *X_STREAM_INF) {
            auto redition_url = playlist_url.resolve(uri(inf["URI"]));
            playlist.clear();
            std::cout << redition_url.string() << std::endl;
            net::download(redition_url, [&playlist](const char* data, unsigned int size) -> bool {
                static const size_t maxplaylistSize = 10 * 1024 * 1024;
                playlist.insert(playlist.end(), data, data + size);
                return playlist.size() < maxplaylistSize;
            });

            if (playlist.empty()) {
                std::cerr << "Could not download playlist" << std::endl;
                return EXIT_FAILURE;
            }

            auto rendition = m3u8::parse(playlist);
            if (rendition.empty()) {
                std::cerr << "Could not parse playlist" << std::endl;
                return EXIT_FAILURE;
            }

            ProcessVariant(rendition, redition_url);
        }
    } else {
        ProcessVariant(json, playlist_url);
    }

    return EXIT_SUCCESS;
}
