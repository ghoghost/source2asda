#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace fnx {

struct Pattern {
    std::vector<uint8_t> bytes;
    std::vector<bool> mask; // true = wildcard/ignore
    bool valid = false;
};

Pattern parse_pattern(const std::wstring& pattern_str);

struct PatternMatch {
    size_t offset_in_module = 0; // offset from module base
    uintptr_t address = 0;       // absolute in remote process
};

// Scan a local buffer for the given pattern. base_address is added to offsets to produce absolute addresses.
std::vector<PatternMatch> scan_with_pattern(const void* data, size_t size, uintptr_t base_address, const Pattern& pat);

} // namespace fnx

