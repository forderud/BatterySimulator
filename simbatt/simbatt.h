#pragma once
#include <wdm.h>
#include <wdf.h>
extern "C" {
#include <batclass.h>
}
#include <wmistr.h>
#include <wmilib.h>
#include <ntstrsafe.h>

//------------------------------------------------------------- Debug Facilities
#define SIMBATT_ERROR               DPFLTR_ERROR_LEVEL      // ed Kd_IHVDRIVER_Mask 0x1
#define SIMBATT_WARN                DPFLTR_WARNING_LEVEL    // ed Kd_IHVDRIVER_Mask 0x2
#define SIMBATT_TRACE               DPFLTR_TRACE_LEVEL      // ed Kd_IHVDRIVER_Mask 0x4
#define SIMBATT_INFO                DPFLTR_INFO_LEVEL       // ed Kd_IHVDRIVER_Mask 0x8

#if defined(DEBUGPRINT)
    #define DebugPrint(_Level, _Msg, ...) \
        SimBattPrint(_Level, _Msg, __VA_ARGS__)

    #define DebugEnter() \
        DebugPrint(SIMBATT_TRACE, "Entering " __FUNCTION__ "\n")

    #define DebugExit() \
        DebugPrint(SIMBATT_TRACE, "Leaving " __FUNCTION__ "\n")

    #define DebugExitStatus(_status_) \
        DebugPrint(SIMBATT_TRACE, \
                   "Leaving " __FUNCTION__ ": Status=0x%x\n", \
                   _status_)
#else
    #define DebugPrint(l, m, ...)
    #define DebugEnter()
    #define DebugExit()
    #define DebugExitStatus(_status_)
#endif

//--------------------------------------------------------------------- Literals

#define SIMBATT_TAG                 'StaB'
#define SIMBATT_STATE_REG_NAME      L"SimbattState"

//------------------------------------------------------------------ Definitions

struct SIMBATT_GLOBAL_DATA {
    UNICODE_STRING                  RegistryPath;
};

struct SIMBATT_STATE {
    BATTERY_MANUFACTURE_DATE        ManufactureDate;
    BATTERY_INFORMATION             BatteryInfo;
    BATTERY_STATUS                  BatteryStatus;
    ULONG                           GranularityCount;
    BATTERY_REPORTING_SCALE         GranularityScale[4];
    ULONG                           EstimatedTime;
    ULONG                           Temperature;
    WCHAR                           DeviceName[MAX_BATTERY_STRING_SIZE];
    WCHAR                           ManufacturerName[MAX_BATTERY_STRING_SIZE];
    WCHAR                           SerialNumber[MAX_BATTERY_STRING_SIZE];
    WCHAR                           UniqueId[MAX_BATTERY_STRING_SIZE];
};

struct SIMBATT_FDO_DATA {
    // Battery class registration
    void*                           ClassHandle;
    WDFWAITLOCK                     ClassInitLock;
    WMILIB_CONTEXT                  WmiLibContext;

    // Battery state
    WDFWAITLOCK                     StateLock;
    ULONG                           BatteryTag;
    SIMBATT_STATE                   State;
};

//------------------------------------------------------ WDF Context Declaration

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(SIMBATT_GLOBAL_DATA, GetGlobalData);
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(SIMBATT_FDO_DATA, GetDeviceExtension);

//----------------------------------------------------- Prototypes (miniclass.cpp)

_IRQL_requires_same_
void InitializeBatteryState (_In_ WDFDEVICE Device);

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL SimBattIoDeviceControl;
BCLASS_QUERY_TAG_CALLBACK QueryTag;
BCLASS_QUERY_INFORMATION_CALLBACK QueryInformation;
BCLASS_SET_INFORMATION_CALLBACK SimBattSetInformation;
BCLASS_QUERY_STATUS_CALLBACK QueryStatus;
BCLASS_SET_STATUS_NOTIFY_CALLBACK SetStatusNotify;
BCLASS_DISABLE_STATUS_NOTIFY_CALLBACK SimBattDisableStatusNotify;

_IRQL_requires_same_
void SimBattPrint (_In_ ULONG Level, _In_ PCSTR Format, ...);
