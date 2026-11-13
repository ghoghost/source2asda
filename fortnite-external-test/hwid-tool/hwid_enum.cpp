// hwid_enum.cpp - Read-only HWID/serial discovery across common Windows user-mode observation paths
// Purpose: evidence gathering for authorized, reversible identity virtualization testing.
// No spoofing performed. No anti-cheat behavior assumed or fabricated.
//
// Build (MSVC x64 Developer Command Prompt):
//   cl /nologo /std:c++17 /EHsc /W4 /DUNICODE /D_UNICODE hwid_enum.cpp /Fe:hwid_enum.exe /link iphlpapi.lib advapi32.lib

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iphlpapi.h>
#include <intrin.h>
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "advapi32.lib")

static void wlog(const std::wstring& s) { std::wcout << s << L"\n"; }

static std::wstring to_hex(const uint8_t* data, size_t len, bool spaced = false) {
    std::wostringstream oss;
    oss << std::hex << std::uppercase << std::setfill(L'0');
    for (size_t i = 0; i < len; ++i) {
        if (spaced && i) oss << L' ';
        oss << std::setw(2) << (int)data[i];
    }
    return oss.str();
}

static std::wstring trim_ws(std::wstring s) {
    auto notspace = [](wchar_t c){ return !iswspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
    return s;
}

static void probe_computer_name() {
    wlog(L"\n== Computer/Host Name ==");
    wchar_t buf[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD sz = ARRAYSIZE(buf);
    if (GetComputerNameW(buf, &sz)) wlog(L"  GetComputerName: " + std::wstring(buf));
    else wlog(L"  GetComputerName failed: Win32=" + std::to_wstring(GetLastError()));
    wchar_t dns[256] = {}; DWORD dsz = 256;
    if (GetComputerNameExW(ComputerNameDnsHostname, dns, &dsz)) wlog(L"  DNS Hostname: " + std::wstring(dns));
}

static void probe_machine_guid() {
    wlog(L"\n== Machine GUID (Registry: HKLM\\SOFTWARE\\Microsoft\\Cryptography\\MachineGuid) ==");
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS) {
        wlog(L"  RegOpenKeyEx failed: Win32=" + std::to_wstring(GetLastError()));
        return;
    }
    wchar_t val[128] = {}; DWORD type = 0, cb = sizeof(val);
    if (RegQueryValueExW(key, L"MachineGuid", nullptr, &type, (LPBYTE)val, &cb) == ERROR_SUCCESS) {
        wlog(L"  MachineGuid: " + std::wstring(val));
    } else {
        wlog(L"  RegQueryValueEx(MachineGuid) failed: Win32=" + std::to_wstring(GetLastError()));
    }
    RegCloseKey(key);
}

static void probe_volume_serials() {
    wlog(L"\n== Volume Serials (GetVolumeInformation) ==");
    wchar_t drives[512] = {};
    DWORD len = GetLogicalDriveStringsW(ARRAYSIZE(drives) - 1, drives);
    if (!len) { wlog(L"  GetLogicalDriveStrings failed: Win32=" + std::to_wstring(GetLastError())); return; }
    for (wchar_t* p = drives; *p; p += wcslen(p) + 1) {
        DWORD serial = 0, maxComp = 0, flags = 0;
        wchar_t fsName[64] = {}, volName[128] = {};
        if (GetVolumeInformationW(p, volName, ARRAYSIZE(volName), &serial, &maxComp, &flags, fsName, ARRAYSIZE(fsName))) {
            std::wostringstream oss;
            oss << L"  " << p << L" serial=0x" << std::hex << std::uppercase << std::setw(8) << std::setfill(L'0') << serial
                << std::dec << L" fs=" << fsName << L" label=" << volName;
            wlog(oss.str());
        } else {
            wlog(L"  " + std::wstring(p) + L" (GetVolumeInformation failed, Win32=" + std::to_wstring(GetLastError()) + L")");
        }
    }
}

static std::wstring get_firmware_table_string(DWORD provider, DWORD signature) {
    UINT size = GetSystemFirmwareTable(provider, signature, nullptr, 0);
    if (size == 0) return L"";
    std::vector<uint8_t> buf(size);
    if (GetSystemFirmwareTable(provider, signature, buf.data(), size) != size) return L"";
    return L"present (" + std::to_wstring(size) + L" bytes)";
}

// Best-effort SMBIOS string extraction (structure walk). Does not fabricate; reports raw-derived fields if parseable.
static void probe_smbios() {
    wlog(L"\n== SMBIOS/Firmware (GetSystemFirmwareTable 'RSMB') ==");
    const DWORD RSMB = 'BMSR';
    UINT size = GetSystemFirmwareTable(RSMB, 0, nullptr, 0);
    if (size == 0) {
        wlog(L"  RSMB table not available. Win32=" + std::to_wstring(GetLastError()));
        return;
    }
    std::vector<uint8_t> buf(size);
    if (GetSystemFirmwareTable(RSMB, 0, buf.data(), size) != size) {
        wlog(L"  GetSystemFirmwareTable(RSMB) read failed. Win32=" + std::to_wstring(GetLastError()));
        return;
    }
    if (size < 8) { wlog(L"  RSMB buffer too small."); return; }
    uint8_t major = buf[1], minor = buf[2];
    wlog(L"  SMBIOS version (raw header): " + std::to_wstring(major) + L"." + std::to_wstring(minor) + L", size=" + std::to_wstring(size));

    // Walk SMBIOS structures: header at offset 8 in RawSMBIOSData
    size_t i = 8;
    auto get_str = [&](const uint8_t* structStart, size_t structLen, uint8_t idx) -> std::wstring {
        if (idx == 0) return L"";
        const char* p = reinterpret_cast<const char*>(structStart + structLen);
        uint8_t cur = 1;
        while (*p) {
            if (cur == idx) {
                std::string s(p);
                return std::wstring(s.begin(), s.end());
            }
            p += strlen(p) + 1;
            ++cur;
        }
        return L"";
    };

    int structures = 0;
    while (i + 4 <= buf.size() && structures < 512) {
        uint8_t type = buf[i];
        uint8_t len = buf[i + 1];
        if (len < 4 || i + len > buf.size()) break;
        const uint8_t* s = &buf[i];

        if (type == 1 && len >= 0x19) { // System Information
            wlog(L"  [Type 1 System Information]");
            wlog(L"    Manufacturer: " + get_str(s, len, s[0x04]));
            wlog(L"    Product Name: " + get_str(s, len, s[0x05]));
            wlog(L"    Version: " + get_str(s, len, s[0x06]));
            wlog(L"    Serial Number: " + get_str(s, len, s[0x07]));
            // UUID at 0x08 (16 bytes)
            if (len >= 0x18) {
                wlog(L"    UUID: " + to_hex(s + 0x08, 16, true));
            }
            wlog(L"    SKU Number: " + (len >= 0x19 ? get_str(s, len, s[0x19 - 0? 0x19:0]) : L"")); // simplified; SKU index at 0x19 in 2.4+
            if (len >= 0x1A) wlog(L"    Family: " + get_str(s, len, s[0x1A - 0])); // may vary by spec
        }
        if (type == 2 && len >= 0x11) { // Baseboard
            wlog(L"  [Type 2 Baseboard Information]");
            wlog(L"    Manufacturer: " + get_str(s, len, s[0x04]));
            wlog(L"    Product: " + get_str(s, len, s[0x05]));
            wlog(L"    Version: " + get_str(s, len, s[0x06]));
            wlog(L"    Serial Number: " + get_str(s, len, s[0x07]));
            wlog(L"    Asset Tag: " + get_str(s, len, s[0x08]));
        }
        if (type == 4 && len >= 0x23) { // Processor
            wlog(L"  [Type 4 Processor Information]");
            wlog(L"    Socket: " + get_str(s, len, s[0x04]));
            wlog(L"    Manufacturer: " + get_str(s, len, s[0x07]));
            // Processor ID at 0x08 (8 bytes) for some versions
            if (len >= 0x10) wlog(L"    Processor ID (raw 8 bytes): " + to_hex(s + 0x08, 8, true));
            wlog(L"    Version: " + get_str(s, len, s[0x10]));
            wlog(L"    Serial Number: " + get_str(s, len, s[0x20]));
            wlog(L"    Asset Tag: " + get_str(s, len, s[0x21]));
            wlog(L"    Part Number: " + get_str(s, len, s[0x22]));
        }
        if (type == 127) break; // end-of-table

        // advance to next structure: after formatted section, string set ends with double-null
        size_t j = i + len;
        while (j + 1 < buf.size()) {
            if (buf[j] == 0 && buf[j + 1] == 0) { j += 2; break; }
            ++j;
        }
        if (j <= i) break;
        i = j;
        ++structures;
    }
    wlog(L"  Parsed structures (best-effort): " + std::to_wstring(structures));
    wlog(L"  NOTE: SMBIOS parsing is best-effort; field offsets/availability vary by spec version and firmware. Raw table is authoritative for the bytes present.");
}

static void probe_mac_addresses() {
    wlog(L"\n== Network Adapters / MAC Addresses (GetAdaptersAddresses) ==");
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
    ULONG size = 0;
    GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, nullptr, &size);
    if (size == 0) { wlog(L"  GetAdaptersAddresses sizing failed. Win32=" + std::to_wstring(GetLastError())); return; }
    std::vector<uint8_t> buf(size);
    auto* addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
    DWORD rc = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, addrs, &size);
    if (rc != NO_ERROR) { wlog(L"  GetAdaptersAddresses failed. Win32=" + std::to_wstring(rc)); return; }
    for (auto* a = addrs; a; a = a->Next) {
        wlog(L"  Adapter: " + std::wstring(a->FriendlyName ? a->FriendlyName : L"") + L" | " + std::wstring(a->Description ? a->Description : L""));
        if (a->PhysicalAddressLength > 0) {
            wlog(L"    MAC: " + to_hex(a->PhysicalAddress, a->PhysicalAddressLength, true));
        } else {
            wlog(L"    MAC: (none)");
        }
        wlog(L"    IfType=" + std::to_wstring(a->IfType) + L" OperStatus=" + std::to_wstring(a->OperStatus));
    }
}

