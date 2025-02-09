/*++
    This module implements battery miniclass functionality specific to the
    simulated battery driver.
--*/

#include <ntdef.h>
#include <wdm.h>
#include <hidpddi.h> // for PHIDP_PREPARSED_DATA
#include <hidclass.h> // for HID_COLLECTION_INFORMATION

#include "simbatt.h"
#include "simbattdriverif.h"
#include "CppAllocator.hpp"
//------------------------------------------------------------------- Prototypes

_IRQL_requires_same_
void UpdateTag (_Inout_ BATT_FDO_DATA* DevExt);

BCLASS_QUERY_TAG_CALLBACK QueryTag;
BCLASS_QUERY_INFORMATION_CALLBACK QueryInformation;
BCLASS_SET_INFORMATION_CALLBACK SetInformation;
BCLASS_QUERY_STATUS_CALLBACK QueryStatus;
BCLASS_SET_STATUS_NOTIFY_CALLBACK SetStatusNotify;
BCLASS_DISABLE_STATUS_NOTIFY_CALLBACK DisableStatusNotify;

_Must_inspect_result_
_Success_(return==STATUS_SUCCESS)
NTSTATUS SetBatteryStatus (_In_ WDFDEVICE Device, _In_ BATTERY_STATUS* BatteryStatus);

_Must_inspect_result_
_Success_(return==STATUS_SUCCESS)
NTSTATUS SetBatteryInformation (_In_ WDFDEVICE Device, _In_ BATTERY_INFORMATION* BatteryInformation);

//------------------------------------------------------------ Battery Interface

/** RAII wrapper of PHIDP_PREPARSED_DATA. */
class PHIDP_PREPARSED_DATA_Wrap {
public:
    PHIDP_PREPARSED_DATA_Wrap(size_t size) {
        m_ptr = new BYTE[size];
    }
    ~PHIDP_PREPARSED_DATA_Wrap() {
        if (m_ptr) {
            delete[] m_ptr;
            m_ptr = nullptr;
        }
    }

    operator PHIDP_PREPARSED_DATA () const {
        return (PHIDP_PREPARSED_DATA)m_ptr;
    }

private:
    BYTE* m_ptr = nullptr;
};

/** RAII wrapper of WDFIOTARGET. */
class WDFIOTARGET_Wrap {
public:
    WDFIOTARGET_Wrap() {
    }
    ~WDFIOTARGET_Wrap() {
        if (m_obj != NULL) {
            WdfObjectDelete(m_obj);
            m_obj = NULL;
        }
    }

    operator WDFIOTARGET () const {
        return m_obj;
    }
    WDFIOTARGET* operator & () {
        return &m_obj;
    }

private:
    WDFIOTARGET m_obj = NULL;
};


