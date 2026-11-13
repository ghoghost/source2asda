#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <windows.h>

namespace fnx {

struct ModuleInfo {
    std::wstring name;
    std::wstring path;
    uintptr_t base = 0;
    size_t size = 0;
};

struct ProcessInfo {
    DWORD pid = 0;
    std::wstring name;
    std::wstring arch = L"x64-assumed"; // external tool targets x64 Windows processes
};

class Process {
public:
    Process() = default;
    ~Process();

    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;
    Process(Process&& other) noexcept;
    Process& operator=(Process&& other) noexcept;

    bool attach(DWORD pid, std::wstring& error);
    bool attach_by_name(const std::wstring& name, std::wstring& error);
    void detach();

    bool is_attached() const { return handle_ != nullptr; }
    HANDLE handle() const { return handle_; }
    DWORD pid() const { return pid_; }

    std::vector<ModuleInfo> modules(std::wstring& error) const;
    std::optional<ModuleInfo> find_module(const std::wstring& name, std::wstring& error) const;

    static std::optional<DWORD> find_pid_by_name(const std::wstring& name, std::wstring& error);

private:
    HANDLE handle_ = nullptr;
    DWORD pid_ = 0;
};

} // namespace fnx
