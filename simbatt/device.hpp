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

// DML macro for white text (wbg) on red background (changed)
#define DML_ERR(msg) ("<?dml?><col fg=\"wbg\" bg=\"changed\">"##msg##"</col>\n")

#if DBG
/** Print debugger message.
Arguments:
  Level - DPFLTR_ERROR_LEVEL   maps to Kd_IHVDRIVER_Mask 0x1
          DPFLTR_WARNING_LEVEL maps to Kd_IHVDRIVER_Mask 0x2
          DPFLTR_TRACE_LEVEL   maps to Kd_IHVDRIVER_Mask 0x4
          DPFLTR_INFO_LEVEL    maps to Kd_IHVDRIVER_Mask 0x8
  Format - Message in varible argument format.
*/
_IRQL_requires_same_
inline void DebugPrint(ULONG Level, PCSTR Format, ...) {
    va_list Arglist;
    va_start(Arglist, Format);
    vDbgPrintEx(DPFLTR_IHVDRIVER_ID, Level, Format, Arglist);
}

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

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BATT_GLOBAL_DATA, GetGlobalData);


EVT_WDF_DRIVER_DEVICE_ADD BattDriverDeviceAdd;
