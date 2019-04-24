#include "netutils.hpp"
#include <numeric>

static std::string tos(const char* b, const char* e)
{
    return (b && e && b < e) ? std::string(b, e) : std::string();
}

uri::uri(const std::string& u)
{
    const char* YYCURSOR = u.c_str();
    const char* YYMARKER = YYCURSOR;
    const char *scheme_b = nullptr, *scheme_e = nullptr;
    const char *authority_b = nullptr, *authority_e = nullptr;
    const char *path_b = nullptr, *path_e = nullptr;
    const char *query_b = nullptr, *query_e = nullptr;
    const char *fragment_b = nullptr, *fragment_e = nullptr;
    /*!stags:re2c format = "const char * @@;"; */
    /*!stags:re2c format = "@@ = nullptr;"; */
    /*!re2c
    re2c:flags:tags = 1;
    re2c:yyfill:enable = 0;
    re2c:define:YYCTYPE = char;
    re2c:flags:bit-vectors = 1;
    * {return;}
    ( @scheme_b [^:/?#\x00]+ @scheme_e ":")?
    ("//" @authority_b [^/?#\x00]* @authority_e )?
    (@path_b [^?#\x00]* @path_e )
    ("?" @query_b [^#\x00]* @query_e )?
    ("#" @fragment_b [^\x00]* @fragment_e )? {
        m_scheme = tos(scheme_b, scheme_e);
        m_authority = tos(authority_b, authority_e);
        m_path = tos(path_b, path_e);
        m_query = tos(query_b, query_e);
        m_fragment = tos(fragment_b, fragment_e);
        return;
    }
    */
}

std::string uri::merge(const std::string& path) const
{
    // If the base URI has a defined authority component and an empty
    // path, then return a string consisting of "/" concatenated with the
    // reference's path
    if (!m_authority.empty() && m_path.empty()) {
        return "/" + path;
    }

    // return a string consisting of the reference's path component
    // appended to all but the last segment of the base URI's path (i.e.,
    // excluding any characters after the right-most "/" in the base URI
    // path, or excluding the entire base URI path if it does not contain
    // any "/" characters).
    auto end = m_path.find_last_of("/");
    if (std::string::npos != end) {
        return m_path.substr(0, end + 1) + path;
    }

    return path;
}

static std::string remove_dot_segments(const std::string& path)
{
    // While the input buffer is not empty, loop as follows:
    const char* YYCURSOR = path.c_str();
    const char* YYMARKER = YYCURSOR;
    std::vector<std::string> segments;
    const char *a = nullptr, *b = nullptr;
    while (true) {
        /*!stags:re2c format = "const char * @@;"; */
        /*!stags:re2c format = "@@ = nullptr;"; */
        /*!re2c
        re2c:flags:tags = 1;
        re2c:yyfill:enable = 0;
        re2c:define:YYCTYPE = char;
        re2c:flags:bit-vectors = 1;

        // End of string
        "\x00" { goto done; }
        "/" / "/" { continue; } // Multiple slashes, eat one
        * { return std::string(); }

        // A.  If the input buffer begins with a prefix of "../" or "./",
        // then remove that prefix from the input buffer; otherwise,
        "."{1,2} / "/" {
            continue;
        }

        // B.  if the input buffer begins with a prefix of "/./" or "/.",
        // where "." is a complete path segment, then replace that
        // prefix with "/" in the input buffer; otherwise,
        "/." / [/\x00] {
            continue;
        }

        // C.  if the input buffer begins with a prefix of "/../" or "/..",
        // where ".." is a complete path segment, then replace that
        // prefix with "/" in the input buffer and remove the last
        // segment and its preceding "/" (if any) from the output
        // buffer; otherwise,
        "/.." / [/\x00] {
            if(!segments.empty()) {
                    segments.pop_back();
            }

            continue;
        }

        // D.  if the input buffer consists only of "." or "..", then remove
        // that from the input buffer; otherwise,
        "."{1,2} "\x00" {
            continue;
        }

        // E.  move the first path segment in the input buffer to the end of
        // the output buffer, including the initial "/" character (if
        // any) and any subsequent characters up to, but not including,
        // the next "/" character or the end of the input buffer.
        path_segment = @a [^?#\x00][^/?#\x00]* @b;
        path_segment / [/\x00] {
            segments.emplace_back( std::string(a, b) );
            continue;
        }
        */
    }
done:
    return std::accumulate(segments.begin(), segments.end(), std::string(""));
}

uri uri::resolve(uri u) const
{
    if (u.m_scheme.empty()) {
        u.m_scheme = m_scheme;
        if (u.m_authority.empty()) {
            u.m_authority = m_authority;
            if (u.m_path.empty()) {
                u.m_path = m_path;
                if (u.m_query.empty()) {
                    u.m_query = m_query;
                }
            } else if ('/' != u.m_path.front()) {
                u.m_path = merge(u.m_path);
            }
        }

        u.m_path = remove_dot_segments(u.m_path);
    }

    return u;
}

std::string uri::string() const
{
    std::string str;
    str += !m_scheme.empty() ? m_scheme + "://" : std::string();
    str += m_authority;
    str += !m_authority.empty() && m_path.empty() ? std::string("/") : m_path;
    str += !m_query.empty() ? "?" + m_query : std::string();
    str += !m_fragment.empty() ? "#" + m_fragment : std::string();
    return str;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
size_t _write_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    size *= nmemb;
    auto callback = static_cast<std::function<bool(char*, size_t)>*>(userdata);
    if ((*callback)(ptr, size)) {
        return size;
    }

    return 0;
}

int net::download(const uri& u, unsigned int offset, unsigned int size, const std::function<bool(const char*, unsigned int)>& callback)
{
    long response_code = 0;
    CURL* curl = nullptr;
    CURLcode err = CURLE_OK;
    char range[32];
    if (!(curl = curl_easy_init())) {
        err = CURLE_FAILED_INIT;
        curl_easy_reset(curl);
        return -err;
    }

    if (CURLE_OK != (err = curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L))) {
        curl_easy_reset(curl);
        return -err;
    }

    if (CURLE_OK != (err = curl_easy_setopt(curl, CURLOPT_URL, u.string().c_str()))) {
        curl_easy_reset(curl);
        return -err;
    }

    if (CURLE_OK != (err = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _write_callback))) {
        curl_easy_reset(curl);
        return -err;
    }

    if (CURLE_OK != (err = curl_easy_setopt(curl, CURLOPT_WRITEDATA, static_cast<const void*>(&callback)))) {
        curl_easy_reset(curl);
        return -err;
    }

    if (0 < offset || 0 < size) {
        if (0 < size) {
            snprintf(range, sizeof(range), "%u-%u", offset, size);
        } else {
            snprintf(range, sizeof(range), "%u-", offset);
        }

        if (CURLE_OK != (err = curl_easy_setopt(curl, CURLOPT_RANGE, range))) {
            curl_easy_reset(curl);
            return -err;
        }
    }

    if (CURLE_OK != (err = curl_easy_perform(curl))) {
        curl_easy_reset(curl);
        return -err;
    }

    if (CURLE_OK != (err = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code))) {
        curl_easy_reset(curl);
        return -err;
    }

    return response_code;
}
