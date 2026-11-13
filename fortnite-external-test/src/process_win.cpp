#include "process.h"

#include <algorithm>
#include <cwctype>
#include <psapi.h>
#include <tlhelp32.h>

namespace fnx {

static std::wstring lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(std::towlower(c));
    });
    return s;
}

static std::wstring win_error(DWORD code) {
    wchar_t* buf = nullptr;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD len = FormatMessageW(flags, nullptr, code, 0, reinterpret_cast<LPWSTR>(&buf), 0, nullptr);
    std::wstring msg = (len && buf) ? std::wstring(buf, len) : L"unknown error";
    if (buf) LocalFree(buf);
    while (!msg.empty() && (msg.back() == L'\r' || msg.back() == L'\n' || msg.back() == L' ')) msg.pop_back();
    return L"Win32 " + std::to_wstring(code) + L": " + msg;
}

Process::~Process() { detach(); }

Process::Process(Process&& other) noexcept : handle_(other.handle_), pid_(other.pid_) {
    other.handle_ = nullptr;
    other.pid_ = 0;
}

Process& Process::operator=(Process&& other) noexcept {
    if (this != &other) {
        detach();
        handle_ = other.handle_;
        pid_ = other.pid_;
        other.handle_ = nullptr;
        other.pid_ = 0;
    }
    return *this;
}

bool Process::attach(DWORD pid, std::wstring& error) {
    detach();
    DWORD access = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ;
    HANDLE h = OpenProcess(access, FALSE, pid);
    if (!h) {
        error = L"OpenProcess failed for PID " + std::to_wstring(pid) + L": " + win_error(GetLastError());
        return false;
    }
    handle_ = h;
    pid_ = pid;
    return true;
}

void Process::detach() {
    if (handle_) {
        CloseHandle(handle_);
        handle_ = nullptr;
    }
    pid_ = 0;
}

std::optional<DWORD> Process::find_pid_by_name(const std::wstring& name, std::wstring& error) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        error = L"CreateToolhelp32Snapshot failed: " + win_error(GetLastError());
        return std::nullopt;
    }
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    std::wstring needle = lower(name);
    DWORD found = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (lower(pe.szExeFile) == needle) {
                found = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    } else {
        error = L"Process32FirstW failed: " + win_error(GetLastError());
        CloseHandle(snap);
        return std::nullopt;
    }
    CloseHandle(snap);
    if (!found) {
        error = L"Process not found: " + name;
        return std::nullopt;
    }
    return found;
}

bool Process::attach_by_name(const std::wstring& name, std::wstring& error) {
    auto pid = find_pid_by_name(name, error);
    if (!pid) return false;
    return attach(*pid, error);
}

std::vector<ModuleInfo> Process::modules(std::wstring& error) const {
    std::vector<ModuleInfo> out;
    if (!handle_) {
        error = L"Not attached";
        return out;
    }
    HMODULE mods[1024];
    DWORD needed = 0;
    if (!EnumProcessModulesEx(handle_, mods, sizeof(mods), &needed, LIST_MODULES_ALL)) {
        error = L"EnumProcessModulesEx failed: " + win_error(GetLastError());
        return out;
    }
    size_t count = (std::min)(static_cast<size_t>(needed / sizeof(HMODULE)), static_cast<size_t>(1024));
    for (size_t i = 0; i < count; ++i) {
        MODULEINFO mi{};
        wchar_t baseName[MAX_PATH] = {};
        wchar_t fullPath[MAX_PATH * 2] = {};
        if (!GetModuleInformation(handle_, mods[i], &mi, sizeof(mi))) continue;
        GetModuleBaseNameW(handle_, mods[i], baseName, MAX_PATH);
        GetModuleFileNameExW(handle_, mods[i], fullPath, MAX_PATH * 2);
        ModuleInfo m;
        m.name = baseName;
        m.path = fullPath;
        m.base = reinterpret_cast<uintptr_t>(mi.lpBaseOfDll);
        m.size = static_cast<size_t>(mi.SizeOfImage);
        out.push_back(std::move(m));
    }
    return out;
}

std::optional<ModuleInfo> Process::find_module(const std::wstring& name, std::wstring& error) const {
    auto mods = modules(error);
    if (!error.empty() && mods.empty()) return std::nullopt;
    std::wstring needle = lower(name);
    for (const auto& m : mods) {
        if (lower(m.name) == needle) return m;
    }
    return std::nullopt;
}

} // namespace fnx
