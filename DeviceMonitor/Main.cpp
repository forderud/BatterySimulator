#include <windows.h>
#include <Cfgmgr32.h>
#include <dbt.h> // for DEV_BROADCAST_HDR
#include <cstdio>
#include <cassert>
#include <string>

#pragma comment(lib, "Cfgmgr32.lib") // for CM_Register_Notification


const wchar_t* ActionToStr (CM_NOTIFY_ACTION action) {
    switch (action) {
    /* Filter type: CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE */
    case CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL: return L"Device interface ARRIVAL";
    case CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL: return L"Device interface REMOVAL";
    /* Filter type: CM_NOTIFY_FILTER_TYPE_DEVICEHANDLE */
    case CM_NOTIFY_ACTION_DEVICEQUERYREMOVE: return L"Device query REMOVE";
    case CM_NOTIFY_ACTION_DEVICEQUERYREMOVEFAILED: return L"Device query REMOVEFAILED";
    case CM_NOTIFY_ACTION_DEVICEREMOVEPENDING: return L"Device REMOVE PENDING";
    case CM_NOTIFY_ACTION_DEVICEREMOVECOMPLETE: return L"Device REMOVE COMPLETE";
    case CM_NOTIFY_ACTION_DEVICECUSTOMEVENT: return L"Device CUSTOMEVENT";
    /* Filter type: CM_NOTIFY_FILTER_TYPE_DEVICEINSTANCE */
    case CM_NOTIFY_ACTION_DEVICEINSTANCEENUMERATED: return L"Device instance ENUMERATED";
    case CM_NOTIFY_ACTION_DEVICEINSTANCESTARTED: return L"Device instance STARTED";
    case CM_NOTIFY_ACTION_DEVICEINSTANCEREMOVED: return L"Device instance REMOVED";
    default:
        assert(false && "Unknown CM_NOTIFY_ACTION");
        return L"<unknown CM_NOTIFY_ACTION>";
    }
}


DWORD PnP_callback (
    _In_ HCMNOTIFICATION       /*hNotify*/,
    _In_opt_ void*             /*Context*/,
    _In_ CM_NOTIFY_ACTION      Action,
    _In_reads_bytes_(EventDataSize) CM_NOTIFY_EVENT_DATA* EventData,
    _In_ DWORD                 EventDataSize
) {
    if (EventData->FilterType == CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE) {
        assert(EventDataSize >= sizeof(EventData->u.DeviceInterface)); EventDataSize;
        auto data = &EventData->u.DeviceInterface;
        
        wchar_t guid_str[39]{};
        StringFromGUID2(data->ClassGuid, guid_str, 39);
        wprintf(L"%s:\n", ActionToStr(Action));
        wprintf(L"  ClassGuid=%s\n", guid_str);
        wprintf(L"  SymbolicLink=%s\n", data->SymbolicLink);
    } else if (EventData->FilterType == CM_NOTIFY_FILTER_TYPE_DEVICEHANDLE) {
        assert(EventDataSize >= sizeof(EventData->u.DeviceHandle)); EventDataSize;
        auto data = &EventData->u.DeviceHandle;

        wchar_t guid_str[39]{};
        StringFromGUID2(data->EventGuid, guid_str, 39);
        
        wprintf(L"%s:\n", ActionToStr(Action));
        wprintf(L"  EventGuid=%s\n", guid_str);
        wprintf(L"  NameOffset=%u\n", data->NameOffset);
        wprintf(L"  DataSize=%u\n", data->DataSize);
        data->Data;
    } else if (EventData->FilterType == CM_NOTIFY_FILTER_TYPE_DEVICEINSTANCE) {
        assert(EventDataSize >= sizeof(EventData->u.DeviceInstance)); EventDataSize;
        auto data = &EventData->u.DeviceInstance;

        wprintf(L"%s:\n", ActionToStr(Action));
        wprintf(L"  InstanceId=%s\n", data->InstanceId);
    } else {
        assert(false && "Unknown CM_NOTIFY_EVENT_DATA::FilterType");
    }

    return ERROR_SUCCESS;
}

enum class EnumType {
    Devices,
    Interfaces,
};


int wmain (int argc, wchar_t* argv[]) {
    const wchar_t usage_helpstring[] = L"USAGE DeviceMonitor.exe [--devices | --interfaces]\n";

    // Parse command-line arguments
    if (argc < 2) {
        wprintf(usage_helpstring);
        return 1;
    }

    EnumType enumerator = EnumType::Devices;
    for (int idx = 1; idx < argc; idx++) {
        std::wstring arg = argv[idx];
        if (arg == L"--devices") {
            enumerator = EnumType::Devices;
        } else if (arg == L"--interfaces") {
            enumerator = EnumType::Interfaces;
        } else {
            wprintf(usage_helpstring);
            return 1;
        }
    }

    wprintf(L"PnP event monitor started. Press Ctrl+C to exit.\n");
    wprintf(L"\n");

    HCMNOTIFICATION hNotify = 0;
    {
        // subscribe to PnP events
        CM_NOTIFY_FILTER filter{};
        filter.cbSize = sizeof(filter);

        if (enumerator == EnumType::Devices) {
            wprintf(L"Listening to PnP events for all devices...\n");
            filter.Flags = CM_NOTIFY_FILTER_FLAG_ALL_DEVICE_INSTANCES;
            filter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINSTANCE;
            filter.u.DeviceInstance.InstanceId[0] = '\0'; // empty string
        } else {
            wprintf(L"Listening to PnP events for all device interface classes...\n");
            filter.Flags = CM_NOTIFY_FILTER_FLAG_ALL_INTERFACE_CLASSES;
            filter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
            filter.u.DeviceInterface.ClassGuid = {};
        }

        CONFIGRET ret = CM_Register_Notification(&filter, nullptr, PnP_callback, &hNotify);
        assert(ret == CR_SUCCESS);
    }

    Sleep(INFINITE);

    CONFIGRET ret = CM_Unregister_Notification(hNotify);
    assert(ret == CR_SUCCESS); ret;

    return 1;
}