_Use_decl_annotations_
void InitializeBatteryState (WDFDEVICE Device)
/*++
Routine Description:
    This routine is called to initialize battery data to sane values.

    A real battery would query hardware to determine if a battery is present,
    query its static capabilities, etc.

Arguments:
    Device - Supplies the device to initialize.
--*/
{
    DebugEnter();

    BATT_FDO_DATA* DevExt = GetDeviceExtension(Device);

    // Get this battery's state - use defaults.
    {
        WdfWaitLockAcquire(DevExt->StateLock, NULL);
        UpdateTag(DevExt);

        // manufactured on 8th September 2024
        DevExt->State.ManufactureDate.Day = 8;
        DevExt->State.ManufactureDate.Month = 9;
        DevExt->State.ManufactureDate.Year = 2024;

        DevExt->State.BatteryInfo.Capabilities = BATTERY_SYSTEM_BATTERY;
        DevExt->State.BatteryInfo.Technology = 1;
        DevExt->State.BatteryInfo.Chemistry[0] = 'F';
        DevExt->State.BatteryInfo.Chemistry[1] = 'a';
        DevExt->State.BatteryInfo.Chemistry[2] = 'k';
        DevExt->State.BatteryInfo.Chemistry[3] = 'e';
        DevExt->State.BatteryInfo.DesignedCapacity = 110;
        DevExt->State.BatteryInfo.FullChargedCapacity = 100;
        DevExt->State.BatteryInfo.DefaultAlert1 = 0;
        DevExt->State.BatteryInfo.DefaultAlert2 = 0;
        DevExt->State.BatteryInfo.CriticalBias = 0;
        DevExt->State.BatteryInfo.CycleCount = 100;

        DevExt->State.BatteryStatus.PowerState = BATTERY_POWER_ON_LINE;
        DevExt->State.BatteryStatus.Capacity = 90;
        DevExt->State.BatteryStatus.Voltage = BATTERY_UNKNOWN_VOLTAGE;
        DevExt->State.BatteryStatus.Rate = 0;

        //DevExt->State.GranularityCount = 0;
        //for (unsigned int i = 0; i < DevExt->State.GranularityCount; ++i) {
        //    DevExt->State.GranularityScale[i].Granularity = 0; // granularity [mWh]
        //    DevExt->State.GranularityScale[i].Capacity = 0; // upper capacity limit for Granularity [mWh]
        //}

        DevExt->State.EstimatedTime = BATTERY_UNKNOWN_TIME; // battery run time, in seconds

        DevExt->State.Temperature = 2981; // 25 degree Celsius [10ths of a degree Kelvin]

        RtlStringCchCopyW(DevExt->State.DeviceName, MAX_BATTERY_STRING_SIZE, L"SimulatedBattery");

        RtlStringCchCopyW(DevExt->State.ManufacturerName, MAX_BATTERY_STRING_SIZE, L"OpenSource");

        RtlStringCchCopyW(DevExt->State.SerialNumber, MAX_BATTERY_STRING_SIZE, L"1234");

        RtlStringCchCopyW(DevExt->State.UniqueId, MAX_BATTERY_STRING_SIZE, L"SimulatedBattery007");

        WdfWaitLockRelease(DevExt->StateLock);
    }

    DebugPrint(DPFLTR_TRACE_LEVEL, "Batt: EvtSetBlackTimer begin\n");

#if 0
    WDFIOTARGET hidTarget = WdfDeviceGetIoTarget(Device);
#else
    WDFIOTARGET_Wrap hidTarget;
    {
        // Use PDO for HID commands.
        NTSTATUS status = WdfIoTargetCreate(Device, WDF_NO_OBJECT_ATTRIBUTES, &hidTarget);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("Batt: WdfIoTargetCreate failed 0x%x"), status);
            return;
        }

        // open in shared read-write mode
        WDF_IO_TARGET_OPEN_PARAMS openParams = {};
        WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&openParams, &DevExt->PdoName, FILE_READ_ACCESS | FILE_WRITE_ACCESS);
        openParams.ShareAccess = FILE_SHARE_WRITE | FILE_SHARE_READ;

        status = WdfIoTargetOpen(hidTarget, &openParams);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("Batt: WdfIoTargetOpen failed 0x%x"), status);
            return;
        }
    }
