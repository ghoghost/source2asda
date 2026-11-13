#include "memory.h"

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

bool Memory::read(uintptr_t address, void* buffer, size_t size, std::wstring& error) const {
    if (!process_) {
        error = L"ReadProcessMemory: no process handle";
        return false;
    }
    if (!buffer || size == 0) {
        error = L"ReadProcessMemory: invalid buffer/size";
        return false;
    }
    if (address == 0) {
        error = L"ReadProcessMemory: null address";
        return false;
    }
    SIZE_T read = 0;
    BOOL ok = ReadProcessMemory(process_, reinterpret_cast<LPCVOID>(address), buffer, static_cast<SIZE_T>(size), &read);
    if (!ok || read != static_cast<SIZE_T>(size)) {
        DWORD code = ok ? ERROR_PARTIAL_COPY : GetLastError();
        error = L"ReadProcessMemory failed at 0x" + [&]() {
            wchar_t tmp[32];
            swprintf_s(tmp, L"%llX", static_cast<unsigned long long>(address));
            return std::wstring(tmp);
        }() + L" size=" + std::to_wstring(size) + L" read=" + std::to_wstring(read) + L": " + win_error(code);
        return false;
    }
    return true;
}

std::optional<std::vector<uint8_t>> Memory::read_bytes(uintptr_t address, size_t size, std::wstring& error) const {
    std::vector<uint8_t> buf(size);
    if (!read(address, buf.data(), size, error)) return std::nullopt;
    return buf;
}

} // namespace fnx
