// fnxdrv.c - KMDF diagnostic driver
// Read-only memory reads for a user-mode test client.

#define _KERNEL_MODE
#include <ntifs.h>
#include <wdf.h>
#include "shared.h"

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD FnxDeviceAdd;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL FnxIoControl;

static NTSTATUS FnxReadMemory(
    ULONG pid,
    ULONG64 address,
    ULONG size,
    PVOID buffer,
    PULONG bytesRead)
{
    PEPROCESS process = NULL;
    SIZE_T copied = 0;
    NTSTATUS status;

    if (!buffer || !bytesRead || size == 0 || size > FNX_MAX_READ_SIZE || address == 0)
        return STATUS_INVALID_PARAMETER;

    *bytesRead = 0;

    status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)pid, &process);
    if (!NT_SUCCESS(status))
        return status;

    status = MmCopyVirtualMemory(
        process,
        (PVOID)(ULONG_PTR)address,
        PsGetCurrentProcess(),
        buffer,
        (SIZE_T)size,
        KernelMode,
        &copied);

    ObDereferenceObject(process);

    if (!NT_SUCCESS(status))
        return status;

    *bytesRead = (ULONG)copied;

    if (copied != (SIZE_T)size)
        return STATUS_PARTIAL_COPY;

    return STATUS_SUCCESS;
}

VOID FnxIoControl(
    WDFQUEUE queue,
    WDFREQUEST request,
    size_t outputLength,
    size_t inputLength,
    ULONG ioControlCode)
{
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    ULONG_PTR info = 0;

    UNREFERENCED_PARAMETER(queue);

    switch (ioControlCode)
    {
    case IOCTL_FNX_GET_VERSION:
    {
        PFNX_VERSION_RESPONSE out = NULL;

        if (outputLength < sizeof(*out))
        {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        status = WdfRequestRetrieveOutputBuffer(request, sizeof(*out), (PVOID*)&out, NULL);
        if (!NT_SUCCESS(status))
            break;

        out->major = FNX_DRIVER_VERSION_MAJOR;
        out->minor = FNX_DRIVER_VERSION_MINOR;
        out->status = 0;
        info = sizeof(*out);
        break;
    }

    case IOCTL_FNX_READ_MEMORY:
    {
        PFNX_READ_MEMORY_REQUEST in = NULL;
        PFNX_READ_MEMORY_RESPONSE out = NULL;
        ULONG bytesRead = 0;

        if (inputLength < sizeof(*in) || outputLength < sizeof(*out))
        {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        status = WdfRequestRetrieveInputBuffer(request, sizeof(*in), (PVOID*)&in, NULL);
        if (!NT_SUCCESS(status))
            break;

        status = WdfRequestRetrieveOutputBuffer(request, sizeof(*out), (PVOID*)&out, NULL);
        if (!NT_SUCCESS(status))
            break;

        RtlZeroMemory(out, sizeof(*out));
        out->status = FnxReadMemory(in->targetPid, in->targetAddress, in->size, out->data, &bytesRead);
        out->bytesRead = bytesRead;
        info = sizeof(*out);
        status = STATUS_SUCCESS;
        break;
    }
    }

    WdfRequestCompleteWithInformation(request, status, info);
}

NTSTATUS FnxDeviceAdd(
    WDFDRIVER driver,
    PWDFDEVICE_INIT deviceInit)
{
    WDFDEVICE device;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_IO_QUEUE_CONFIG queueConfig;
    NTSTATUS status;

    UNREFERENCED_PARAMETER(driver);

    DECLARE_CONST_UNICODE_STRING(deviceName, FNX_DEVICE_NAME);
    DECLARE_CONST_UNICODE_STRING(symbolicName, FNX_DOS_DEVICE_NAME);

    status = WdfDeviceInitAssignName(deviceInit, &deviceName);
    if (!NT_SUCCESS(status))
        return status;

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    status = WdfDeviceCreate(&deviceInit, &attributes, &device);
    if (!NT_SUCCESS(status))
        return status;

    status = WdfDeviceCreateSymbolicLink(device, &symbolicName);
    if (!NT_SUCCESS(status))
        return status;

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);
    queueConfig.EvtIoDeviceControl = FnxIoControl;

    return WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);
}

NTSTATUS DriverEntry(
    PDRIVER_OBJECT driverObject,
    PUNICODE_STRING registryPath)
{
    WDF_DRIVER_CONFIG config;
    WDF_OBJECT_ATTRIBUTES attributes;

    WDF_DRIVER_CONFIG_INIT(&config, FnxDeviceAdd);
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

    return WdfDriverCreate(driverObject, registryPath, &attributes, &config, WDF_NO_HANDLE);
}
