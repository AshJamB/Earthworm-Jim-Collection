// Platform-independent helpers for the updater: version comparison and the little JSON scraping needed for
// GitHub's "latest release" reply. Header-only so they can be unit-tested without Windows (tests/update_test.cpp).
#pragma once
#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

namespace upd {

// "v1.2.3" -> {1,2,3}. Anything after a non-numeric character in a part is ignored ("1.2.3-beta" -> 1,2,3).
inline std::vector<int> parse_version(const std::string& s) {
    std::vector<int> parts;
    size_t i = 0;
    if (i < s.size() && (s[i] == 'v' || s[i] == 'V')) i++;
    while (i < s.size()) {
        if (!isdigit((unsigned char)s[i])) break;
        int v = 0;
        while (i < s.size() && isdigit((unsigned char)s[i])) v = v * 10 + (s[i++] - '0');
        parts.push_back(v);
        if (i < s.size() && s[i] == '.') i++; else break;
    }
    return parts;
}

// True if version `a` is newer than `b`. Unparseable versions are never newer.
inline bool version_newer(const std::string& a, const std::string& b) {
    std::vector<int> x = parse_version(a), y = parse_version(b);
    if (x.empty() || y.empty()) return false;
    size_t n = x.size() > y.size() ? x.size() : y.size();
    for (size_t i = 0; i < n; i++) {
        int xi = i < x.size() ? x[i] : 0, yi = i < y.size() ? y[i] : 0;
        if (xi != yi) return xi > yi;
    }
    return false;
}

// The string value of the first `"key": "value"` at or after `from` ("" if none). Handles \" and \\ escapes only,
// which is all GitHub puts in the fields read here.
inline std::string json_string(const std::string& json, const std::string& key, size_t from = 0, size_t* end = nullptr) {
    const std::string needle = "\"" + key + "\"";
    size_t p = json.find(needle, from);
    while (p != std::string::npos) {
        size_t q = p + needle.size();
        while (q < json.size() && isspace((unsigned char)json[q])) q++;
        if (q < json.size() && json[q] == ':') {
            q++;
            while (q < json.size() && isspace((unsigned char)json[q])) q++;
            if (q < json.size() && json[q] == '"') {
                std::string out;
                for (q++; q < json.size() && json[q] != '"'; q++) {
                    if (json[q] == '\\' && q + 1 < json.size()) q++;
                    out += json[q];
                }
                if (end) *end = q;
                return out;
            }
        }
        p = json.find(needle, p + needle.size());
    }
    return "";
}

inline bool ends_with(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// The first release asset download URL ending with `suffix` (e.g. "-win64.zip"), or "".
inline std::string find_asset_url(const std::string& json, const std::string& suffix) {
    size_t pos = 0;
    for (;;) {
        size_t e = 0;
        std::string url = json_string(json, "browser_download_url", pos, &e);
        if (url.empty()) return "";
        if (ends_with(url, suffix)) return url;
        pos = e + 1;
    }
}

}  // namespace upd
