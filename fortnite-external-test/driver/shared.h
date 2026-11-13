#pragma once

// Shared definitions for fnx test driver (kernel) and user-mode client.
// All values are explicit and parameterized by the caller; no undocumented structures or offsets.

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <windows.h>
#include <winioctl.h>
#include <stdint.h>
#endif

#define FNX_DEVICE_NAME      L"\\Device\\FnxTestDrv"
#define FNX_DOS_DEVICE_NAME  L"\\DosDevices\\FnxTestDrv"
#define FNX_USER_DEVICE_PATH "\\\\.\\FnxTestDrv"

#define FNX_IOCTL_TYPE 0x8000

// METHOD_BUFFERED: input/output copied by I/O manager, suitable for simple fixed structures.
#define IOCTL_FNX_GET_VERSION CTL_CODE(FNX_IOCTL_TYPE, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_FNX_READ_MEMORY CTL_CODE(FNX_IOCTL_TYPE, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define FNX_DRIVER_VERSION_MAJOR 1
#define FNX_DRIVER_VERSION_MINOR 0
#define FNX_MAX_READ_SIZE 4096

#pragma pack(push, 1)

typedef struct _FNX_VERSION_RESPONSE {
    uint32_t major;
    uint32_t minor;
    uint32_t status; // 0 = ok
} FNX_VERSION_RESPONSE, *PFNX_VERSION_RESPONSE;

typedef struct _FNX_READ_MEMORY_REQUEST {
    uint32_t targetPid;
    uint64_t targetAddress;
    uint32_t size;
    uint32_t reserved;
} FNX_READ_MEMORY_REQUEST, *PFNX_READ_MEMORY_REQUEST;

typedef struct _FNX_READ_MEMORY_RESPONSE {
    uint32_t status;    // 0 = ok, otherwise Win32/NT-derived diagnostic code
    uint32_t bytesRead;
    uint8_t  data[FNX_MAX_READ_SIZE];
} FNX_READ_MEMORY_RESPONSE, *PFNX_READ_MEMORY_RESPONSE;

#pragma pack(pop)
