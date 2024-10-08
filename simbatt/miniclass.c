/*++
    This module implements battery miniclass functionality specific to the
    simulated battery driver.
--*/

#include "simbatt.h"
#include "simbattdriverif.h"

//--------------------------------------------------------------------- Literals

#define DEFAULT_NAME                L"SimulatedBattery"
#define DEFAULT_MANUFACTURER        L"Microsoft Corp"
#define DEFAULT_SERIALNO            L"0000"
#define DEFAULT_UNIQUEID            L"SimulatedBattery0000"

//------------------------------------------------------------------- Prototypes

_IRQL_requires_same_
VOID
SimBattUpdateTag (
    _Inout_ SIMBATT_FDO_DATA* DevExt
    );

BCLASS_QUERY_TAG_CALLBACK SimBattQueryTag;
BCLASS_QUERY_INFORMATION_CALLBACK SimBattQueryInformation;
BCLASS_SET_INFORMATION_CALLBACK SimBattSetInformation;
BCLASS_QUERY_STATUS_CALLBACK SimBattQueryStatus;
BCLASS_SET_STATUS_NOTIFY_CALLBACK SimBattSetStatusNotify;
BCLASS_DISABLE_STATUS_NOTIFY_CALLBACK SimBattDisableStatusNotify;

_Must_inspect_result_
_Success_(return==STATUS_SUCCESS)
NTSTATUS
SimBattSetBatteryStatus (
    _In_ WDFDEVICE Device,
    _In_ PBATTERY_STATUS BatteryStatus
    );

_Must_inspect_result_
_Success_(return==STATUS_SUCCESS)
NTSTATUS
SimBattSetBatteryInformation (
    _In_ WDFDEVICE Device,
    _In_ PBATTERY_INFORMATION BatteryInformation
    );

_Success_(return==STATUS_SUCCESS)
NTSTATUS
SimBattSetBatteryString (
    _In_ PCWSTR String,
    _Out_writes_(MAX_BATTERY_STRING_SIZE) PWCHAR Destination
    );

//------------------------------------------------------------ Battery Interface

_Use_decl_annotations_
VOID
SimBattPrepareHardware (
    WDFDEVICE Device
    )
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

    SIMBATT_FDO_DATA* DevExt = GetDeviceExtension(Device);

    // Get this battery's state - use defaults.
    {
        WdfWaitLockAcquire(DevExt->StateLock, NULL);
        SimBattUpdateTag(DevExt);

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
        DevExt->State.BatteryInfo.DesignedCapacity = 100;
        DevExt->State.BatteryInfo.FullChargedCapacity = 100;
        DevExt->State.BatteryInfo.DefaultAlert1 = 0;
        DevExt->State.BatteryInfo.DefaultAlert2 = 0;
        DevExt->State.BatteryInfo.CriticalBias = 0;
        DevExt->State.BatteryInfo.CycleCount = 100;

        DevExt->State.BatteryStatus.PowerState = BATTERY_POWER_ON_LINE;
        DevExt->State.BatteryStatus.Capacity = 100;
        DevExt->State.BatteryStatus.Voltage = BATTERY_UNKNOWN_VOLTAGE;
        DevExt->State.BatteryStatus.Rate = 0;

        //DevExt->State.GranularityCount = 0;
        //for (unsigned int i = 0; i < DevExt->State.GranularityCount; ++i) {
        //    DevExt->State.GranularityScale[i].Granularity = 0; // granularity [mWh]
        //    DevExt->State.GranularityScale[i].Capacity = 0; // upper capacity limit for Granularity [mWh]
        //}

        DevExt->State.EstimatedTime = BATTERY_UNKNOWN_TIME; // battery run time, in seconds

        DevExt->State.Temperature = 2931; // 20 degree Celsius [10ths of a degree Kelvin]

        DevExt->State.MaxCurrentDraw = UNKNOWN_CURRENT;

        SimBattSetBatteryString(DEFAULT_NAME, DevExt->State.DeviceName);

        SimBattSetBatteryString(DEFAULT_MANUFACTURER, DevExt->State.ManufacturerName);

        SimBattSetBatteryString(DEFAULT_SERIALNO, DevExt->State.SerialNumber);

        SimBattSetBatteryString(DEFAULT_UNIQUEID, DevExt->State.UniqueId);

        WdfWaitLockRelease(DevExt->StateLock);
    }

    return;
}

