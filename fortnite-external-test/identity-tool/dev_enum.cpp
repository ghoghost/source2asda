// dev_enum.cpp - Mouse/HID identity discovery and verification tool (Windows x64)
// Read-only enumeration of identifiers visible through common user-mode observation paths.
// No spoofing performed. No anti-cheat behavior assumed. Use for evidence gathering.
//
// Build (MSVC x64 Developer Command Prompt):
//   cl /nologo /std:c++17 /EHsc /W4 /DUNICODE /D_UNICODE dev_enum.cpp /Fe:dev_enum.exe /link setupapi.lib hid.lib user32.lib cfgmgr32.lib

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <hidsdi.h>
#include <initguid.h>
#include <devguid.h>
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <fstream>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "cfgmgr32.lib")

static void wlog(const std::wstring& s) { std::wcout << s << L"\n"; }

static std::wstring hex_bytes(const void* data, size_t len) {
    const uint8_t* b = static_cast<const uint8_t*>(data);
    std::wostringstream oss;
    for (size_t i = 0; i < len; ++i) {
        if (i) oss << L' ';
        oss << std::hex << std::uppercase << std::setw(2) << std::setfill(L'0') << (int)b[i];
    }
    return oss.str();
}

static std::wstring reg_prop_string(HDEVINFO devInfo, SP_DEVINFO_DATA& devData, DWORD prop) {
    DWORD type = 0, size = 0;
    SetupDiGetDeviceRegistryPropertyW(devInfo, &devData, prop, &type, nullptr, 0, &size);
    if (size == 0) return L"";
    std::vector<uint8_t> buf(size + 2, 0);
    if (!SetupDiGetDeviceRegistryPropertyW(devInfo, &devData, prop, &type, buf.data(), size, &size)) {
        return L"";
    }
    if (type == REG_SZ || type == REG_EXPAND_SZ) {
        return std::wstring(reinterpret_cast<wchar_t*>(buf.data()));
    }
    if (type == REG_MULTI_SZ) {
        std::wstring out;
        const wchar_t* p = reinterpret_cast<const wchar_t*>(buf.data());
        while (*p) {
            if (!out.empty()) out += L" | ";
            out += p;
            p += wcslen(p) + 1;
        }
        return out;
    }
    return L"(non-string, type=" + std::to_wstring(type) + L")";
}

