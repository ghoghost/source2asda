#include "diag.h"

#include "pattern.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <iostream>
#include <vector>

namespace fnx {

static void wlog(const std::wstring& s) {
    std::wcout << s << L"\n";
}

static std::wstring hex64(uintptr_t v) {
    wchar_t buf[32];
    swprintf_s(buf, L"0x%llX", static_cast<unsigned long long>(v));
    return buf;
}

int run_self_test() {
    wlog(L"== Self-test ==");

    // 1) Pattern scan against local buffer
    uint8_t local_buf[64] = {0};
    const uint8_t seq[] = {0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x20,0x8B,0xF2,0x48,0x8B,0xD9};
    std::memcpy(local_buf + 10, seq, sizeof(seq));

    Pattern pat = parse_pattern(L"48 89 5C 24 08 57 48 83 EC 20 8B ?? 48 8B D9");
    if (!pat.valid) {
        wlog(L"SELF_TEST: FAIL (pattern parser)");
        return 1;
    }
    uintptr_t fake_base = 0x140000000ULL;
    auto matches = scan_with_pattern(local_buf, sizeof(local_buf), fake_base, pat);
    if (matches.size() != 1 || matches[0].offset_in_module != 10 || matches[0].address != fake_base + 10) {
        wlog(L"SELF_TEST: FAIL (pattern scan)");
        wlog(L"  matches=" + std::to_wstring(matches.size()));
        if (!matches.empty()) wlog(L"  first_offset=" + std::to_wstring(matches[0].offset_in_module));
        return 1;
    }
    wlog(L"Pattern scan: PASS (match at offset 10, addr " + hex64(matches[0].address) + L")");

    // 2) Pointer chain read in own process memory
    volatile uint64_t value = 0x1122334455667788ULL;
    volatile uint64_t* level1 = const_cast<volatile uint64_t*>(&value);
    volatile uint64_t** root = const_cast<volatile uint64_t**>(&level1);

    Memory mem(GetCurrentProcess());
    std::wstring err;
    auto p1 = mem.read_value<uint64_t>(reinterpret_cast<uintptr_t>(root), err);
    if (!p1 || *p1 != reinterpret_cast<uint64_t>(level1)) {
        wlog(L"SELF_TEST: FAIL (read root pointer): " + err);
        return 1;
    }
    auto p2 = mem.read_value<uint64_t>(*p1, err);
    if (!p2 || *p2 != value) {
        wlog(L"SELF_TEST: FAIL (read value): " + err);
        return 1;
    }
    wlog(L"Pointer chain read: PASS (value " + hex64(static_cast<uintptr_t>(*p2)) + L")");

    // 3) Self-module enumeration sanity (current process)
    Process self;
    if (!self.attach(GetCurrentProcessId(), err)) {
        wlog(L"SELF_TEST: FAIL (self attach): " + err);
        return 1;
    }
    auto mods = self.modules(err);
    if (mods.empty()) {
        wlog(L"SELF_TEST: FAIL (self module enum): " + err);
        return 1;
    }
    wlog(L"Self module enumeration: PASS (" + std::to_wstring(mods.size()) + L" modules, main base " + hex64(mods[0].base) + L")");

    wlog(L"SELF_TEST: PASS");
    return 0;
}

static std::wstring status_for_pattern(const PatternEntry& /*entry*/, bool parsed_ok, bool placeholder) {
    if (placeholder) return L"MISSING_USER_PATTERN";
    if (!parsed_ok) return L"INVALID_PATTERN";
    return L"READY";
}

int run_diagnostics(const DiagOptions& opts) {
    wlog(L"== Diagnostics ==");
    if (!opts.diagnose) {
        wlog(L"No --diagnose flag; nothing to do.");
        return 2;
    }

    std::wstring err;
    Process proc;
    if (opts.pid != 0) {
        if (!proc.attach(opts.pid, err)) {
            wlog(L"ERROR: " + err);
            return 1;
        }
    } else if (!opts.process_name.empty()) {
        if (!proc.attach_by_name(opts.process_name, err)) {
            wlog(L"ERROR: " + err);
            return 1;
        }
    } else {
        wlog(L"ERROR: specify --process <name.exe> or --pid <id> for diagnostics.");
        return 2;
    }
    wlog(L"Attached: PID " + std::to_wstring(proc.pid()));

    DriverClient drv;
    bool use_driver = !opts.driver_device_path.empty();
    if (use_driver) {
        if (!drv.open(opts.driver_device_path, err)) {
            wlog(L"ERROR opening driver device '" + std::wstring(opts.driver_device_path.begin(), opts.driver_device_path.end()) + L"': " + err);
            return 1;
        }
        uint32_t maj = 0, min = 0;
        if (!drv.get_version(maj, min, err)) {
            wlog(L"ERROR querying driver version: " + err);
            return 1;
        }
        wlog(L"Driver backend: CONNECTED (version " + std::to_wstring(maj) + L"." + std::to_wstring(min) + L", device " +
             std::wstring(opts.driver_device_path.begin(), opts.driver_device_path.end()) + L")");
    } else {
        wlog(L"Driver backend: not used (user-mode ReadProcessMemory)");
    }

    auto mods = proc.modules(err);
    if (mods.empty() && !err.empty()) {
        wlog(L"ERROR enumerating modules: " + err);
        return 1;
    }
    wlog(L"Modules: " + std::to_wstring(mods.size()));
    for (size_t i = 0; i < mods.size() && i < 32; ++i) {
        const auto& m = mods[i];
        wlog(L"  [" + std::to_wstring(i) + L"] " + m.name + L" base=" + hex64(m.base) + L" size=0x" + [&](){
            wchar_t b[32]; swprintf_s(b, L"%llX", static_cast<unsigned long long>(m.size)); return std::wstring(b);
        }());
    }
    if (mods.size() > 32) wlog(L"  ... (" + std::to_wstring(mods.size() - 32) + L" more)");

    if (opts.config_path.empty()) {
        wlog(L"No config; pattern scan skipped.");
        return 0;
    }

    Config cfg = load_config(opts.config_path);
    if (!cfg.loaded) {
        wlog(L"Config load error: " + cfg.load_error);
        return 1;
    }
    wlog(L"Config loaded: " + opts.config_path + L" (process_hint=" + cfg.process_name + L", patterns=" + std::to_wstring(cfg.patterns.size()) + L")");

    Memory mem(proc.handle());
    auto read_bytes_any = [&](uintptr_t addr, size_t size, std::wstring& e) -> std::optional<std::vector<uint8_t>> {
        if (use_driver) return drv.read_bytes(proc.pid(), addr, size, e);
        return mem.read_bytes(addr, size, e);
    };
    auto read_i32_any = [&](uintptr_t addr, std::wstring& e) -> std::optional<int32_t> {
        int32_t v = 0;
        if (use_driver) {
            if (!drv.read(proc.pid(), addr, &v, sizeof(v), e)) return std::nullopt;
            return v;
        }
        return mem.read_value<int32_t>(addr, e);
    };

    for (const auto& entry : cfg.patterns) {
        bool placeholder = is_placeholder_pattern(entry.pattern);
        Pattern pat = placeholder ? Pattern{} : parse_pattern(entry.pattern);
        wlog(L"Pattern '" + entry.name + L"' module=" + entry.module + L" status=" + status_for_pattern(entry, pat.valid, placeholder));
        if (placeholder) {
            wlog(L"  -> no pattern configured");
            continue;
        }
        if (!pat.valid) {
            wlog(L"  -> invalid pattern");
            continue;
        }
        auto mod = proc.find_module(entry.module, err);
        if (!mod) {
            wlog(L"  -> MODULE_NOT_FOUND: " + entry.module + (err.empty() ? L"" : (L" (" + err + L")")));
            continue;
        }
        err.clear();
        auto bytes = read_bytes_any(mod->base, mod->size, err);
        if (!bytes) {
            wlog(L"  -> READ_FAILED: " + err);
            continue;
        }
        auto matches = scan_with_pattern(bytes->data(), bytes->size(), mod->base, pat);
        if (matches.empty()) {
            wlog(L"  -> no match");
            continue;
        }
        wlog(L"  -> OK matches=" + std::to_wstring(matches.size()) + L" first=" + hex64(matches[0].address) + L" (module+0x" + [&](){
            wchar_t b[32]; swprintf_s(b, L"%llX", static_cast<unsigned long long>(matches[0].offset_in_module)); return std::wstring(b);
        }() + L")");
        if (entry.rip_relative) {
            uintptr_t disp_addr = matches[0].address + static_cast<uintptr_t>(entry.disp_offset);
            auto disp = read_i32_any(disp_addr, err);
            if (!disp) {
                wlog(L"  -> RIP resolve failed reading disp32 at " + hex64(disp_addr) + L": " + err);
            } else {
                uintptr_t target = matches[0].address + static_cast<uintptr_t>(entry.disp_offset) + 4ULL + static_cast<int64_t>(*disp) + static_cast<int64_t>(entry.add_offset);
                wlog(L"  -> RIP target=" + hex64(target) + L" (disp32=" + std::to_wstring(*disp) + L")");
            }
        }
        if (matches.size() > 1) {
            wlog(L"  -> multiple matches, pattern is not unique");
        }
    }
    return 0;
}

} // namespace fnx
