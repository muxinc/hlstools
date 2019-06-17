#include "m3u8.hpp"
#include "netutils.hpp"
#include <chrono>
#include <functional>
#include <iostream>
#include <vector>

///////////////////////////////////////////////////////////////////////////////////////////////////
int ProcessVariant(const uri& redition_url)
{
    for(int seg_no = -1;;) {
        std::string playlist;
        net::download(redition_url, [&playlist](const char* data, unsigned int size) -> bool {
            static const size_t maxplaylistSize = 10 * 1024 * 1024;
            playlist.insert(playlist.end(), data, data + size);
            return playlist.size() < maxplaylistSize;
        });

        if (playlist.empty()) {
            std::cerr << "Could not download playlist" << std::endl;
            return EXIT_FAILURE;
        }

        auto json = m3u8::parse(playlist);
        if (json.empty()) {
            std::cerr << "Could not parse playlist" << std::endl;
            return EXIT_FAILURE;
        }

        const auto& EXTINF = json.find("INF");
        const auto media_sequence = json.find("X-MEDIA-SEQUENCE")->get<int>();
        std::cerr << media_sequence << " " << EXTINF->size() << std::endl;
        if(0 > seg_no) {
            seg_no = media_sequence + EXTINF->size() - 1;
        }

        if (media_sequence > seg_no) {
            seg_no = media_sequence;
        }

        for (; seg_no < media_sequence + EXTINF->size(); ++seg_no){
            int seg_idx = seg_no - media_sequence;
            auto seg = EXTINF->at(seg_idx);
            auto segment_uri = redition_url.resolve(uri(seg["URI"]));
            auto byterange = seg.find("BYTE-RANGE");
            size_t offset = 0, size = 0;
            if (byterange != seg.end()) {
                if (2 == byterange->size()) {
                    offset = byterange->at(0).get<int>();
                    size = byterange->at(1).get<int>();
                }
            }

            auto start = std::chrono::steady_clock::now();
              std::cerr << seg_idx << " " << segment_uri.string() << std::endl;

            net::download(segment_uri, offset, size, [&start](const char* data, size_t size) -> bool {
                auto now = std::chrono::steady_clock::now();
                auto elapased = now - start;
                fwrite(data,1,size,stdout);
                std::cerr << elapased.count() << std::endl;
                return true;
            });
        }
    }

    return EXIT_SUCCESS;
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
            // std::cerr << redition_url.string() << std::endl;
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

            return ProcessVariant(redition_url);
        }
    } else {
        return ProcessVariant(playlist_url);
    }

    return EXIT_SUCCESS;
}
