# FnxTest UI (Custom ImGui Control Panel)

Win32 + DirectX 11 + Dear ImGui UI for the Fortnite external test / diagnostic / identity-virtualization test workflow. **UI for configuration, discovery results, driver status, and diagnostics** — it does not claim or implement anti-cheat evasion or "undetected" blocking. "Block EAC from reading all HWID serials" is not a claim this project can make; target-specific blocking requires identifying the consumer's exact observation path(s) and implementing validated, reversible interception per path.

## Features (custom styled)
- Dark custom theme (accented, rounded, readable tables/log).
- **Overview:** environment notes, evidence/claim boundaries, quick actions.
- **Diagnostics:** process/PID/config/driver-device inputs, command preview for `fn_external_test.exe`, status indicators.
- **HWID & Serials (inventory view):** categorized fields for common sources (computer name, machine GUID, volume serials, MACs, disk IDs, SMBIOS fields, CPU) with editable "profile" values and notes; intended to be populated from `hwid_enum.exe` / `dev_enum.exe` output (paste/import workflow) for BEFORE/AFTER comparison. No automatic system modification.
- **Mouse/Device IDs:** fields for SetupDi/Raw Input/HID identifiers (from probe) and per-path virtualization plan notes.
- **Driver:** device path, version/connect test instructions, IOCTL info, explicit non-claims.
- **Profiles/Config:** pattern config editor (INI-style entries), export snippet, validation hints (placeholders flagged).
- **Log:** timestamped UI log.

## Build (CMake, requires git + internet for first configure to fetch ImGui v1.91.0)
```bat
cd fortnite-external-test\ui
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```
Binary: `build\Release\fnx_test_ui.exe`

If offline, manually place Dear ImGui sources under a folder and adjust `CMakeLists.txt` `IMGUI_DIR` (and remove `FetchContent`) to point to it (needs `imgui*.cpp`, `backends/imgui_impl_win32.*`, `backends/imgui_impl_dx11.*`).

## Run
```bat
fnx_test_ui.exe
```
Use alongside:
- `..\fn_external_test.exe --self-test` (offline engine verification)
- `..\hwid-tool\hwid_enum.exe` and `..\identity-tool\dev_enum.exe` (evidence snapshots)

## Important
- Read-only UI/config management. No hooks, no injection, no spoofing, no driver loading performed by the UI.
- Any actual virtualization/spoofing or "blocking" must be implemented per evidenced observation path, be reversible, validated before/after, and compliant with authorization/environment policy. "Undetected" is not guaranteed.