#endif

    HID_COLLECTION_INFORMATION collectionInfo = {};
    {
        // populate "collectionInformation"
        WDF_MEMORY_DESCRIPTOR outputDesc = {};
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDesc, &collectionInfo, sizeof(HID_COLLECTION_INFORMATION));

        NTSTATUS status = WdfIoTargetSendIoctlSynchronously(hidTarget, NULL,
            IOCTL_HID_GET_COLLECTION_INFORMATION,
            NULL, // input
            &outputDesc, // output
            NULL, NULL);
        if (!NT_SUCCESS(status)) {
            // WARNING: Fails with 0xc000000d (STATUS_INVALID_PARAMETER) if using PDO
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("Batt: IOCTL_HID_GET_COLLECTION_INFORMATION failed 0x%x"), status);
            return;
        }

        DebugPrint(DPFLTR_INFO_LEVEL, "Batt: ProductID=%x, VendorID=%x, VersionNumber=%u, DescriptorSize=%u\n", collectionInfo.ProductID, collectionInfo.VendorID, collectionInfo.VersionNumber, collectionInfo.DescriptorSize);
    }

    PHIDP_PREPARSED_DATA_Wrap preparsedData(collectionInfo.DescriptorSize);
    if (!preparsedData) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("Batt: PHIDP_PREPARSED_DATA_Wrap failed"));
        return; // STATUS_INSUFFICIENT_RESOURCES;
    }

    {
        // populate "preparsedData"
        WDF_MEMORY_DESCRIPTOR outputDesc = {};
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDesc, static_cast<PHIDP_PREPARSED_DATA>(preparsedData), collectionInfo.DescriptorSize);

        NTSTATUS status = WdfIoTargetSendIoctlSynchronously(hidTarget, NULL,
            IOCTL_HID_GET_COLLECTION_DESCRIPTOR, // same as HidD_GetPreparsedData in user-mode
            NULL, // input
            &outputDesc, // output
            NULL, NULL);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("Batt: IOCTL_HID_GET_COLLECTION_DESCRIPTOR failed 0x%x"), status);
            return;
        }
    }

    {
        // get capabilities
        HIDP_CAPS caps = {};
        NTSTATUS status = HidP_GetCaps(preparsedData, &caps);
        if (!NT_SUCCESS(status)) {
            return;
        }

        DebugPrint(DPFLTR_INFO_LEVEL, "Batt: Usage=%x, UsagePage=%x\n", caps.Usage, caps.UsagePage);
    }

    {
        // Get FEATURE report from device
        BYTE report[3] = {};
        report[0] = 0x0A; // temperature

        // WARNING: Fails with 0xc0000061 (STATUS_PRIVILEGE_NOT_HELD) if using WdfDeviceGetIoTarget(Device)
        WDF_MEMORY_DESCRIPTOR outputDesc = {};
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDesc, &report, sizeof(report));

        NTSTATUS status = WdfIoTargetSendIoctlSynchronously(hidTarget, NULL,
            IOCTL_HID_GET_FEATURE,
            NULL, // input
            &outputDesc, // output
            NULL, NULL);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("Batt: IOCTL_HID_GET_FEATURE failed 0x%x"), status);
            return;
        }

        DevExt->State.Temperature = report[1] | (report[2] << 8);
        DebugPrint(DPFLTR_INFO_LEVEL, "Batt: Temperature=%u\n", DevExt->State.Temperature);
    }
}

_Use_decl_annotations_
void UpdateTag (BATT_FDO_DATA* DevExt)
/*++
Routine Description:
    This routine is called when static battery properties have changed to
    update the battery tag.
--*/
{
    DevExt->BatteryTag += 1;
    if (DevExt->BatteryTag == BATTERY_TAG_INVALID) {
        DevExt->BatteryTag += 1;
    }
}

_Use_decl_annotations_
NTSTATUS QueryTag (void* Context, ULONG* BatteryTag)
/*++
Routine Description:
    This routine is called to get the value of the current battery tag.

Arguments:
    Context - Supplies the miniport context value for battery
    BatteryTag - Supplies a pointer to a ULONG to receive the battery tag.
--*/
{
    NTSTATUS Status;

    DebugEnter();

    BATT_FDO_DATA* DevExt = (BATT_FDO_DATA*)Context;
    WdfWaitLockAcquire(DevExt->StateLock, NULL);
    *BatteryTag = DevExt->BatteryTag;
    WdfWaitLockRelease(DevExt->StateLock);
    if (*BatteryTag == BATTERY_TAG_INVALID) {
        Status = STATUS_NO_SUCH_DEVICE;
    } else {
        Status = STATUS_SUCCESS;
    }

    DebugExitStatus(Status);
    return Status;
}

