/*++
    This module implements WDF and WDM functionality required to register as a
    device driver, instantiate devices, and register those devices with the
    battery class driver.
--*/

#include "simbatt.h"
#include "simbattdriverif.h"
#include "wmi.h"

//------------------------------------------------------------------- Prototypes

extern "C" DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD BattDriverDeviceAdd;
EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT  BattSelfManagedIoInit;
EVT_WDF_DEVICE_SELF_MANAGED_IO_CLEANUP  SimBattSelfManagedIoCleanup;
EVT_WDF_DEVICE_QUERY_STOP SimBattQueryStop;
EVT_WDF_DEVICE_PREPARE_HARDWARE SimBattDevicePrepareHardware;
EVT_WDFDEVICE_WDM_IRP_PREPROCESS SimBattWdmIrpPreprocessDeviceControl;
EVT_WDFDEVICE_WDM_IRP_PREPROCESS SimBattWdmIrpPreprocessSystemControl;

//-------------------------------------------------------------------- Functions

_Use_decl_annotations_
NTSTATUS DriverEntry (DRIVER_OBJECT* DriverObject, UNICODE_STRING* RegistryPath)
/*++
Routine Description:
    DriverEntry initializes the driver and is the first routine called by the
    system after the driver is loaded. DriverEntry configures and creates a WDF
    driver object.

Parameters Description:
    DriverObject - Supplies a pointer to the driver object.

    RegistryPath - Supplies a pointer to a unicode string representing the path
        to the driver-specific key in the registry.
--*/
{
    DebugEnter();

    WDF_DRIVER_CONFIG DriverConfig;
    WDF_DRIVER_CONFIG_INIT(&DriverConfig, BattDriverDeviceAdd);

    // Initialize attributes and a context area for the driver object.
    //
    // N.B. ExecutionLevel is set to Passive because this driver expect callback
    //      functions to be called at PASSIVE_LEVEL.
    //
    // N.B. SynchronizationScope is not specified and therefore it is set to
    //      None. This means that the WDF framework does not synchronize the
    //      callbacks, you may want to set this to a different value based on
    //      how the callbacks are required to be synchronized in your driver.
    WDF_OBJECT_ATTRIBUTES DriverAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&DriverAttributes);
    WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&DriverAttributes,
                                           SIMBATT_GLOBAL_DATA);

    DriverAttributes.ExecutionLevel = WdfExecutionLevelPassive;

    // Create the driver object
    NTSTATUS Status = WdfDriverCreate(DriverObject,
                             RegistryPath,
                             &DriverAttributes,
                             &DriverConfig,
                             WDF_NO_HANDLE);

    if (!NT_SUCCESS(Status)) {
        DebugPrint(SIMBATT_ERROR,
                   "WdfDriverCreate() Failed. Status 0x%x\n",
                   Status);

        goto DriverEntryEnd;
    }

    SIMBATT_GLOBAL_DATA* GlobalData = GetGlobalData(WdfGetDriver());
    GlobalData->RegistryPath.MaximumLength = RegistryPath->Length +
                                             sizeof(UNICODE_NULL);

    GlobalData->RegistryPath.Length = RegistryPath->Length;
    GlobalData->RegistryPath.Buffer = WdfDriverGetRegistryPath(WdfGetDriver());

