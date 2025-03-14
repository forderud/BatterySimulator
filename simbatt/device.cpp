/*++
    This module implements WDF and WDM functionality required to register as a
    device driver, instantiate devices, and register those devices with the
    battery class driver.
--*/

#include "battery.hpp"
#include "simbattdriverif.h"
#include "wmi.hpp"

//------------------------------------------------------------------- Prototypes

EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT  BattSelfManagedIoInit;
EVT_WDF_DEVICE_SELF_MANAGED_IO_CLEANUP  BattSelfManagedIoCleanup;
EVT_WDF_DEVICE_QUERY_STOP BattQueryStop;
EVT_WDF_DEVICE_PREPARE_HARDWARE BattDevicePrepareHardware;

//-------------------------------------------------------------------- Functions

_Use_decl_annotations_
NTSTATUS BattDriverDeviceAdd (WDFDRIVER Driver, WDFDEVICE_INIT* DeviceInit)
/*++
Routine Description:
    EvtDriverDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager. A WDF device object is created and initialized to
    represent a new instance of the battery device.

Arguments:
    Driver - Supplies a handle to the WDF Driver object.

    DeviceInit - Supplies a pointer to a framework-allocated WDFDEVICE_INIT
        structure.
--*/
{
    UNREFERENCED_PARAMETER(Driver);
    DebugEnter();

    // Initialize the PnpPowerCallbacks structure.  Callback events for PNP
    // and Power are specified here.
    WDF_PNPPOWER_EVENT_CALLBACKS PnpPowerCallbacks;
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&PnpPowerCallbacks);
    PnpPowerCallbacks.EvtDevicePrepareHardware = BattDevicePrepareHardware;
    PnpPowerCallbacks.EvtDeviceSelfManagedIoInit = BattSelfManagedIoInit;
    PnpPowerCallbacks.EvtDeviceSelfManagedIoCleanup = BattSelfManagedIoCleanup;
    PnpPowerCallbacks.EvtDeviceQueryStop = BattQueryStop;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &PnpPowerCallbacks);

    // Register WDM preprocess callbacks for IRP_MJ_DEVICE_CONTROL and
    // IRP_MJ_SYSTEM_CONTROL. The battery class driver needs to handle these IO
    // requests directly.
    NTSTATUS status = WdfDeviceInitAssignWdmIrpPreprocessCallback(
                 DeviceInit,
                 BattWdmIrpPreprocessDeviceControl,
                 IRP_MJ_DEVICE_CONTROL,
                 NULL,
                 0);
    if (!NT_SUCCESS(status)) {
         DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("WdfDeviceInitAssignWdmIrpPreprocessCallback(IRP_MJ_DEVICE_CONTROL) Failed. 0x%x"), status);
         goto DriverDeviceAddEnd;
    }

    status = WdfDeviceInitAssignWdmIrpPreprocessCallback(
                 DeviceInit,
                 BattWdmIrpPreprocessSystemControl,
                 IRP_MJ_SYSTEM_CONTROL,
                 NULL,
                 0);
    if (!NT_SUCCESS(status)) {
         DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("WdfDeviceInitAssignWdmIrpPreprocessCallback(IRP_MJ_SYSTEM_CONTROL) Failed. 0x%x"), status);
         goto DriverDeviceAddEnd;
    }

    // Initialize attributes and a context area for the device object.
    WDF_OBJECT_ATTRIBUTES DeviceAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&DeviceAttributes);
    WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&DeviceAttributes, DEVICE_CONTEXT);

    // Create a framework device object.  This call will in turn create
    // a WDM device object, attach to the lower stack, and set the
    // appropriate flags and attributes.
    WDFDEVICE DeviceHandle = 0;
    status = WdfDeviceCreate(&DeviceInit, &DeviceAttributes, &DeviceHandle);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("WdfDeviceCreate() Failed. 0x%x"), status);
        goto DriverDeviceAddEnd;
    }

    DebugPrint(DPFLTR_INFO_LEVEL, "Device PDO(0x%p) FDO(0x%p), Lower(0x%p)\n", WdfDeviceWdmGetPhysicalDevice(DeviceHandle), WdfDeviceWdmGetDeviceObject(DeviceHandle), WdfDeviceWdmGetAttachedDevice(DeviceHandle));

    // Configure a default queue for IO requests that are not handled by the
    // class driver. For the simulated battery, this queue processes requests
    // to set the simulated status.
    WDF_IO_QUEUE_CONFIG QueueConfig;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&QueueConfig,
                                           WdfIoQueueDispatchSequential);

    QueueConfig.EvtIoDeviceControl = BattIoDeviceControl;
    WDFQUEUE Queue;
    status = WdfIoQueueCreate(DeviceHandle,
                              &QueueConfig,
                              WDF_NO_OBJECT_ATTRIBUTES,
                              &Queue);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("WdfIoQueueCreate() Failed. 0x%x"), status);
        goto DriverDeviceAddEnd;
    }

    // Create a device interface for this device to advertise the simulated
    // battery IO interface.
    status = WdfDeviceCreateDeviceInterface(DeviceHandle,
                                            &SIMBATT_DEVINTERFACE_GUID,
                                            NULL);
    if (!NT_SUCCESS(status)) {
        goto DriverDeviceAddEnd;
    }

    // Finish initializing the device context area.
    DEVICE_CONTEXT* DevExt = WdfObjectGet_DEVICE_CONTEXT(DeviceHandle);
    DevExt->BatteryTag = BATTERY_TAG_INVALID;
    DevExt->ClassHandle = NULL;

    WDF_OBJECT_ATTRIBUTES LockAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&LockAttributes);
    LockAttributes.ParentObject = DeviceHandle;

    status = WdfWaitLockCreate(&LockAttributes, &DevExt->ClassInitLock);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("WdfWaitLockCreate(ClassInitLock) Failed. Status 0x%x"), status);
        goto DriverDeviceAddEnd;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&LockAttributes);
    LockAttributes.ParentObject = DeviceHandle;

    status = WdfWaitLockCreate(&LockAttributes, &DevExt->StateLock);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("WdfWaitLockCreate(StateLock) Failed. Status 0x%x"), status);
        goto DriverDeviceAddEnd;
    }

