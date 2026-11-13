#include "driver_client.h"

#include "../driver/shared.h"

#include <algorithm>
#include <cstring>

namespace fnx {

static std::wstring win_error(DWORD code) {
    wchar_t* buf = nullptr;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD len = FormatMessageW(flags, nullptr, code, 0, reinterpret_cast<LPWSTR>(&buf), 0, nullptr);
    std::wstring msg = (len && buf) ? std::wstring(buf, len) : L"unknown error";
    if (buf) LocalFree(buf);
    while (!msg.empty() && (msg.back() == L'\r' || msg.back() == L'\n' || msg.back() == L' ')) msg.pop_back();
    return L"Win32 " + std::to_wstring(code) + L": " + msg;
}

DriverClient::~DriverClient() { close(); }

bool DriverClient::open(const std::string& device_path, std::wstring& error) {
    close();
    HANDLE h = CreateFileA(device_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        error = L"CreateFileA failed for driver device: " + win_error(GetLastError());
        return false;
    }
    handle_ = h;
    return true;
}

void DriverClient::close() {
    if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
}

bool DriverClient::get_version(uint32_t& major, uint32_t& minor, std::wstring& error) const {
    if (!is_open()) {
        error = L"Driver not open";
        return false;
    }
    FNX_VERSION_RESPONSE out{};
    DWORD returned = 0;
    BOOL ok = DeviceIoControl(handle_, IOCTL_FNX_GET_VERSION, nullptr, 0, &out, sizeof(out), &returned, nullptr);
    if (!ok || returned != sizeof(out)) {
        error = L"IOCTL_FNX_GET_VERSION failed: " + win_error(GetLastError());
        return false;
    }
    if (out.status != 0) {
        error = L"Driver version query returned status 0x" + std::to_wstring(out.status);
        return false;
    }
    major = out.major;
    minor = out.minor;
    return true;
}

bool DriverClient::read(uint32_t target_pid, uintptr_t address, void* buffer, size_t size, std::wstring& error) const {
    if (!is_open()) {
        error = L"Driver not open";
        return false;
    }
    if (!buffer || size == 0) {
        error = L"Invalid read buffer/size";
        return false;
    }
    if (address == 0) {
        error = L"Null address";
        return false;
    }
    uint8_t* dst = static_cast<uint8_t*>(buffer);
    size_t remaining = size;
    uintptr_t cur = address;
    while (remaining > 0) {
        uint32_t chunk = static_cast<uint32_t>((std::min)(remaining, static_cast<size_t>(FNX_MAX_READ_SIZE)));
        FNX_READ_MEMORY_REQUEST req{};
        req.targetPid = target_pid;
        req.targetAddress = static_cast<uint64_t>(cur);
        req.size = chunk;
        req.reserved = 0;
        FNX_READ_MEMORY_RESPONSE resp{};
        DWORD returned = 0;
        BOOL ok = DeviceIoControl(handle_, IOCTL_FNX_READ_MEMORY, &req, sizeof(req), &resp, sizeof(resp), &returned, nullptr);
        if (!ok || returned != sizeof(resp)) {
            error = L"IOCTL_FNX_READ_MEMORY failed at 0x" + std::to_wstring(cur) + L": " + win_error(GetLastError());
            return false;
        }
        if (resp.status != 0 || resp.bytesRead != chunk) {
            wchar_t addrbuf[32];
            swprintf_s(addrbuf, L"0x%llX", static_cast<unsigned long long>(cur));
            error = L"Driver read failed at " + std::wstring(addrbuf) + L" status=0x" + std::to_wstring(resp.status) + L" bytesRead=" + std::to_wstring(resp.bytesRead);
            return false;
        }
        std::memcpy(dst, resp.data, chunk);
        dst += chunk;
        cur += chunk;
        remaining -= chunk;
    }
    return true;
}

std::optional<std::vector<uint8_t>> DriverClient::read_bytes(uint32_t target_pid, uintptr_t address, size_t size, std::wstring& error) const {
    std::vector<uint8_t> buf(size);
    if (!read(target_pid, address, buf.data(), size, error)) return std::nullopt;
    return buf;
}

} // namespace fnx
