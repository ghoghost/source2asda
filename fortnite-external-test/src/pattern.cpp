#include "pattern.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace fnx {

static bool hex_char(wchar_t c, uint8_t& out) {
    if (c >= L'0' && c <= L'9') { out = static_cast<uint8_t>(c - L'0'); return true; }
    if (c >= L'a' && c <= L'f') { out = static_cast<uint8_t>(10 + (c - L'a')); return true; }
    if (c >= L'A' && c <= L'F') { out = static_cast<uint8_t>(10 + (c - L'A')); return true; }
    return false;
}

Pattern parse_pattern(const std::wstring& pattern_str) {
    Pattern p;
    // tokenize by whitespace
    size_t i = 0;
    while (i < pattern_str.size()) {
        // skip whitespace
        while (i < pattern_str.size() && iswspace(pattern_str[i])) ++i;
        if (i >= pattern_str.size()) break;
        // read token
        size_t start = i;
        while (i < pattern_str.size() && !iswspace(pattern_str[i])) ++i;
        std::wstring tok = pattern_str.substr(start, i - start);
        if (tok.empty()) continue;
        // check wildcard
        if (tok == L"?" || tok == L"??") {
            p.bytes.push_back(0);
            p.mask.push_back(true);
            continue;
        }
        // hex string of length 2
        if (tok.size() == 2) {
            uint8_t h1 = 0, h2 = 0;
            if (hex_char(tok[0], h1) && hex_char(tok[1], h2)) {
                p.bytes.push_back(static_cast<uint8_t>((h1 << 4) | h2));
                p.mask.push_back(false);
                continue;
            }
        }
        // try concatenated
        if (tok.size() % 2 == 0 && tok.size() >= 2) {
            bool ok = true;
            std::vector<uint8_t> b;
            std::vector<bool> m;
            for (size_t j = 0; j < tok.size(); j += 2) {
                if (tok[j] == L'?' || tok[j + 1] == L'?') {
                    b.push_back(0);
                    m.push_back(true);
                    continue;
                }
                uint8_t h1 = 0, h2 = 0;
                if (!hex_char(tok[j], h1) || !hex_char(tok[j + 1], h2)) { ok = false; break; }
                b.push_back(static_cast<uint8_t>((h1 << 4) | h2));
                m.push_back(false);
            }
            if (ok && !b.empty()) {
                p.bytes = std::move(b);
                p.mask = std::move(m);
                p.valid = true;
                return p;
            }
        }
        // invalid token
        return Pattern{}; // invalid
    }
    p.valid = !p.bytes.empty();
    return p;
}

std::vector<PatternMatch> scan_with_pattern(const void* data, size_t size, uintptr_t base_address, const Pattern& pat) {
    std::vector<PatternMatch> results;
    if (!data || size == 0 || !pat.valid || pat.bytes.empty()) return results;
    if (pat.bytes.size() > size) return results;
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    const size_t plen = pat.bytes.size();
    for (size_t i = 0; i <= size - plen; ++i) {
        bool match = true;
        for (size_t j = 0; j < plen; ++j) {
            if (pat.mask[j]) continue;
            if (bytes[i + j] != pat.bytes[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            PatternMatch m;
            m.offset_in_module = i;
            m.address = base_address ? (base_address + i) : 0;
            results.push_back(m);
        }
    }
    return results;
}

} // namespace fnx