static void probe_disk_serials() {
    wlog(L"\n== Physical Disk IDs (IOCTL_STORAGE_QUERY_PROPERTY, best-effort) ==");
    // Probe \\.\PhysicalDrive0..15 (read-only query). Requires appropriate access; may fail without admin.
    for (int i = 0; i < 16; ++i) {
        std::wstring path = L"\\\\.\\PhysicalDrive" + std::to_wstring(i);
        HANDLE h = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) {
            // common for non-existent or access denied; report only if first few to reduce noise? report all briefly
            continue;
        }
        STORAGE_PROPERTY_QUERY q{};
        q.PropertyId = StorageDeviceProperty;
        q.QueryType = PropertyStandardQuery;
        std::vector<uint8_t> out(4096);
        DWORD ret = 0;
        if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &q, sizeof(q), out.data(), (DWORD)out.size(), &ret, nullptr) && ret >= sizeof(STORAGE_DEVICE_DESCRIPTOR)) {
            auto* d = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(out.data());
            wlog(L"  " + path + L":");
            auto get_offstr = [&](DWORD off) -> std::wstring {
                if (off == 0 || off >= ret) return L"";
                const char* s = reinterpret_cast<const char*>(out.data() + off);
                size_t maxl = ret - off;
                size_t l = strnlen(s, maxl);
                std::string ss(s, l);
                return trim_ws(std::wstring(ss.begin(), ss.end()));
            };
            wlog(L"    VendorId: " + get_offstr(d->VendorIdOffset));
            wlog(L"    ProductId: " + get_offstr(d->ProductIdOffset));
            wlog(L"    ProductRevision: " + get_offstr(d->ProductRevisionOffset));
            wlog(L"    SerialNumber: " + get_offstr(d->SerialNumberOffset));
            wlog(L"    BusType=" + std::to_wstring(d->BusType) + L" Removable=" + std::to_wstring(d->RemovableMedia));
        } else {
            wlog(L"  " + path + L": IOCTL_STORAGE_QUERY_PROPERTY failed. Win32=" + std::to_wstring(GetLastError()));
        }
        CloseHandle(h);
    }
    wlog(L"  (Drives not listed were not present or not accessible; try elevated for more results.)");
}

