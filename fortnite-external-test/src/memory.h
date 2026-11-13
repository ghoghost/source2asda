#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <windows.h>

namespace fnx {

class Memory {
public:
    explicit Memory(HANDLE process = nullptr) : process_(process) {}
    void set_process(HANDLE process) { process_ = process; }

    bool read(uintptr_t address, void* buffer, size_t size, std::wstring& error) const;
    std::optional<std::vector<uint8_t>> read_bytes(uintptr_t address, size_t size, std::wstring& error) const;

    template <typename T>
    std::optional<T> read_value(uintptr_t address, std::wstring& error) const {
        T value{};
        if (!read(address, &value, sizeof(T), error)) return std::nullopt;
        return value;
    }

private:
    HANDLE process_ = nullptr;
};

} // namespace fnx