DriverEntryEnd:
    DebugExitStatus(Status);
    return Status;
}

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
    PnpPowerCallbacks.EvtDevicePrepareHardware = SimBattDevicePrepareHardware;
    PnpPowerCallbacks.EvtDeviceSelfManagedIoInit = BattSelfManagedIoInit;
    PnpPowerCallbacks.EvtDeviceSelfManagedIoCleanup = SimBattSelfManagedIoCleanup;
    PnpPowerCallbacks.EvtDeviceQueryStop = SimBattQueryStop;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &PnpPowerCallbacks);

    // Register WDM preprocess callbacks for IRP_MJ_DEVICE_CONTROL and
    // IRP_MJ_SYSTEM_CONTROL. The battery class driver needs to handle these IO
    // requests directly.
    NTSTATUS Status = WdfDeviceInitAssignWdmIrpPreprocessCallback(
                 DeviceInit,
                 SimBattWdmIrpPreprocessDeviceControl,
                 IRP_MJ_DEVICE_CONTROL,
                 NULL,
                 0);

    if (!NT_SUCCESS(Status)) {
         DebugPrint(SIMBATT_ERROR,
                    "WdfDeviceInitAssignWdmIrpPreprocessCallback"
                    "(IRP_MJ_DEVICE_CONTROL) Failed. 0x%x\n",
                    Status);

         goto DriverDeviceAddEnd;
    }

    Status = WdfDeviceInitAssignWdmIrpPreprocessCallback(
                 DeviceInit,
                 SimBattWdmIrpPreprocessSystemControl,
                 IRP_MJ_SYSTEM_CONTROL,
                 NULL,
                 0);

    if (!NT_SUCCESS(Status)) {
         DebugPrint(SIMBATT_ERROR,
                    "WdfDeviceInitAssignWdmIrpPreprocessCallback"
                    "(IRP_MJ_SYSTEM_CONTROL) Failed. 0x%x\n",
                    Status);

         goto DriverDeviceAddEnd;
    }

    // Initialize attributes and a context area for the device object.
    WDF_OBJECT_ATTRIBUTES DeviceAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&DeviceAttributes);
    WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&DeviceAttributes, SIMBATT_FDO_DATA);

    // Create a framework device object.  This call will in turn create
    // a WDM device object, attach to the lower stack, and set the
    // appropriate flags and attributes.
    WDFDEVICE DeviceHandle;
    Status = WdfDeviceCreate(&DeviceInit, &DeviceAttributes, &DeviceHandle);
    if (!NT_SUCCESS(Status)) {
        DebugPrint(SIMBATT_ERROR, "WdfDeviceCreate() Failed. 0x%x\n", Status);
        goto DriverDeviceAddEnd;
    }

    // Configure a default queue for IO requests that are not handled by the
    // class driver. For the simulated battery, this queue processes requests
    // to set the simulated status.
    WDF_IO_QUEUE_CONFIG QueueConfig;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&QueueConfig,
                                           WdfIoQueueDispatchSequential);

    QueueConfig.EvtIoDeviceControl = BattIoDeviceControl;
    WDFQUEUE Queue;
    Status = WdfIoQueueCreate(DeviceHandle,
                              &QueueConfig,
                              WDF_NO_OBJECT_ATTRIBUTES,
                              &Queue);

    if (!NT_SUCCESS(Status)) {
        DebugPrint(SIMBATT_ERROR, "WdfIoQueueCreate() Failed. 0x%x\n", Status);
        goto DriverDeviceAddEnd;
    }

    // Create a device interface for this device to advertise the simulated
    // battery IO interface.
    Status = WdfDeviceCreateDeviceInterface(DeviceHandle,
                                            &SIMBATT_DEVINTERFACE_GUID,
                                            NULL);

    if (!NT_SUCCESS(Status)) {
        goto DriverDeviceAddEnd;
    }

    // Finish initializing the device context area.
    SIMBATT_FDO_DATA* DevExt = GetDeviceExtension(DeviceHandle);
    DevExt->BatteryTag = BATTERY_TAG_INVALID;
    DevExt->ClassHandle = NULL;
    WDF_OBJECT_ATTRIBUTES LockAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&LockAttributes);
    LockAttributes.ParentObject = DeviceHandle;
    Status = WdfWaitLockCreate(&LockAttributes,
                               &DevExt->ClassInitLock);

    if (!NT_SUCCESS(Status)) {
        DebugPrint(SIMBATT_ERROR,
                   "WdfWaitLockCreate(ClassInitLock) Failed. Status 0x%x\n",
                   Status);

        goto DriverDeviceAddEnd;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&LockAttributes);
    LockAttributes.ParentObject = DeviceHandle;
    Status = WdfWaitLockCreate(&LockAttributes,
                               &DevExt->StateLock);

    if (!NT_SUCCESS(Status)) {
        DebugPrint(SIMBATT_ERROR,
                   "WdfWaitLockCreate(StateLock) Failed. Status 0x%x\n",
                   Status);

        goto DriverDeviceAddEnd;
    }

DriverDeviceAddEnd:
    DebugExitStatus(Status);
    return Status;
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
    SIMBATT_FDO_DATA* DevExt = GetDeviceExtension(Device);

    // Attach to the battery class driver.

    BATTERY_MINIPORT_INFO_V1_1 BattInit = {0};
    BattInit.MajorVersion = BATTERY_CLASS_MAJOR_VERSION;
    BattInit.MinorVersion = BATTERY_CLASS_MINOR_VERSION_1;
    BattInit.Context = DevExt;
    BattInit.QueryTag = QueryTag;
    BattInit.QueryInformation = QueryInformation;
    BattInit.SetInformation = SetInformation;
    BattInit.QueryStatus = QueryStatus;
    BattInit.SetStatusNotify = SetStatusNotify;
    BattInit.DisableStatusNotify = DisableStatusNotify;
    BattInit.Pdo = WdfDeviceWdmGetPhysicalDevice(Device);
    BattInit.DeviceName = NULL;
    BattInit.Fdo = WdfDeviceWdmGetDeviceObject(Device);
    WdfWaitLockAcquire(DevExt->ClassInitLock, NULL);
    NTSTATUS Status = BatteryClassInitializeDevice((PBATTERY_MINIPORT_INFO)&BattInit,
                                          &DevExt->ClassHandle);

    WdfWaitLockRelease(DevExt->ClassInitLock);
    if (!NT_SUCCESS(Status)) {
        goto DevicePrepareHardwareEnd;
    }

    RegisterWMI(Device);

DevicePrepareHardwareEnd:
    DebugExitStatus(Status);
    return Status;
}

