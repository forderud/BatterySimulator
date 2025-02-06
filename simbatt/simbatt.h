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


/** Print debugger message. 
Arguments:
  Level - DPFLTR_ERROR_LEVEL   maps to Kd_IHVDRIVER_Mask 0x1
          DPFLTR_WARNING_LEVEL maps to Kd_IHVDRIVER_Mask 0x2
          DPFLTR_TRACE_LEVEL   maps to Kd_IHVDRIVER_Mask 0x4
          DPFLTR_INFO_LEVEL    maps to Kd_IHVDRIVER_Mask 0x8
  Format - Message in varible argument format.
*/
_IRQL_requires_same_
inline void BattPrint(ULONG Level, PCSTR Format, ...) {
    va_list Arglist;
    va_start(Arglist, Format);
    vDbgPrintEx(DPFLTR_IHVDRIVER_ID, Level, Format, Arglist);
}

#if defined(DEBUGPRINT)
    #define DebugPrint(_Level, _Msg, ...) \
        BattPrint(_Level, _Msg, __VA_ARGS__)

    #define DebugEnter() \
        DebugPrint(DPFLTR_TRACE_LEVEL, "Entering " __FUNCTION__ "\n")

    #define DebugExit() \
        DebugPrint(DPFLTR_TRACE_LEVEL, "Leaving " __FUNCTION__ "\n")

    #define DebugExitStatus(_status_) \
        DebugPrint(DPFLTR_TRACE_LEVEL, \
                   "Leaving " __FUNCTION__ ": Status=0x%x\n", \
                   _status_)
#else
    #define DebugPrint(l, m, ...)
    #define DebugEnter()
    #define DebugExit()
    #define DebugExitStatus(_status_)
#endif

//--------------------------------------------------------------------- Literals

#define POOL_TAG                    'StaB'

//------------------------------------------------------------------ Definitions

struct BATT_GLOBAL_DATA {
    UNICODE_STRING                  RegistryPath;
};

struct BATT_STATE {
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

struct BATT_FDO_DATA {
    // Battery class registration
    void*                           ClassHandle;
    WDFWAITLOCK                     ClassInitLock;
    WMILIB_CONTEXT                  WmiLibContext;

    // Battery state
    WDFWAITLOCK                     StateLock;
    ULONG                           BatteryTag;
    BATT_STATE                   State;
};

//------------------------------------------------------ WDF Context Declaration

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BATT_GLOBAL_DATA, GetGlobalData);
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BATT_FDO_DATA, GetDeviceExtension);

//----------------------------------------------------- Prototypes (miniclass.cpp)

_IRQL_requires_same_
void InitializeBatteryState (_In_ WDFDEVICE Device);

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL BattIoDeviceControl;
BCLASS_QUERY_TAG_CALLBACK QueryTag;
BCLASS_QUERY_INFORMATION_CALLBACK QueryInformation;
BCLASS_SET_INFORMATION_CALLBACK SetInformation;
BCLASS_QUERY_STATUS_CALLBACK QueryStatus;
BCLASS_SET_STATUS_NOTIFY_CALLBACK SetStatusNotify;
BCLASS_DISABLE_STATUS_NOTIFY_CALLBACK DisableStatusNotify;