_Use_decl_annotations_
NTSTATUS QueryInformation (
    void* Context,
    ULONG BatteryTag,
    BATTERY_QUERY_INFORMATION_LEVEL Level,
    LONG AtRate,
    void* Buffer,
    ULONG BufferLength,
    ULONG* ReturnedLength)
/*++
Routine Description:
    Called by the class driver to retrieve battery information

    The battery class driver will serialize all requests it issues to
    the miniport for a given battery.

    Return invalid parameter when a request for a specific level of information
    can't be handled. This is defined in the battery class spec.

Arguments:
    Context - Supplies the miniport context value for battery

    BatteryTag - Supplies the tag of current battery

    Level - Supplies the type of information required

    AtRate - Supplies the rate of drain for the BatteryEstimatedTime level

    Buffer - Supplies a pointer to a buffer to place the information

    BufferLength - Supplies the length in bytes of the buffer

    ReturnedLength - Supplies the length in bytes of the returned data

Return Value:
    Success if there is a battery currently installed, else no such device.
--*/
{
    ULONG ResultValue;
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(AtRate);

    DebugEnter();

    BATT_FDO_DATA* DevExt = (BATT_FDO_DATA*)Context;
    WdfWaitLockAcquire(DevExt->StateLock, NULL);
    if (BatteryTag != DevExt->BatteryTag) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto QueryInformationEnd;
    }

    // Determine the value of the information being queried for and return it.
    // In a real battery, this would require hardware/firmware accesses. The
    // simulated battery fakes this by storing the data to be returned in
    // memory.
    void* ReturnBuffer = NULL;
    size_t ReturnBufferLength = 0;
    DebugPrint(DPFLTR_INFO_LEVEL, "Query for information level 0x%x\n", Level);
    Status = STATUS_INVALID_DEVICE_REQUEST;
    switch (Level) {
    case BatteryInformation:
        ReturnBuffer = &DevExt->State.BatteryInfo;
        ReturnBufferLength = sizeof(BATTERY_INFORMATION);
        Status = STATUS_SUCCESS;
        break;

    case BatteryEstimatedTime:
        if (DevExt->State.EstimatedTime == SIMBATT_RATE_CALCULATE) {
            if (AtRate == 0) {
                AtRate = DevExt->State.BatteryStatus.Rate;
            }

            if (AtRate < 0) {
                ResultValue = (3600 * DevExt->State.BatteryStatus.Capacity) /
                                (-AtRate);

            } else {
                ResultValue = BATTERY_UNKNOWN_TIME;
            }

        } else {
            ResultValue = DevExt->State.EstimatedTime;
        }

        ReturnBuffer = &ResultValue;
        ReturnBufferLength = sizeof(ResultValue);
        Status = STATUS_SUCCESS;
        break;

    case BatteryUniqueID:
        ReturnBuffer = DevExt->State.UniqueId;
        Status = RtlStringCbLengthW(DevExt->State.UniqueId,
                                    sizeof(DevExt->State.UniqueId),
                                    &ReturnBufferLength);

        ReturnBufferLength += sizeof(WCHAR);
        break;

    case BatteryManufactureName:
        ReturnBuffer = DevExt->State.ManufacturerName;
        Status = RtlStringCbLengthW(DevExt->State.ManufacturerName,
                                    sizeof(DevExt->State.ManufacturerName),
                                    &ReturnBufferLength);

        ReturnBufferLength += sizeof(WCHAR);
        break;

    case BatteryDeviceName:
        ReturnBuffer = DevExt->State.DeviceName;
        Status = RtlStringCbLengthW(DevExt->State.DeviceName,
                                    sizeof(DevExt->State.DeviceName),
                                    &ReturnBufferLength);

        ReturnBufferLength += sizeof(WCHAR);
        break;

    case BatterySerialNumber:
        ReturnBuffer = DevExt->State.SerialNumber;
        Status = RtlStringCbLengthW(DevExt->State.SerialNumber,
                                    sizeof(DevExt->State.SerialNumber),
                                    &ReturnBufferLength);

        ReturnBufferLength += sizeof(WCHAR);
        break;

    case BatteryManufactureDate:
        if (DevExt->State.ManufactureDate.Day != 0) {
            ReturnBuffer = &DevExt->State.ManufactureDate;
            ReturnBufferLength = sizeof(BATTERY_MANUFACTURE_DATE);
            Status = STATUS_SUCCESS;
        }

        break;

    case BatteryGranularityInformation:
        if (DevExt->State.GranularityCount > 0) {
            ReturnBuffer = DevExt->State.GranularityScale;
            ReturnBufferLength = DevExt->State.GranularityCount*sizeof(BATTERY_REPORTING_SCALE);

            Status = STATUS_SUCCESS;
        }

        break;

    case BatteryTemperature:
        ReturnBuffer = &DevExt->State.Temperature;
        ReturnBufferLength = sizeof(ULONG);
        Status = STATUS_SUCCESS;
        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    NT_ASSERT(((ReturnBufferLength == 0) && (ReturnBuffer == NULL)) ||
              ((ReturnBufferLength > 0)  && (ReturnBuffer != NULL)));

    if (NT_SUCCESS(Status)) {
        *ReturnedLength = (ULONG)ReturnBufferLength;
        if (ReturnBuffer != NULL) {
            if ((Buffer == NULL) || (BufferLength < ReturnBufferLength)) {
                Status = STATUS_BUFFER_TOO_SMALL;

            } else {
                memcpy(Buffer, ReturnBuffer, ReturnBufferLength);
            }
        }

    } else {
        *ReturnedLength = 0;
    }

QueryInformationEnd:
    WdfWaitLockRelease(DevExt->StateLock);
    DebugExitStatus(Status);
    return Status;
}