_Use_decl_annotations_
VOID
SimBattUpdateTag (
    SIMBATT_FDO_DATA* DevExt
    )
/*++
Routine Description:
    This routine is called when static battery properties have changed to
    update the battery tag.

Arguments:
    DevExt - Supplies a pointer to the device extension  of the battery to
        update.
--*/
{
    DevExt->BatteryTag += 1;
    if (DevExt->BatteryTag == BATTERY_TAG_INVALID) {
        DevExt->BatteryTag += 1;
    }

    return;
}

_Use_decl_annotations_
NTSTATUS
SimBattQueryTag (
    PVOID Context,
    PULONG BatteryTag
    )
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

    SIMBATT_FDO_DATA* DevExt = (SIMBATT_FDO_DATA*)Context;
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
NTSTATUS
SimBattQueryInformation (
    PVOID Context,
    ULONG BatteryTag,
    BATTERY_QUERY_INFORMATION_LEVEL Level,
    LONG AtRate,
    PVOID Buffer,
    ULONG BufferLength,
    PULONG ReturnedLength
    )
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

    SIMBATT_FDO_DATA* DevExt = (SIMBATT_FDO_DATA*)Context;
    WdfWaitLockAcquire(DevExt->StateLock, NULL);
    if (BatteryTag != DevExt->BatteryTag) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto QueryInformationEnd;
    }

    // Determine the value of the information being queried for and return it.
    // In a real battery, this would require hardware/firmware accesses. The
    // simulated battery fakes this by storing the data to be returned in
    // memory.

    PVOID ReturnBuffer = NULL;
    size_t ReturnBufferLength = 0;
    DebugPrint(SIMBATT_INFO, "Query for information level 0x%x\n", Level);
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
NTSTATUS
SimBattQueryStatus (
    PVOID Context,
    ULONG BatteryTag,
    PBATTERY_STATUS BatteryStatus
    )
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

    SIMBATT_FDO_DATA* DevExt = (SIMBATT_FDO_DATA*)Context;
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
NTSTATUS
SimBattSetStatusNotify (
    PVOID Context,
    ULONG BatteryTag,
    PBATTERY_NOTIFY BatteryNotify
    )
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

    SIMBATT_FDO_DATA* DevExt = (SIMBATT_FDO_DATA*)Context;
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
NTSTATUS
SimBattDisableStatusNotify (
    PVOID Context
    )
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
NTSTATUS
SimBattSetInformation (
    PVOID Context,
    ULONG BatteryTag,
    BATTERY_SET_INFORMATION_LEVEL Level,
    PVOID Buffer
    )
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

    DebugEnter();

    SIMBATT_FDO_DATA* DevExt = (SIMBATT_FDO_DATA*)Context;
    WdfWaitLockAcquire(DevExt->StateLock, NULL);
    if (BatteryTag != DevExt->BatteryTag) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto SetInformationEnd;
    }

    if (Buffer == NULL) {
        Status = STATUS_INVALID_PARAMETER_4;

    } else if (Level == BatteryChargingSource) {
        PBATTERY_CHARGING_SOURCE ChargingSource = (PBATTERY_CHARGING_SOURCE)Buffer;
        DevExt->State.MaxCurrentDraw = ChargingSource->MaxCurrent;
        DebugPrint(SIMBATT_INFO,
                   "SimBatt : Set MaxCurrentDraw = %u mA\n",
                   DevExt->State.MaxCurrentDraw);

        Status = STATUS_SUCCESS;
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
VOID
SimBattIoDeviceControl (
    WDFQUEUE Queue,
    WDFREQUEST Request,
    size_t OutputBufferLength,
    size_t InputBufferLength,
    ULONG IoControlCode
    )
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
    PBATTERY_INFORMATION BatteryInformation;
    PBATTERY_STATUS BatteryStatus;
    size_t Length;
    NTSTATUS TempStatus;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    ULONG BytesReturned = 0;
    WDFDEVICE Device = WdfIoQueueGetDevice(Queue);
    DebugPrint(SIMBATT_INFO, "SimBattIoDeviceControl: 0x%p\n", Device);
    NTSTATUS Status = STATUS_INVALID_PARAMETER;
    switch (IoControlCode) {
    case IOCTL_SIMBATT_SET_STATUS:
        TempStatus = WdfRequestRetrieveInputBuffer(Request, sizeof(BATTERY_STATUS), &BatteryStatus, &Length);

        if (NT_SUCCESS(TempStatus) && (Length == sizeof(BATTERY_STATUS))) {
            Status = SimBattSetBatteryStatus(Device, BatteryStatus);
        }

        break;

    case IOCTL_SIMBATT_SET_INFORMATION:
        TempStatus = WdfRequestRetrieveInputBuffer(Request, sizeof(BATTERY_INFORMATION), &BatteryInformation, &Length);

        if (NT_SUCCESS(TempStatus) && (Length == sizeof(BATTERY_INFORMATION))) {
            Status = SimBattSetBatteryInformation(Device, BatteryInformation);
        }

        break;

    default:
        break;
    }

    WdfRequestCompleteWithInformation(Request, Status, BytesReturned);
    return;
}

