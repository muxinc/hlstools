/*!re2c
    yyyy = ('-'|[-+]?[0-9]{4});
    mo = ([0][1-9] | [1][0-2]);
    dd = ([0][1-9] | [12][0-9] | [3][0-1]);
    wk = ([0][1-9] | [1-4][0-9] | [5][0-3]) ([-]? [0-7])?;
    // TODO Ordinal date
    date = yyyy '-' (wk | (mo '-' dd));

    hh = ([01][0-9] | [2][0-4]);
    mm = ([0-5][0-9]);
    ss = ([0-5][0-9]);
    ms = ([0-9]+);
    time = 'T' (hh ':' mm ':' ss '.' ms | hh mm ss '.' ms | hh ':' mm ':' ss | hh mm ss | hh ':' mm | hh mm | hh);

    timezone = ('Z'|[-+][0-9]{2}(':'[0-9]{2})?);
    iso8601 = date (time? timezone)?;

    endl = ("\r\n" | [\r\n] | [\x00]);
    decimal_integer = ("0" | [1-9][0-9]*);
    signed_decimal_integer = [+-]? decimal_integer;
    floating_point = signed_decimal_integer ("." [0-9]+)?;
    quoted_string = ([\x22] [^\r\n\x22\x00]* [\x22]);
    enumerated_string = ([^\r\n\t\x20\x22\x2c\x3d\x00]+);

    attribute_name = ([-A-Z0-9]+);
    attribute_value = (quoted_string | enumerated_string);
    attribute_list = (#keyStart attribute_name #keyEnd "=" #valStart attribute_value #valEnd) ','?;

    title = [^\r\n\x00]*;
    uri = [^\r\n\x00]+;
*/

