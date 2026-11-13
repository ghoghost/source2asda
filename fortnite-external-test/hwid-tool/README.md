# HWID/Serial Probe (Discovery & Verification)

Read-only Windows tool to enumerate common OS-exposed hardware/system identifiers and serials. For authorized testing and evidence gathering (BEFORE/AFTER snapshots).

## Observation paths covered (user-mode / OS APIs)
- Computer/host name (`GetComputerName*`)
- Machine GUID (`HKLM\SOFTWARE\Microsoft\Cryptography\MachineGuid`)
- Volume serials (`GetVolumeInformation` per logical drive)
- Network adapter MAC addresses (`GetAdaptersAddresses`)
- Physical disk identifiers (`IOCTL_STORAGE_QUERY_PROPERTY` -> `STORAGE_DEVICE_DESCRIPTOR`: vendor/product/revision/serial where available; may require elevation)
- SMBIOS/firmware tables (`GetSystemFirmwareTable` RSMB): best-effort parse of Type 1/2/4 fields (manufacturer/product/serial/UUID/asset/processor IDs as present) + raw size/version; field availability varies by firmware/spec
- CPU identity (`CPUID` vendor/brand/signature; not a unique per-unit serial in general)

## Not guaranteed / may differ per observer
"All HWID/serials" is observer-dependent. Specific anti-cheat or software may read via other paths (kernel drivers, direct IOCTLs, ACPI/other firmware tables, NIC/disk controller-specific queries, WMI classes, registry keys not listed, GPU/monitor IDs, TPM, browser/app telemetry, etc.). This tool inventories common sources; target-specific blocking/virtualization requires tracing the actual consumer's observation path(s).

## Build & run
```bat
cd fortnite-external-test\hwid-tool
cl /nologo /std:c++17 /EHsc /W4 /DUNICODE /D_UNICODE hwid_enum.cpp /Fe:hwid_enum.exe /link iphlpapi.lib advapi32.lib

hwid_enum.exe > hwid_before.txt
:: after authorized reversible change
hwid_enum.exe > hwid_after.txt
fc hwid_before.txt hwid_after.txt
```

## Notes
- Read-only. No system modifications.
- To "block/virtualize" a specific identifier for a specific consumer: identify the exact observation path, implement the narrowest reversible interception at that boundary, validate per-path, and provide restore. Do not claim undetectability (environment/target dependent).