_Use_decl_annotations_
NTSTATUS QueryStatus (void* Context, ULONG BatteryTag, BATTERY_STATUS* BatteryStatus)
/*++
Routine Description:
    Called by the class driver to retrieve the batteries current status

    The battery class driver will serialize all requests it issues to
    the miniport for a given battery.

Arguments:
    Context - Supplies the miniport context value for battery

    BatteryTag - Supplies the tag of current battery

    BatteryStatus - Supplies a pointer to the structure to return the current
        battery status in

Return Value:
    Success if there is a battery currently installed, else no such device.
--*/
{
    NTSTATUS Status;

    DebugEnter();

    BATT_FDO_DATA* DevExt = (BATT_FDO_DATA*)Context;
    WdfWaitLockAcquire(DevExt->StateLock, NULL);
    if (BatteryTag != DevExt->BatteryTag) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto QueryStatusEnd;
    }

    RtlCopyMemory(BatteryStatus,
                  &DevExt->State.BatteryStatus,
                  sizeof(BATTERY_STATUS));

    Status = STATUS_SUCCESS;

QueryStatusEnd:
    WdfWaitLockRelease(DevExt->StateLock);
    DebugExitStatus(Status);
    return Status;
}

_Use_decl_annotations_
NTSTATUS SetStatusNotify (void* Context, ULONG BatteryTag, BATTERY_NOTIFY* BatteryNotify)
/*++
Routine Description:
    Called by the class driver to set the capacity and power state levels
    at which the class driver requires notification.

    The battery class driver will serialize all requests it issues to
    the miniport for a given battery.

Arguments:
    Context - Supplies the miniport context value for battery

    BatteryTag - Supplies the tag of current battery

    BatteryNotify - Supplies a pointer to a structure containing the
        notification critera.

Return Value:
    Success if there is a battery currently installed, else no such device.
--*/
{
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(BatteryNotify);

    DebugEnter();

    BATT_FDO_DATA* DevExt = (BATT_FDO_DATA*)Context;
    WdfWaitLockAcquire(DevExt->StateLock, NULL);
    if (BatteryTag != DevExt->BatteryTag) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto SetStatusNotifyEnd;
    }

    Status = STATUS_NOT_SUPPORTED;