_Use_decl_annotations_
void SimBattSelfManagedIoCleanup (WDFDEVICE Device)
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

    SIMBATT_FDO_DATA* DevExt = GetDeviceExtension(Device);
    WdfWaitLockAcquire(DevExt->ClassInitLock, NULL);

    NTSTATUS Status = STATUS_SUCCESS;
    if (DevExt->ClassHandle != NULL) {
        Status = BatteryClassUnload(DevExt->ClassHandle);
        DevExt->ClassHandle = NULL;
    }

    WdfWaitLockRelease(DevExt->ClassInitLock);
    DebugExitStatus(Status);
}

_Use_decl_annotations_
NTSTATUS SimBattQueryStop (_In_ WDFDEVICE Device)
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
NTSTATUS SimBattDevicePrepareHardware (WDFDEVICE Device, WDFCMRESLIST ResourcesRaw, WDFCMRESLIST ResourcesTranslated)
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

_Use_decl_annotations_
NTSTATUS SimBattWdmIrpPreprocessDeviceControl (WDFDEVICE Device, IRP* Irp)
/*++
Routine Description:
    This event is called when the framework receives IRP_MJ_DEVICE_CONTROL
    requests from the system.

    N.B. Battery stack requires the device IOCTLs be sent to it at
         PASSIVE_LEVEL only, any IOCTL comming from user mode is therefore
         fine, kernel components, such as filter drivers sitting on top of
         the battery drivers should be careful to not voilate this
         requirement.

Arguments:
    Device - Supplies a handle to a framework device object.

    Irp - Supplies the IO request being processed.
--*/
{
    DebugEnter();

    ASSERTMSG("Must be called at IRQL = PASSIVE_LEVEL",
              (KeGetCurrentIrql() == PASSIVE_LEVEL));

    SIMBATT_FDO_DATA* DevExt = GetDeviceExtension(Device);
    NTSTATUS Status = STATUS_NOT_SUPPORTED;

    // Suppress 28118:Irq Exceeds Caller, see Routine Description for
    // explaination.
    #pragma warning(suppress: 28118)
    WdfWaitLockAcquire(DevExt->ClassInitLock, NULL);

    // N.B. An attempt to queue the IRP with the port driver should happen
    //      before WDF assumes ownership of this IRP, i.e. before
    //      WdfDeviceWdmDispatchPreprocessedIrp is called, this is so that the
    //      Battery port driver, which is a WDM driver, may complete the IRP if
    //      it does endup procesing it.
    if (DevExt->ClassHandle != NULL) {
        // Suppress 28118:Irq Exceeds Caller, see above N.B.
        #pragma warning(suppress: 28118)
        Status = BatteryClassIoctl(DevExt->ClassHandle, Irp);
    }

    WdfWaitLockRelease(DevExt->ClassInitLock);
    if (Status == STATUS_NOT_SUPPORTED) {
        IoSkipCurrentIrpStackLocation(Irp);
        Status = WdfDeviceWdmDispatchPreprocessedIrp(Device, Irp);
    }

    DebugExitStatus(Status);
    return Status;
}

_Use_decl_annotations_
NTSTATUS SimBattWdmIrpPreprocessSystemControl (WDFDEVICE Device, IRP* Irp)
/*++
Routine Description:
    This event is called when the framework receives IRP_MJ_SYSTEM_CONTROL
    requests from the system.

    N.B. Battery stack requires the device IOCTLs be sent to it at
         PASSIVE_LEVEL only, any IOCTL comming from user mode is therefore
         fine, kernel components, such as filter drivers sitting on top of
         the battery drivers should be careful to not voilate this
         requirement.

Arguments:
    Device - Supplies a handle to a framework device object.

    Irp - Supplies the IO request being processed.
--*/
{
    DebugEnter();
    ASSERTMSG("Must be called at IRQL = PASSIVE_LEVEL",(KeGetCurrentIrql() == PASSIVE_LEVEL));

    NTSTATUS Status = STATUS_NOT_IMPLEMENTED;
    SIMBATT_FDO_DATA* DevExt = GetDeviceExtension(Device);
    SYSCTL_IRP_DISPOSITION Disposition = IrpForward;

    // Acquire the class initialization lock and attempt to queue the IRP with
    // the class driver.
    //
    // Suppress 28118:Irq Exceeds Caller, see Routine Description for
    // explaination.
    #pragma warning(suppress: 28118)
    WdfWaitLockAcquire(DevExt->ClassInitLock, NULL);
    if (DevExt->ClassHandle != NULL) {
        DEVICE_OBJECT* DeviceObject = WdfDeviceWdmGetDeviceObject(Device);
        Status = BatteryClassSystemControl(DevExt->ClassHandle,
                                           &DevExt->WmiLibContext,
                                           DeviceObject,
                                           Irp,
                                           &Disposition);
    }

    WdfWaitLockRelease(DevExt->ClassInitLock);
    switch (Disposition) {
    case IrpProcessed:
        break;

    case IrpNotCompleted:
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        break;

    case IrpForward:
    case IrpNotWmi:
    default:
        IoSkipCurrentIrpStackLocation(Irp);
        Status = WdfDeviceWdmDispatchPreprocessedIrp(Device, Irp);
        break;
    }

    DebugExitStatus(Status);
    return Status;
}