#include "m3u8.hpp"
using mtag_t = std::pair<int, const char*>;
static nlohmann::json render_attribute_list(const std::vector<mtag_t>& list, int keyStart, int keyEnd, int valStart, int valEnd)
{
    if (0 > keyStart || 0 > valStart) {
        return nlohmann::json::object();
    }

    auto json = render_attribute_list(list, list[keyStart].first, list[keyEnd].first, list[valStart].first, list[valEnd].first);
    auto key = std::string(list[keyStart].second, list[keyEnd].second);
    auto val = std::string(list[valStart].second, list[valEnd].second);
    json.emplace(std::move(key), std::move(val));
    return json;
}
// https://tools.ietf.org/html/rfc8216
nlohmann::json m3u8::parse(const char* YYCURSOR)
{
    int keyindex = -1, mapindex = -1;
    int segment_size = 0, segment_offset = 0;
    auto json = nlohmann::json::object();
    auto key = nlohmann::json::array();
    auto map = nlohmann::json::array();
    auto media = nlohmann::json::object();
    auto inf = nlohmann::json::array();
    auto streaminf = nlohmann::json::array();
    bool discontinuity = false;
    const char* YYMARKER = YYCURSOR;
    const char *a, *b, *c, *d, *e;
    int keyStart, keyEnd, valStart, valEnd;
    std::vector<mtag_t> attrlist;
    std::string program_date_time;
    const char* end = YYCURSOR + strlen(YYCURSOR);

#define YYMTAGP(t) mtag(&t, YYCURSOR);
#define YYMTAGN(t) mtag(&t, nullptr);
    auto mtag = [&attrlist](int* pmt, const char* YYCURSOR) {
        attrlist.push_back({ (*pmt), YYCURSOR });
        *pmt = attrlist.size() - 1;
    };

    /*!mtags:re2c format = "int @@;\n"; */
    /*!stags:re2c format = "const char * @@;"; */
    while (YYCURSOR < end) {
        attrlist.clear();
        /*!mtags:re2c format = "@@ = -1;"; */
        /*!stags:re2c format = "@@ = nullptr;"; */
        /*!re2c
        re2c:flags:tags = 1;
        re2c:yyfill:enable = 0;
        re2c:define:YYCTYPE = char;
        re2c:flags:bit-vectors = 1;

        // Special cases:
        "#EXTINF:" @a floating_point (',' @b title @c)? endl @d uri @e endl {
            inf.push_back({
                {"DURATION", std::atof(a)},
                {"URI", std::string(d, e)},
             });

            if(c > b) { inf.back().push_back({"TITLE", std::string(b, c)}); }
            if(0 <= keyindex) { inf.back().push_back({"KEY", keyindex}); }
            if(0 <= mapindex) { inf.back().push_back({"MAP", mapindex}); }

            if(discontinuity) {
                inf.back().push_back({"DISCONTINUITY", true});
                discontinuity = false;
            }

            if(!program_date_time.empty()) {
                inf.back().push_back({"PROGRAM-DATE-TIME", std::move(program_date_time)});
                program_date_time.clear();
            }

            if(0 < segment_size) {
                inf.back().push_back({"BYTERANGE", {segment_size, segment_offset}});
                segment_offset += segment_size;
                segment_size = 0;
            } else {
                segment_offset = 0;
            }

            continue;
        }

        "#EXT-X-PREFETCH:" @a uri @b endl {
            inf.push_back({
                {"PREFETCH", true},
                {"URI", std::string(a, b)},
             });

            if(discontinuity) {
                inf.back().emplace("DISCONTINUITY", true);
                discontinuity = false;
            }

            continue;
        }

        "#EXT-X-MEDIA:" attribute_list+ endl {
            // Move the GROUP-ID to be the key for the object, to make lookup of groups eaiser
            auto group = render_attribute_list(attrlist, keyStart, keyEnd, valStart, valEnd);
            auto group_id = group.find("GROUP-ID");
            if(group.end() != group_id) {
                auto name = (*group_id);
                group.erase(group_id);
                media.emplace(std::move(name), std::move(group));
            }
            continue;
        }

        "#EXT-X-STREAM-INF:" attribute_list+ endl @a uri @b endl {
            streaminf.push_back( {
                { "ATTRIBUTES", render_attribute_list(attrlist, keyStart, keyEnd, valStart, valEnd) },
                { "URI", std::string(a, b) }
            });
            continue;
        }

        "#EXT-X-KEY:" attribute_list+ endl {
            auto attrs = render_attribute_list(attrlist, keyStart, keyEnd, valStart, valEnd);
            auto method = attrs.find("METHOD");
            if(attrs.end() == method || "NONE" == (*method) ) {
                keyindex = -1;
            } else {
                key.push_back({std::move(attrs)});
                keyindex = key.size() - 1;
            }
            continue;
        }

        "#EXT-X-KEY:" attribute_list+ endl {
            auto attrs = render_attribute_list(attrlist, keyStart, keyEnd, valStart, valEnd);
            map.push_back({std::move(attrs)});
            index = map.size() - 1;
            continue;
        }

        // #EXT-X-DISCONTINUITY, #EXT-X-PREFETCH-DISCONTINUITY
        "#EXT-X-" "PREFETCH-"? "DISCONTINUITY" endl { discontinuity = true; continue; }
        "#EXT-X-PROGRAM-DATE-TIME:" @a iso8601 @b endl { program_date_time = std::string(a, b); continue; }
        "#EXT-X-PLAYLIST-TYPE:" @a ("EVENT" | "VOD") @b endl { json.emplace("X-PLAYLIST-TYPE", std::string(a, b)); continue; }
        "#EXT-X-BYTERANGE:" @a decimal_integer ("@" @b decimal_integer)? endl { segment_size  = std::atoi(a); segment_offset  = b ? std::atoi(b) : 0; continue; }

        // #EXT-X-MEDIA-SEQUENCE, #EXT-X-TARGETDURATION, #EXT-X-VERSION, #EXT-X-DISCONTINUITY-SEQUENCE
        "#EXT-" @a [A-Z][-A-Z]*  @b ":" @c  decimal_integer endl {
            json.emplace(std::string(a, b), std::atof(c));
            continue;
        }

        // #EXT-X-START, #EXT-X-DATERANGE, #EXT-X-SESSION-DATA, #EXT-X-SESSION-KEY
        "#EXT-"  @a [A-Z][-A-Z]* @b ":" attribute_list+ endl {
            json.emplace(std::string(a, b), render_attribute_list(attrlist, keyStart, keyEnd, valStart, valEnd));
            continue;
        }

        // #EXT-X-ENDLIST, #EXT-X-I-FRAMES-ONLY, #EXT-X-INDEPENDENT-SEGMENTS
        "#EXT-" @a [A-Z][-A-Z]* @b endl {
            json.emplace(std::string(a, b), true);
            continue;
        }

        // Other
        "#" [^\r\n\x00]* endl { continue; } // Comment
        [ \t]* endl { continue; } // Whitespace
        [\x00] { goto done; } // eof
        * { goto parse_error; } // Something else
        */
    }

done:
    // If these values are set, they were not consumed.
    if (discontinuity || segment_size || !program_date_time.empty()) {
        return nlohmann::json {};
    }

    // can not be both master and varient
    if ((streaminf.empty() && inf.empty()) || (!streaminf.empty() && !inf.empty())) {
        return nlohmann::json {};
    }

    if (streaminf.empty()) {
        json.emplace("INF", std::move(inf));

        if (!key.empty()) {
            json.emplace("X-KEY", std::move(key));
        }

        if (!map.empty()) {
            json.emplace("X-MAP", std::move(map));
        }

        if (json.end() == json.find("X-ENDLIST")) {
            json.emplace("X-ENDLIST", false);
        }
    } else {
        json.emplace("X-STREAM-INF", std::move(streaminf));

        if (!media.empty()) {
            json.emplace("X-MEDIA", std::move(media));
        }
    }

    return json;
parse_error:
    return nlohmann::json {};
}

std::string render_attribute_list(const nlohmann::json& list)
{
    std::string str;
    for (auto pair = list.begin(); pair != list.end(); ++pair) {
        if (pair != list.begin()) {
            str += ",";
        }

        str += pair.key() + "=";
        if (pair.value().is_number()) {
            auto num = pair.value().get<double>();
            if (num == std::floor(num)) {
                str += std::to_string(static_cast<int64_t>(num));
            } else {
                str += std::to_string(static_cast<int64_t>(num));
            }
        } else if (pair.value().is_string()) {
            str += pair.value().get<std::string>();
        }
    }

    return str;
}
