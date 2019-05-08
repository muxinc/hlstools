![HLS Tools](https://banner.mux.dev/HLS%20Tools.svg)

# hlstools

Tools for analyzing and processing HLS streams. See our blog post on [measuring package overhead](https://mux.com/blog/quantifying-packaging-overhead-2/) for more details on how we use this internally.

A central repository for tools to analyse and process HLS streams. Think Apple's [HTTP Live Streaming Tools](https://developer.apple.com/documentation/http_live_streaming/about_apple_s_http_live_streaming_tools) but with just one of the packages...for now. :imp:

* `muxincstreamvalidator` - A tool to analyse HLS streams. Similar to Apple's mediastreamvalidator, but works on non-Apple platforms and supports measuring TS packaging overhead.


These tools use the excellent [JSON for Modern C++](https://github.com/nlohmann/json) and is available under the [MIT](./LICENSE).