static void probe_cpu() {
    wlog(L"\n== CPU (CPUID, user-mode) ==");
    int regs[4] = {0};
    __cpuid(regs, 0);
    char vendor[13] = {};
    memcpy(vendor, &regs[1], 4); memcpy(vendor + 4, &regs[3], 4); memcpy(vendor + 8, &regs[2], 4);
    wlog(L"  Vendor: " + std::wstring(vendor, vendor + strlen(vendor)));
    __cpuid(regs, 1);
    std::wostringstream fam;
    fam << L"  Signature/EAX=0x" << std::hex << std::uppercase << regs[0] << L" EDX=0x" << regs[3] << L" ECX=0x" << regs[2];
    wlog(fam.str());
    // Brand string
    char brand[49] = {};
    __cpuid(regs, 0x80000000);
    unsigned maxExt = regs[0];
    if (maxExt >= 0x80000004) {
        __cpuid((int*)regs, 0x80000002); memcpy(brand, regs, 16);
        __cpuid((int*)regs, 0x80000003); memcpy(brand + 16, regs, 16);
        __cpuid((int*)regs, 0x80000004); memcpy(brand + 32, regs, 16);
        std::string b(brand);
        wlog(L"  Brand: " + trim_ws(std::wstring(b.begin(), b.end())));
    }
    wlog(L"  NOTE: CPUID exposes processor identity/features; it is not a unique per-unit serial on modern CPUs in general.");
}

int wmain() {
    wlog(L"hwid_enum - read-only HWID/serial discovery (common OS/user-mode observation paths)");
    wlog(L"Environment: Windows user-mode. No spoofing. No anti-cheat behavior assumed.");
    wlog(L"IMPORTANT: 'All serials' depends on observer; this covers common OS-exposed sources, not necessarily every path a specific anti-cheat/driver/firmware/app may use.");

    probe_computer_name();
    probe_machine_guid();
    probe_volume_serials();
    probe_mac_addresses();
    probe_disk_serials();
    probe_smbios();
    probe_cpu();

    wlog(L"\n== Done ==");
    wlog(L"Save as BEFORE/AFTER snapshots and diff. For any target-specific blocking/virtualization, identify the exact consumer and observation path first; validate per-path; prefer reversible changes; do not claim undetectability.");
    return 0;
}

int main() { return wmain(); }