static void enum_setupdi_mice() {
    wlog(L"\n== SetupDi (GUID_DEVCLASS_MOUSE + HID mice) ==");
    // Mouse class
    HDEVINFO devInfo = SetupDiGetClassDevsW(&GUID_DEVCLASS_MOUSE, nullptr, nullptr, DIGCF_PRESENT);
    if (devInfo == INVALID_HANDLE_VALUE) {
        wlog(L"SetupDiGetClassDevs(GUID_DEVCLASS_MOUSE) failed. Win32=" + std::to_wstring(GetLastError()));
    } else {
        SP_DEVINFO_DATA devData{};
        devData.cbSize = sizeof(devData);
        for (DWORD i = 0; SetupDiEnumDeviceInfo(devInfo, i, &devData); ++i) {
            wlog(L"\n[MouseClass Device #" + std::to_wstring(i) + L"]");
            wchar_t instanceId[MAX_DEVICE_ID_LEN] = {};
            if (CM_Get_Device_IDW(devData.DevInst, instanceId, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS) {
                wlog(L"  InstanceId (CM_Get_Device_ID): " + std::wstring(instanceId));
            }
            wlog(L"  SPDRP_HARDWAREID: " + reg_prop_string(devInfo, devData, SPDRP_HARDWAREID));
            wlog(L"  SPDRP_FRIENDLYNAME: " + reg_prop_string(devInfo, devData, SPDRP_FRIENDLYNAME));
            wlog(L"  SPDRP_DEVICEDESC: " + reg_prop_string(devInfo, devData, SPDRP_DEVICEDESC));
            wlog(L"  SPDRP_MFG: " + reg_prop_string(devInfo, devData, SPDRP_MFG));
            wlog(L"  SPDRP_DRIVER: " + reg_prop_string(devInfo, devData, SPDRP_DRIVER));
            wlog(L"  SPDRP_CLASS: " + reg_prop_string(devInfo, devData, SPDRP_CLASS));
            wlog(L"  SPDRP_ENUMERATOR_NAME: " + reg_prop_string(devInfo, devData, SPDRP_ENUMERATOR_NAME));
            wlog(L"  SPDRP_LOCATION_INFORMATION: " + reg_prop_string(devInfo, devData, SPDRP_LOCATION_INFORMATION));
            // Try SPDRP_PHYSICAL_DEVICE_OBJECT_NAME (may not be available)
            wlog(L"  SPDRP_PHYSICAL_DEVICE_OBJECT_NAME: " + reg_prop_string(devInfo, devData, SPDRP_PHYSICAL_DEVICE_OBJECT_NAME));
        }
        SetupDiDestroyDeviceInfoList(devInfo);
    }

    // HID devices filtered to mouse usage page/usage where possible
    wlog(L"\n== SetupDi (HID interface devices, mouse-usage filtered when readable) ==");
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);
    HDEVINFO hidInfo = SetupDiGetClassDevsW(&hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hidInfo == INVALID_HANDLE_VALUE) {
        wlog(L"SetupDiGetClassDevs(HID) failed. Win32=" + std::to_wstring(GetLastError()));
        return;
    }
    SP_DEVICE_INTERFACE_DATA ifData{};
    ifData.cbSize = sizeof(ifData);
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(hidInfo, nullptr, &hidGuid, i, &ifData); ++i) {
        DWORD detailSize = 0;
        SetupDiGetDeviceInterfaceDetailW(hidInfo, &ifData, nullptr, 0, &detailSize, nullptr);
        if (detailSize == 0) continue;
        std::vector<uint8_t> buf(detailSize);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buf.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        SP_DEVINFO_DATA devData{};
        devData.cbSize = sizeof(devData);
        if (!SetupDiGetDeviceInterfaceDetailW(hidInfo, &ifData, detail, detailSize, nullptr, &devData)) continue;

        std::wstring path = detail->DevicePath;
        HANDLE h = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        bool isMouse = false;
        USHORT vid = 0, pid = 0, ver = 0;
        if (h != INVALID_HANDLE_VALUE) {
            HIDD_ATTRIBUTES attr{};
            attr.Size = sizeof(attr);
            if (HidD_GetAttributes(h, &attr)) {
                vid = attr.VendorID;
                pid = attr.ProductID;
                ver = attr.VersionNumber;
            }
            PHIDP_PREPARSED_DATA pp = nullptr;
            if (HidD_GetPreparsedData(h, &pp)) {
                HIDP_CAPS caps{};
                if (HidP_GetCaps(pp, &caps) == HIDP_STATUS_SUCCESS) {
                    // Generic Desktop (0x01), Mouse (0x02)
                    if (caps.UsagePage == 0x01 && caps.Usage == 0x02) isMouse = true;
                }
                HidD_FreePreparsedData(pp);
            }
            CloseHandle(h);
        }
        if (!isMouse) continue; // focus on mice for this probe

        wlog(L"\n[HID Mouse Interface #" + std::to_wstring(i) + L"]");
        wlog(L"  DevicePath: " + path);
        wlog(L"  InstanceId: " + reg_prop_string(hidInfo, devData, SPDRP_HARDWAREID));
        wchar_t instanceId[MAX_DEVICE_ID_LEN] = {};
        if (CM_Get_Device_IDW(devData.DevInst, instanceId, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS) {
            wlog(L"  CM_Device_ID: " + std::wstring(instanceId));
        }
        wlog(L"  VID/PID/VER: " + std::to_wstring(vid) + L"/" + std::to_wstring(pid) + L"/" + std::to_wstring(ver));
        wlog(L"  SPDRP_DEVICEDESC: " + reg_prop_string(hidInfo, devData, SPDRP_DEVICEDESC));
        wlog(L"  SPDRP_MFG: " + reg_prop_string(hidInfo, devData, SPDRP_MFG));
        wlog(L"  SPDRP_DRIVER: " + reg_prop_string(hidInfo, devData, SPDRP_DRIVER));
    }
    SetupDiDestroyDeviceInfoList(hidInfo);
}

