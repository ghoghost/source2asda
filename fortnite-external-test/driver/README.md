# FnxTest Diagnostic KMDF Driver

Read-only diagnostic/test driver providing a versioned IOCTL interface for process memory reads (`MmCopyVirtualMemory`, documented API). Intended for local development/testing in controlled environments.

## Important (evidence-based)
- **No anti-cheat evasion/undetectability is claimed or implemented.** "Undetected" depends on target software, OS version, signing/enforcement policy, driver load method, memory access patterns, and other environment-specific factors not established here. This driver is a standard, observable KMDF software device (`\\.\FnxTestDrv`) with explicit IOCTLs.
- Uses documented interfaces (`PsLookupProcessByProcessId`, `MmCopyVirtualMemory`, KMDF). No undocumented kernel offsets/structures are fabricated.
- Input validation: buffer sizes checked, max read size limited (`FNX_MAX_READ_SIZE`), null address rejected, PID resolved per request, NTSTATUS returned diagnostically.

## Files
- `shared.h` - IOCTL codes and request/response structures (shared with user-mode client).
- `driver.c` - KMDF driver source.
- `fnxdrv.inf` - install template (root-enumerated software device, demand start).

## Build (WDK + Visual Studio)
Prereqs: Windows 10/11 x64 dev machine, Visual Studio 2022, Windows SDK, Windows Driver Kit (WDK) matching SDK.

1. In Visual Studio: **Create a new project** -> search **Kernel Mode Driver, Empty (KMDF)**.
2. Set configuration to **Release | x64** (and/or Debug as needed).
3. Add `driver.c` and `shared.h` to the project (or include from this folder).
4. Project properties (KMDF driver):
   - **Configuration Properties > Driver Settings > Target OS Version / Target Platform**: match your test machine.
   - **C/C++ > General**: ensure WDK include paths are used by the KMDF template (default).
   - **Driver Settings > General**: KMDF version e.g. 1.15+ (match `KmdfLibraryVersion` in INF).
   - If the template does not already define it, no special preprocessor defs are required beyond `_KERNEL_MODE` in code path (handled in `shared.h`).
5. Build -> produces `fnxdrv.sys` (rename output to `fnxdrv.sys` if project name differs, or adjust INF `ServiceBinary`/`SourceDisksFiles`).
6. For test-signed deployment, enable test signing on the **target** machine and sign the driver/package per WDK docs:
   - Target (admin, reboot required): `bcdedit /set testsigning on`
   - Create/use a test cert and sign `.sys`/catalog as appropriate for your WDK workflow, or use Visual Studio deployment/test signing options.

## Install / load (test machine, admin)
**Option A: devcon / pnputil (recommended for INF-based install)**
```bat
:: Copy fnxdrv.sys, fnxdrv.inf (and .cat if signed/cataloged) to a folder on target
pnputil /add-driver fnxdrv.inf /install
devcon install fnxdrv.inf Root\FnxTestDrv
```
(`devcon` from WDK tools; `pnputil` built into Windows.)

**Option B: service-based manual load (development, if you have a signed/test-signed .sys)**
```bat
sc create FnxTestDrv type= kernel binPath= "C:\path\to\fnxdrv.sys"
sc start FnxTestDrv
```
Note: device/symbolic link creation depends on driver initialization; the KMDF driver creates `\\.\FnxTestDrv` in `EvtDeviceAdd`.

**Stop/remove**
```bat
sc stop FnxTestDrv
sc delete FnxTestDrv
:: or
pnputil /delete-driver fnxdrv.inf /uninstall /force
```
Disable test signing when done (target, admin, reboot): `bcdedit /set testsigning off`

## Verification (user-mode client)
With driver loaded and device present:
```bat
fn_external_test.exe --diagnose --process Notepad.exe --driver \\.\FnxTestDrv
```
Expected diagnostics include: `Driver backend: CONNECTED (version 1.0, device \\.\FnxTestDrv)` and module/read results using the driver backend. If connection fails, check device path, service state, signing/test-signing policy, and privileges.

## Failure diagnostics
- `CreateFileA failed... Win32 2`: device not present (driver not loaded / device not created).
- `CreateFileA failed... Win32 5`: access denied.
- `IOCTL_FNX_GET_VERSION failed`: driver not handling request, wrong device, or IOCTL mismatch (rebuild client+driver from same `shared.h`).
- Driver read `status` non-zero in response: NTSTATUS from lookup/copy (e.g. invalid PID/address, partial copy, inaccessible range). Use user-mode diagnostics to confirm PID/module/base/region validity.
- Load/start failures: signature enforcement, Secure Boot policy, test signing not enabled, wrong architecture (must be x64), or INF/service misconfiguration. Check Event Viewer (System) and `!analyze -v` / WinDbg if debugging.
