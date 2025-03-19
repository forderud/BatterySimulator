#pragma once
#include "device.hpp"
#include <wmilib.h>
extern "C" {
#include <batclass.h>
}


struct BATT_STATE {
    BATTERY_MANUFACTURE_DATE ManufactureDate;
    BATTERY_INFORMATION      BatteryInfo;
    BATTERY_STATUS           BatteryStatus;
    ULONG                    GranularityCount;
    BATTERY_REPORTING_SCALE  GranularityScale[4];
    ULONG                    EstimatedTime; // seconds
    ULONG                    Temperature;   // 10ths of a degree Kelvin
    WCHAR                    DeviceName[MAX_BATTERY_STRING_SIZE];
    WCHAR                    ManufacturerName[MAX_BATTERY_STRING_SIZE];
    WCHAR                    SerialNumber[MAX_BATTERY_STRING_SIZE];
    WCHAR                    UniqueId[MAX_BATTERY_STRING_SIZE];
};

struct DEVICE_CONTEXT {
    // Battery class registration
    void*                    ClassHandle;
    WDFWAITLOCK              ClassInitLock;
    WMILIB_CONTEXT           WmiLibContext;

    // Battery state
    WDFWAITLOCK              StateLock;
    ULONG                    BatteryTag;
    BATT_STATE               State;
};

WDF_DECLARE_CONTEXT_TYPE(DEVICE_CONTEXT);


NTSTATUS InitializeBatteryClass(_In_ WDFDEVICE Device);
NTSTATUS UnloadBatteryClass(_In_ WDFDEVICE Device);

_IRQL_requires_same_
void InitializeBatteryState(_In_ WDFDEVICE Device);

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL BattIoDeviceControl;

EVT_WDFDEVICE_WDM_IRP_PREPROCESS BattWdmIrpPreprocessDeviceControl;
EVT_WDFDEVICE_WDM_IRP_PREPROCESS BattWdmIrpPreprocessSystemControl;
