# Mouse/HID Identity Probe (Discovery & Verification)

Read-only Windows tool to enumerate mouse/HID identifiers visible through common **user-mode** observation paths. Use it to establish *what* is exposed and *where* before implementing any (authorized, reversible) identity virtualization.

## What it reports
- **SetupDi / Configuration Manager (mouse class):** device instance ID, `SPDRP_HARDWAREID`, friendly name, device description, manufacturer, driver, enumerator, location, PDO name (when available).
- **HID interface devices (filtered to mouse usage Generic Desktop/Mouse):** device path, instance/hardware IDs, VID/PID/version from HID attributes, descriptors/manufacturer/driver.
- **Raw Input:** `RIDI_DEVICENAME`, `RIDI_DEVICEINFO` (id, buttons, sample rate, horizontal wheel) for `RIM_TYPEMOUSE` devices.

## Build (MSVC x64 Developer Command Prompt)
```bat
cd fortnite-external-test\identity-tool
cl /nologo /std:c++17 /EHsc /W4 /DUNICODE /D_UNICODE dev_enum.cpp /Fe:dev_enum.exe /link setupapi.lib hid.lib user32.lib cfgmgr32.lib
```

## Run
```bat
dev_enum.exe > before.txt
:: perform authorized, reversible change in a controlled environment
dev_enum.exe > after.txt
fc before.txt after.txt
```

## Identity virtualization engineering notes (evidence-first)
1. **Identify the observer:** which process/component reads the ID, and via which API/IOCTL/registry/driver/firmware path? This probe covers common user-mode paths only; kernel, driver stack, firmware tables, or app-specific telemetry may differ.
2. **Locate the source:** device/driver/registry/firmware/generated-at-runtime. One value often has multiple independent sources.
3. **Choose narrowest reversible layer:** prefer user-mode/API-level virtualization in the consuming process (for testing) when sufficient; use driver-backed interception only when the boundary requires it and you can validate behavior safely.
4. **Implement + validate:** change only the targeted observation, preserve unrelated behavior, verify via the same observation path(s) before/after, and provide restore.
5. **No "undetected" claim:** detection/evasion depends on the specific target, anti-cheat, OS, signing/enforcement, and instrumentation - none of which are established by this tool. Do not fabricate.

## Safety
- Read-only. No system changes. For any spoofing/virtualization work, use authorized test environments, make changes reversible, and confirm before irreversible operations.