_Use_decl_annotations_
NTSTATUS
SimBattSetBatteryStatus (
    WDFDEVICE Device,
    PBATTERY_STATUS BatteryStatus
    )
/*++
Routine Description:
    Set the simulated battery status structure values.

Arguments:
    Device - Supplies the device to set data for.

    BatteryStatus - Supplies the new status data to set.
--*/
{
    NTSTATUS Status = STATUS_INVALID_PARAMETER;
    SIMBATT_FDO_DATA* DevExt = GetDeviceExtension(Device);
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
NTSTATUS
SimBattSetBatteryInformation (
    WDFDEVICE Device,
    PBATTERY_INFORMATION BatteryInformation
    )
/*++
Routine Description:
    Set the simulated battery information structure values.

Arguments:
    Device - Supplies the device to set data for.

    BatteryInformation - Supplies the new information data to set.
--*/
{
    NTSTATUS Status = STATUS_INVALID_PARAMETER;
    SIMBATT_FDO_DATA* DevExt = GetDeviceExtension(Device);
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
    SimBattUpdateTag(DevExt);
    WdfWaitLockRelease(DevExt->StateLock);
    BatteryClassStatusNotify(DevExt->ClassHandle);
    Status = STATUS_SUCCESS;

SetBatteryInformationEnd:
    return Status;
}

_Use_decl_annotations_
NTSTATUS
SimBattSetBatteryString (
    PCWSTR String,
    PWCHAR Destination
    )
/*++
Routine Description:
    Set one of the simulated battery strings.

Arguments:
    String - Supplies the new string value to set.

    Destination - Supplies a pointer to the buffer to store the new string.
--*/
{
    return RtlStringCchCopyW(Destination, MAX_BATTERY_STRING_SIZE, String);
}

_Use_decl_annotations_
VOID
SimBattPrint (
    ULONG Level,
    PCSTR Format,
    ...
    )

/*++
Routine Description:
    This routine emits the debugger message.

Arguments:
    Level - Supplies the criticality of message being printed.

    Format - Message to be emitted in varible argument format.

--*/
{
	va_list Arglist;
	va_start(Arglist, Format);
	vDbgPrintEx(DPFLTR_IHVDRIVER_ID, Level, Format, Arglist);
}