static void enum_raw_input_mice() {
    wlog(L"\n== Raw Input (GetRawInputDeviceList / GetRawInputDeviceInfo) ==");
    UINT n = 0;
    if (GetRawInputDeviceList(nullptr, &n, sizeof(RAWINPUTDEVICELIST)) == (UINT)-1) {
        wlog(L"GetRawInputDeviceList(count) failed. Win32=" + std::to_wstring(GetLastError()));
        return;
    }
    if (n == 0) { wlog(L"No raw input devices."); return; }
    std::vector<RAWINPUTDEVICELIST> list(n);
    if (GetRawInputDeviceList(list.data(), &n, sizeof(RAWINPUTDEVICELIST)) == (UINT)-1) {
        wlog(L"GetRawInputDeviceList(list) failed. Win32=" + std::to_wstring(GetLastError()));
        return;
    }
    int mouseIndex = 0;
    for (UINT i = 0; i < n; ++i) {
        if (list[i].dwType != RIM_TYPEMOUSE) continue;
        wlog(L"\n[RawInput Mouse #" + std::to_wstring(mouseIndex++) + L"] handle=" + std::to_wstring((uintptr_t)list[i].hDevice));
        UINT nameLen = 0;
        GetRawInputDeviceInfoW(list[i].hDevice, RIDI_DEVICENAME, nullptr, &nameLen);
        if (nameLen > 0) {
            std::wstring name(nameLen, L'\0');
            if (GetRawInputDeviceInfoW(list[i].hDevice, RIDI_DEVICENAME, name.data(), &nameLen) != (UINT)-1) {
                if (!name.empty() && name.back() == L'\0') name.pop_back();
                wlog(L"  RIDI_DEVICENAME: " + name);
            }
        }
        RID_DEVICE_INFO info{};
        info.cbSize = sizeof(info);
        UINT sz = sizeof(info);
        if (GetRawInputDeviceInfoW(list[i].hDevice, RIDI_DEVICEINFO, &info, &sz) != (UINT)-1 && info.dwType == RIM_TYPEMOUSE) {
            wlog(L"  Mouse.dwId: " + std::to_wstring(info.mouse.dwId));
            wlog(L"  Mouse.dwNumberOfButtons: " + std::to_wstring(info.mouse.dwNumberOfButtons));
            wlog(L"  Mouse.dwSampleRate: " + std::to_wstring(info.mouse.dwSampleRate));
            wlog(L"  Mouse.fHasHorizontalWheel: " + std::to_wstring(info.mouse.fHasHorizontalWheel));
        }
    }
    if (mouseIndex == 0) wlog(L"No RIM_TYPEMOUSE devices found.");
}

int wmain() {
    wlog(L"dev_enum - read-only mouse/HID identity discovery");
    wlog(L"Purpose: enumerate identifiers visible via common user-mode APIs (evidence for virtualization testing).");
    wlog(L"Observation paths: SetupDi/CM device properties, HID attributes/preparsed caps, Raw Input device info.");
    wlog(L"Note: Different consumers may observe different identifiers (other APIs, drivers, kernel, firmware, network).");

    enum_setupdi_mice();
    enum_raw_input_mice();

    wlog(L"\n== Done ==");
    wlog(L"Use this output as a BEFORE snapshot. After any (authorized, reversible) change, run again and diff.");
    wlog(L"Spoofing/virtualization must target the specific observation path used by the consumer; 'undetectability' cannot be guaranteed.");
    return 0;
}

int main() { return wmain(); }