DriverDeviceAddEnd:
    DebugExitStatus(status);
    return status;
}

_Use_decl_annotations_
NTSTATUS BattSelfManagedIoInit (WDFDEVICE Device)
/*++
Routine Description:
    The framework calls this function once per device after EvtDeviceD0Entry
    callback has been called for the first time. This function is not called
    again unless device is remove and reconnected or the drivers are reloaded.

Arguments:
    Device - Supplies a handle to a framework device object.
--*/
{
    DebugEnter();
    NTSTATUS Status = InitializeBatteryClass(Device);
    if (!NT_SUCCESS(Status)) {
        goto DevicePrepareHardwareEnd;
    }

    RegisterWMI(Device);

DevicePrepareHardwareEnd:
    DebugExitStatus(Status);
    return Status;
}

_Use_decl_annotations_
void BattSelfManagedIoCleanup (WDFDEVICE Device)
/*++
Routine Description:
    This function is called after EvtDeviceSelfManagedIoSuspend callback. This
    function must release any sel-managed I/O operation data.

Arguments:
    Device - Supplies a handle to a framework device object.

Return Value:
    NTSTATUS - Failures will be logged, but not acted on.
--*/
{
    DebugEnter();

    UnregisterWMI(Device);

    NTSTATUS status = UnloadBatteryClass(Device);

    DebugExitStatus(status);
}

_Use_decl_annotations_
NTSTATUS BattQueryStop (_In_ WDFDEVICE Device)
/*++
Routine Description:
    EvtDeviceQueryStop event callback function determines whether a specified 
    device can be stopped so that the PnP manager can redistribute system 
    hardware resources.

    SimBatt is designed to fail a rebalance operation, for reasons described 
    below. Note however that this approach must *not* be adopted by an actual
    battery driver. 

    SimBatt unregisters itself as a Battery driver by calling 
    BatteryClassUnload() when IRP_MN_STOP_DEVICE arrives at the driver. It
    re-establishes itself as a Battery driver on arrival of IRP_MN_START_DEVICE.
    This results in any IOCTLs normally handeled by the Battery Class driver to 
    be delivered to SimBatt. The IO Queue employed by SimBatt is power managed,
    it causes these IOCTLs to be pended when SimBatt is not in D0. Now if the 
    device attempts to do a shutdown while an IOCTL is pended in SimBatt, it
    would result in a 0x9F bugcheck. By opting out of PNP rebalance operation 
    SimBatt circumvents this issue.

Arguments:
    Device - Supplies a handle to a framework device object.
--*/
{
    UNREFERENCED_PARAMETER(Device);
    return STATUS_UNSUCCESSFUL;
}

_Use_decl_annotations_
NTSTATUS BattDevicePrepareHardware (WDFDEVICE Device, WDFCMRESLIST ResourcesRaw, WDFCMRESLIST ResourcesTranslated)
/*++
Routine Description:
    EvtDevicePrepareHardware event callback performs operations that are
    necessary to make the driver's device operational. The framework calls the
    driver's EvtDevicePrepareHardware callback when the PnP manager sends an
    IRP_MN_START_DEVICE request to the driver stack.

Arguments:
    Device - Supplies a handle to a framework device object.

    ResourcesRaw - Supplies a handle to a collection of framework resource
        objects. This collection identifies the raw (bus-relative) hardware
        resources that have been assigned to the device.

    ResourcesTranslated - Supplies a handle to a collection of framework
        resource objects. This collection identifies the translated
        (system-physical) hardware resources that have been assigned to the
        device. The resources appear from the CPU's point of view. Use this list
        of resources to map I/O space and device-accessible memory into virtual
        address space
--*/
{
    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    DebugEnter();

    InitializeBatteryState(Device);
    NTSTATUS Status = STATUS_SUCCESS;
    DebugExitStatus(Status);
    return Status;
}
