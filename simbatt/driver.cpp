#include "device.hpp"

extern "C" DRIVER_INITIALIZE DriverEntry;

_Use_decl_annotations_
NTSTATUS DriverEntry(DRIVER_OBJECT* DriverObject, UNICODE_STRING* RegistryPath)
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
    DriverConfig.DriverPoolTag = POOL_TAG;

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
    WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&DriverAttributes, DRIVER_CONTEXT);

    DriverAttributes.ExecutionLevel = WdfExecutionLevelPassive;

    // Create the driver object
    NTSTATUS Status = WdfDriverCreate(DriverObject, RegistryPath, &DriverAttributes, &DriverConfig, WDF_NO_HANDLE);
    if (!NT_SUCCESS(Status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("WdfDriverCreate() Failed. Status 0x%x"), Status);
        goto DriverEntryEnd;
    }

    DRIVER_CONTEXT* GlobalData = WdfObjectGet_DRIVER_CONTEXT(WdfGetDriver());
    GlobalData->RegistryPath.MaximumLength = RegistryPath->Length + sizeof(UNICODE_NULL);
    GlobalData->RegistryPath.Length = RegistryPath->Length;
    GlobalData->RegistryPath.Buffer = WdfDriverGetRegistryPath(WdfGetDriver());

DriverEntryEnd:
    DebugExitStatus(Status);
    return Status;
}
