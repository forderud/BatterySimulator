#include "wmi.hpp"
#include "simbattdriverif.h"


void RegisterWMI(WDFDEVICE Device)
{
    BATT_FDO_DATA* DevExt = GetDeviceExtension(Device);

    // Register the device as a WMI data provider. This is done using WDM
    // methods because the battery class driver uses WDM methods to complete
    // WMI requests.
    DevExt->WmiLibContext.GuidCount = 0;
    DevExt->WmiLibContext.GuidList = NULL;
    DevExt->WmiLibContext.QueryWmiRegInfo = QueryWmiRegInfo;
    DevExt->WmiLibContext.QueryWmiDataBlock = QueryWmiDataBlock;
    DevExt->WmiLibContext.SetWmiDataBlock = NULL;
    DevExt->WmiLibContext.SetWmiDataItem = NULL;
    DevExt->WmiLibContext.ExecuteWmiMethod = NULL;
    DevExt->WmiLibContext.WmiFunctionControl = NULL;

    DEVICE_OBJECT* DeviceObject = WdfDeviceWdmGetDeviceObject(Device);
    NTSTATUS Status = IoWMIRegistrationControl(DeviceObject, WMIREG_ACTION_REGISTER);

    // Failure to register with WMI is nonfatal.
    if (!NT_SUCCESS(Status)) {
        DebugPrint(DPFLTR_WARNING_LEVEL, DML_ERR("IoWMIRegistrationControl() Failed. Status 0x%x"), Status);
    }
}

void UnregisterWMI(WDFDEVICE Device)
{
    DEVICE_OBJECT* DeviceObject = WdfDeviceWdmGetDeviceObject(Device);

    NTSTATUS Status = IoWMIRegistrationControl(DeviceObject, WMIREG_ACTION_DEREGISTER);

    // Failure to unregister with WMI is nonfatal.
    if (!NT_SUCCESS(Status)) {
        DebugPrint(DPFLTR_WARNING_LEVEL, DML_ERR("IoWMIRegistrationControl() Failed. Status 0x%x"), Status);
    }
}


_Use_decl_annotations_
NTSTATUS QueryWmiRegInfo(
    DEVICE_OBJECT* DeviceObject,
    ULONG* RegFlags,
    UNICODE_STRING* InstanceName,
    UNICODE_STRING** RegistryPath,
    UNICODE_STRING* MofResourceName,
    DEVICE_OBJECT** Pdo)
    /*++
    Routine Description:
        This routine is a callback into the driver to retrieve the list of
        guids or data blocks that the driver wants to register with WMI. This
        routine may not pend or block. Driver should NOT call
        WmiCompleteRequest.

    Arguments:
        DeviceObject - Supplies the device whose data block is being queried.

        RegFlags - Supplies a pointer to return a set of flags that describe the
            guids being registered for this device. If the device wants enable and
            disable collection callbacks before receiving queries for the registered
            guids then it should return the WMIREG_FLAG_EXPENSIVE flag. Also the
            returned flags may specify WMIREG_FLAG_INSTANCE_PDO in which case
            the instance name is determined from the PDO associated with the
            device object. Note that the PDO must have an associated devnode. If
            WMIREG_FLAG_INSTANCE_PDO is not set then Name must return a unique
            name for the device.

        InstanceName - Supplies a pointer to return the instance name for the guids
            if WMIREG_FLAG_INSTANCE_PDO is not set in the returned *RegFlags. The
            caller will call ExFreePool with the buffer returned.

        RegistryPath - Supplies a pointer to return the registry path of the driver.

        MofResourceName - Supplies a pointer to return the name of the MOF resource
            attached to the binary file. If the driver does not have a mof resource
            attached then this can be returned as NULL.

        Pdo - Supplies a pointer to return the device object for the PDO associated
            with this device if the WMIREG_FLAG_INSTANCE_PDO flag is returned in
            *RegFlags.
    --*/
{
    UNREFERENCED_PARAMETER(MofResourceName);
    UNREFERENCED_PARAMETER(InstanceName);

    DebugEnter();

    WDFDEVICE Device = WdfWdmDeviceGetWdfDeviceHandle(DeviceObject);
    BATT_GLOBAL_DATA* GlobalData = GetGlobalData(WdfGetDriver());
    *RegFlags = WMIREG_FLAG_INSTANCE_PDO;
    *RegistryPath = &GlobalData->RegistryPath;
    *Pdo = WdfDeviceWdmGetPhysicalDevice(Device);
    NTSTATUS Status = STATUS_SUCCESS;
    DebugExitStatus(Status);
    return Status;
}


_Use_decl_annotations_
NTSTATUS QueryWmiDataBlock(
    DEVICE_OBJECT* DeviceObject,
    IRP* Irp,
    ULONG GuidIndex,
    ULONG InstanceIndex,
    ULONG InstanceCount,
    ULONG* InstanceLengthArray,
    ULONG BufferAvail,
    UCHAR* Buffer)
    /*++
    Routine Description:
        This routine is a callback into the driver to query for the contents of
        a data block. When the driver has finished filling the data block it
        must call WmiCompleteRequest to complete the irp. The driver can
        return STATUS_PENDING if the irp cannot be completed immediately.

    Arguments:
        DeviceObject - Supplies the device whose data block is being queried.

        Irp - Supplies the Irp that makes this request.

        GuidIndex - Supplies the index into the list of guids provided when the
            device registered.

        InstanceIndex - Supplies the index that denotes which instance of the data
            block is being queried.

        InstanceCount - Supplies the number of instances expected to be returned for
            the data block.

        InstanceLengthArray - Supplies a pointer to an array of ULONG that returns
            the lengths of each instance of the data block. If this is NULL then
            there was not enough space in the output buffer to fulfill the request
            so the irp should be completed with the buffer needed.

        BufferAvail - Supplies the maximum size available to write the data
            block.

        Buffer - Supplies a pointer to a buffer to return the data block.
    --*/
{
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(InstanceIndex);
    UNREFERENCED_PARAMETER(InstanceCount);

    DebugEnter();
    ASSERT((InstanceIndex == 0) && (InstanceCount == 1));

    if (InstanceLengthArray == NULL) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto QueryWmiDataBlockEnd;
    }

    WDFDEVICE Device = WdfWdmDeviceGetWdfDeviceHandle(DeviceObject);
    BATT_FDO_DATA* DevExt = GetDeviceExtension(Device);

    // The class driver guarantees that all outstanding IO requests will be
    // completed before it finishes unregistering. As a result, the class
    // initialization lock does not need to be acquired in this callback, since
    // it is called during class driver processing of a WMI IRP.
    Status = BatteryClassQueryWmiDataBlock(DevExt->ClassHandle,
        DeviceObject,
        Irp,
        GuidIndex,
        InstanceLengthArray,
        BufferAvail,
        Buffer);

    if (Status == STATUS_WMI_GUID_NOT_FOUND) {
        Status = WmiCompleteRequest(DeviceObject,
            Irp,
            STATUS_WMI_GUID_NOT_FOUND,
            0,
            IO_NO_INCREMENT);
    }

QueryWmiDataBlockEnd:
    DebugExitStatus(Status);
    return Status;
}