SetStatusNotifyEnd:
    WdfWaitLockRelease(DevExt->StateLock);
    DebugExitStatus(Status);
    return Status;
}

_Use_decl_annotations_
NTSTATUS DisableStatusNotify (void* Context)
/*++
Routine Description:
    Called by the class driver to disable notification.

    The battery class driver will serialize all requests it issues to
    the miniport for a given battery.

Arguments:
    Context - Supplies the miniport context value for battery

Return Value:
    Success if there is a battery currently installed, else no such device.
--*/
{
    UNREFERENCED_PARAMETER(Context);

    DebugEnter();

    NTSTATUS Status = STATUS_NOT_SUPPORTED;
    DebugExitStatus(Status);
    return Status;
}

_Use_decl_annotations_
NTSTATUS SetInformation (
    void* Context,
    ULONG BatteryTag,
    BATTERY_SET_INFORMATION_LEVEL Level,
    void* Buffer)
/*
 Routine Description:
    Called by the class driver to set the battery's charge/discharge state,
    critical bias, or charge current.

Arguments:
    Context - Supplies the miniport context value for battery

    BatteryTag - Supplies the tag of current battery

    Level - Supplies action requested

    Buffer - Supplies a critical bias value if level is BatteryCriticalBias.
--*/
{
    NTSTATUS Status;
    UNREFERENCED_PARAMETER(Level);

    DebugEnter();

    BATT_FDO_DATA* DevExt = (BATT_FDO_DATA*)Context;
    WdfWaitLockAcquire(DevExt->StateLock, NULL);
    if (BatteryTag != DevExt->BatteryTag) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto SetInformationEnd;
    }

    if (Buffer == NULL) {
        Status = STATUS_INVALID_PARAMETER_4;
    } else {
        Status = STATUS_NOT_SUPPORTED;
    }

SetInformationEnd:
    WdfWaitLockRelease(DevExt->StateLock);
    DebugExitStatus(Status);
    return Status;
}

//------------------------------------------------- Battery Simulation Interface
//
// The following IO control handler and associated SimBattSetXxx routines
// implement the control side of the simulated battery. A real battery would
// not implement this interface, and instead read battery data from hardware/
// firmware interfaces.
void BattIoDeviceControl (
    WDFQUEUE Queue,
    WDFREQUEST Request,
    size_t OutputBufferLength,
    size_t InputBufferLength,
    ULONG IoControlCode)
/*++
Routine Description:
    Handle changes to the simulated battery state.

Arguments:
    Queue - Supplies a handle to the framework queue object that is associated
        with the I/O request.

    Request - Supplies a handle to a framework request object. This one
        represents the IRP_MJ_DEVICE_CONTROL IRP received by the framework.

    OutputBufferLength - Supplies the length, in bytes, of the request's output
        buffer, if an output buffer is available.

    InputBufferLength - Supplies the length, in bytes, of the request's input
        buffer, if an input buffer is available.

    IoControlCode - Supplies the Driver-defined or system-defined I/O control
        code (IOCTL) that is associated with the request.
--*/
{
    BATTERY_INFORMATION* BatteryInformation;
    BATTERY_STATUS* BatteryStatus;
    size_t Length;
    NTSTATUS TempStatus;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    ULONG BytesReturned = 0;
    WDFDEVICE Device = WdfIoQueueGetDevice(Queue);
    DebugPrint(DPFLTR_INFO_LEVEL, "BattIoDeviceControl: 0x%p\n", Device);
    NTSTATUS Status = STATUS_INVALID_PARAMETER;
    switch (IoControlCode) {
    case IOCTL_SIMBATT_SET_STATUS:
        TempStatus = WdfRequestRetrieveInputBuffer(Request, sizeof(BATTERY_STATUS), (void**)&BatteryStatus, &Length);

        if (NT_SUCCESS(TempStatus) && (Length == sizeof(BATTERY_STATUS))) {
            Status = SetBatteryStatus(Device, BatteryStatus);
        }

        break;

    case IOCTL_SIMBATT_SET_INFORMATION:
        TempStatus = WdfRequestRetrieveInputBuffer(Request, sizeof(BATTERY_INFORMATION), (void**)&BatteryInformation, &Length);

        if (NT_SUCCESS(TempStatus) && (Length == sizeof(BATTERY_INFORMATION))) {
            Status = SetBatteryInformation(Device, BatteryInformation);
        }

        break;

    default:
        break;
    }

    WdfRequestCompleteWithInformation(Request, Status, BytesReturned);
}

