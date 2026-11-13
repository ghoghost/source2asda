#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace fnx {

struct PatternEntry {
    std::wstring name;
    std::wstring module;
    std::wstring pattern;
    bool rip_relative = false;
    int disp_offset = 0; // offset of disp32 within instruction (for RIP-relative)
    int add_offset = 0;  // extra offset added after resolution
};

struct Config {
    std::wstring process_name;
    std::vector<PatternEntry> patterns;
    bool loaded = false;
    std::wstring load_error;
};

// Minimal INI parser: [process], [pattern.<name>], key=value
Config load_config(const std::wstring& path);

bool is_placeholder_pattern(const std::wstring& pattern);

} // namespace fnx
