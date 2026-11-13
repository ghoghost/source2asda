#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <windows.h>

namespace fnx {

class DriverClient {
public:
    DriverClient() = default;
    ~DriverClient();

    DriverClient(const DriverClient&) = delete;
    DriverClient& operator=(const DriverClient&) = delete;

    bool open(const std::string& device_path, std::wstring& error);
    void close();
    bool is_open() const { return handle_ != INVALID_HANDLE_VALUE; }

    bool get_version(uint32_t& major, uint32_t& minor, std::wstring& error) const;
    bool read(uint32_t target_pid, uintptr_t address, void* buffer, size_t size, std::wstring& error) const;
    std::optional<std::vector<uint8_t>> read_bytes(uint32_t target_pid, uintptr_t address, size_t size, std::wstring& error) const;

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
};

} // namespace fnx
