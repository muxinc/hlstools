#pragma once
#include <curl/curl.h>
#include <functional>
#include <string>
#include <vector>

class uri {
private:
    // rfc3986
    std::string m_scheme;
    std::string m_authority;
    std::string m_path;
    std::string m_query;
    std::string m_fragment;
    std::string merge(const std::string& path) const;

public:
    uri() = default;
    uri(const std::string& u);
    uri resolve(uri u) const;
    bool is_relative() const { return m_scheme.empty(); }
    bool is_absolute() const { return !(m_scheme.empty() || m_authority.empty()); }
    std::string string() const;

    const std::string& scheme() const { return m_scheme; }
    const std::string& authority() const { return m_authority; }
    const std::string& path() const { return m_path; }
    const std::string& query() const { return m_query; }
    const std::string& fragment() const { return m_fragment; }
};

class net {
public:
    static int download(const uri& u, unsigned int offset, unsigned int size, const std::function<bool(const char*, unsigned int)>& callback);
    static int download(const uri& u, const std::function<bool(const char*, unsigned int)>& callback) { return download(u, 0, 0, callback); }
};
