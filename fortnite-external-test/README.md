# Fortnite External Test Framework + Diagnostic Driver (Offline/Test)

External Windows x64 framework for **local testing and diagnostics** with an optional KMDF driver backend. It does **not** contain fabricated Fortnite offsets, signatures, pointer chains, structure layouts, version-specific addresses, or anti-cheat evasion techniques.

## Evidence / claims boundary
- **No Fortnite offsets/signatures included.** All game targets come from runtime module bases + user-supplied patterns in config for the exact build under test. Missing/invalid patterns are reported, never guessed.
- **No "undetected" claim.** Detectability (driver load, device objects, IOCTL patterns, memory access behavior, signing/enforcement, anti-cheat environment) is environment- and target-specific and not established here. The driver is a standard, observable KMDF diagnostic component using documented APIs. Do not treat it as a bypass.
- **Verification state:** Code is provided complete in this workspace but not compiled/executed/loaded/tested by this response. Use the build and verification steps below in your environment.

## Components
- `src/*` - user-mode external tool: process attach, module enum, checked reads, IDA-style pattern scan, config-driven resolution, diagnostics, offline self-test, optional driver client.
- `driver/*` - KMDF diagnostic driver (`\\.\FnxTestDrv`): versioned IOCTLs, validated read-memory requests via `MmCopyVirtualMemory` (documented), shared structs in `driver/shared.h`.
- `config/fortnite.example.ini` - template with placeholders only.

## What works (by design)
- **Offline:** `--self-test` validates pattern parsing/scanning, pointer-chain reads, and self module enumeration using the tool's own process memory (user-mode APIs). No game/network required.
- **User-mode diagnostics:** `--diagnose` attaches read-only, enumerates modules, and scans/optionally resolves user-supplied patterns via `ReadProcessMemory`.
- **Driver-backed diagnostics (when loaded):** `--diagnose --driver \\.\FnxTestDrv` verifies driver connectivity/version and performs memory reads through the driver's IOCTL interface (module enumeration remains user-mode). Useful for controlled test environments where a kernel read path is required.

## Build: user-mode tool (MSVC, x64 Developer Command Prompt)
```bat
cd fortnite-external-test
cl /nologo /std:c++17 /EHsc /W4 /DUNICODE /D_UNICODE ^
  src\main.cpp src\process_win.cpp src\memory_win.cpp src\driver_client.cpp src\pattern.cpp src\config.cpp src\diag.cpp ^
  /Fe:fn_external_test.exe /link Psapi.lib Shell32.lib
```

## Build: user-mode tool (CMake)
```bat
cd fortnite-external-test
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## Build/install: driver (WDK)
See `driver/README.md` for complete WDK/VS KMDF build steps, INF, test-signing notes, install/load/unload commands, and diagnostics. Requires Windows SDK + WDK matching your target.

## Run / verify
```bat
:: 1) Offline self-test (no driver, no game) - expected: SELF_TEST: PASS, exit 0
fn_external_test.exe --self-test

:: 2) User-mode diagnostics against a process
fn_external_test.exe --diagnose --process Notepad.exe

:: 3) Driver connectivity + diagnostics (driver must be installed/loaded)
fn_external_test.exe --diagnose --process Notepad.exe --driver \\.\FnxTestDrv

:: 4) Config-driven scan with user-supplied patterns (exact build evidence required)
fn_external_test.exe --diagnose --process FortniteClient-Win64-Shipping.exe --config config\fortnite.example.ini
fn_external_test.exe --diagnose --process FortniteClient-Win64-Shipping.exe --config config\fortnite.example.ini --driver \\.\FnxTestDrv
```

## Config (parameterized, no fabrication)
`[pattern.<Name>]`: `module`, `pattern` (hex bytes, `??` wildcards), optional `rip_relative`, `disp_offset`, `add_offset`. Placeholders like `SUPPLY_FOR_YOUR_BUILD` are rejected as `MISSING_USER_PATTERN`.

## Failure diagnostics
- **Attach/module:** `OpenProcess failed` (PID/privileges/integrity), `EnumProcessModulesEx failed`, `MODULE_NOT_FOUND`.
- **Reads:** `ReadProcessMemory failed` or driver payload `status=0x...` / `bytesRead` mismatch (unreadable region, wrong base/address, stale pointer, invalid PID, protections/environment).
- **Patterns:** `MISSING_USER_PATTERN`, `INVALID_PATTERN`, `MISS` (wrong for build), multiple matches (non-unique; refine).
- **Driver client:** `CreateFileA failed` (device missing/not loaded/wrong path/access), `IOCTL_FNX_GET_VERSION failed` (IOCTL mismatch - rebuild from same `shared.h`), load/start issues (signing/testsigning/Secure Boot/arch/INF/service) - see `driver/README.md`.
- **RIP resolve:** `RIP resolve failed reading disp32...` check `disp_offset` and match correctness.

## Security/safety notes
- User-mode tool and driver reads are read-only in this implementation. Kernel drivers run at high privilege: load only code you built/trust, in test environments, and follow least-privilege/restore practices. Driver loading is irreversible in effect until stopped/removed; test signing changes require reboot/confirmation in your environment.
- For game targets, ensure use complies with applicable terms/laws and your authorization; this project is provided as an engineering/test artifact.