_Use_decl_annotations_
NTSTATUS SetBatteryStatus (WDFDEVICE Device, BATTERY_STATUS* BatteryStatus)
/*++
Routine Description:
    Set the simulated battery status structure values.

Arguments:
    Device - Supplies the device to set data for.

    BatteryStatus - Supplies the new status data to set.
--*/
{
    NTSTATUS Status = STATUS_INVALID_PARAMETER;
    BATT_FDO_DATA* DevExt = GetDeviceExtension(Device);
    ULONG ValidPowerState = BATTERY_CHARGING |
                      BATTERY_DISCHARGING |
                      BATTERY_CRITICAL |
                      BATTERY_POWER_ON_LINE;

    if ((BatteryStatus->PowerState & ~ValidPowerState) != 0) {
        goto SetBatteryStatusEnd;
    }

    WdfWaitLockAcquire(DevExt->StateLock, NULL);
    static_assert(sizeof(DevExt->State.BatteryStatus) == sizeof(*BatteryStatus));
    RtlCopyMemory(&DevExt->State.BatteryStatus, BatteryStatus, sizeof(BATTERY_STATUS));
    WdfWaitLockRelease(DevExt->StateLock);

    BatteryClassStatusNotify(DevExt->ClassHandle);
    Status = STATUS_SUCCESS;

SetBatteryStatusEnd:
    return Status;
}

_Use_decl_annotations_
NTSTATUS SetBatteryInformation (WDFDEVICE Device, BATTERY_INFORMATION* BatteryInformation)
/*++
Routine Description:
    Set the simulated battery information structure values.

Arguments:
    Device - Supplies the device to set data for.

    BatteryInformation - Supplies the new information data to set.
--*/
{
    NTSTATUS Status = STATUS_INVALID_PARAMETER;
    BATT_FDO_DATA* DevExt = GetDeviceExtension(Device);
    ULONG ValidCapabilities = BATTERY_CAPACITY_RELATIVE |
                        BATTERY_IS_SHORT_TERM |
                        BATTERY_SYSTEM_BATTERY;

    if ((BatteryInformation->Capabilities & ~ValidCapabilities) != 0) {
        goto SetBatteryInformationEnd;
    }

    if (BatteryInformation->Technology > 1) {
        goto SetBatteryInformationEnd;
    }

    WdfWaitLockAcquire(DevExt->StateLock, NULL);
    static_assert(sizeof(DevExt->State.BatteryInfo) == sizeof(*BatteryInformation));
    RtlCopyMemory(&DevExt->State.BatteryInfo, BatteryInformation, sizeof(BATTERY_INFORMATION));

    // To indicate that battery information has changed, update the battery tag
    // and notify the class driver that the battery status has updated. The
    // status query will fail due to a different battery tag, causing the class
    // driver to query for the new tag and new information.
    UpdateTag(DevExt);
    WdfWaitLockRelease(DevExt->StateLock);
    BatteryClassStatusNotify(DevExt->ClassHandle);
    Status = STATUS_SUCCESS;

SetBatteryInformationEnd:
    return Status;
}
