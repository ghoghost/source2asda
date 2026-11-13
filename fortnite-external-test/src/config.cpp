#include "config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>

namespace fnx {

static std::wstring trim(const std::wstring& s) {
    size_t b = 0, e = s.size();
    while (b < e && iswspace(s[b])) ++b;
    while (e > b && iswspace(s[e - 1])) --e;
    return s.substr(b, e - b);
}

static std::wstring lower(const std::wstring& s) {
    std::wstring t = s;
    std::transform(t.begin(), t.end(), t.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return t;
}

bool is_placeholder_pattern(const std::wstring& pattern) {
    std::wstring p = lower(pattern);
    if (p.find(L"supply") != std::wstring::npos) return true;
    if (p.find(L"todo") != std::wstring::npos) return true;
    if (p.find(L"example") != std::wstring::npos) return true;
    if (p.empty()) return true;
    return false;
}

Config load_config(const std::wstring& path) {
    Config cfg;
    std::wifstream in(path);
    if (!in.is_open()) {
        cfg.loaded = false;
        cfg.load_error = L"Failed to open config: " + path;
        return cfg;
    }
    std::wstring line;
    std::wstring current_section;
    PatternEntry current;
    bool in_pattern = false;
    while (std::getline(in, line)) {
        // strip comments ; #
        size_t sc = line.find_first_of(L";#");
        if (sc != std::wstring::npos) line = line.substr(0, sc);
        line = trim(line);
        if (line.empty()) continue;
        if (line.size() >= 2 && line.front() == L'[' && line.back() == L']') {
            // save previous pattern if any
            if (in_pattern && !current.name.empty()) {
                cfg.patterns.push_back(current);
            }
            current_section = line.substr(1, line.size() - 2);
            current = PatternEntry();
            in_pattern = false;
            std::wstring ls = lower(current_section);
            if (ls.size() >= 8 && ls.substr(0, 8) == L"pattern.") {
                current.name = current_section.substr(8);
                in_pattern = true;
            }
            continue;
        }
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring key = trim(line.substr(0, eq));
        std::wstring val = trim(line.substr(eq + 1));
        std::wstring lk = lower(key);
        if (current_section == L"process" || lower(current_section) == L"process") {
            if (lk == L"name" || lk == L"process") cfg.process_name = val;
            continue;
        }
        if (in_pattern) {
            if (lk == L"module") current.module = val;
            else if (lk == L"pattern") current.pattern = val;
            else if (lk == L"rip_relative") current.rip_relative = (val == L"1" || lower(val) == L"true");
            else if (lk == L"disp_offset") current.disp_offset = _wtoi(val.c_str());
            else if (lk == L"add_offset") current.add_offset = _wtoi(val.c_str());
        }
    }
    if (in_pattern && !current.name.empty()) {
        cfg.patterns.push_back(current);
    }
    cfg.loaded = true;
    cfg.load_error.clear();
    return cfg;
}

} // namespace fnx
