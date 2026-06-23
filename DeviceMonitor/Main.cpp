#include <windows.h>
#include <Cfgmgr32.h>
#include <dbt.h> // for DEV_BROADCAST_HDR
#include <cstdio>
#include <cassert>

#pragma comment(lib, "Cfgmgr32.lib") // for CM_Register_Notification


const wchar_t* ActionStr(CM_NOTIFY_ACTION action) {
    switch (action) {
    /* Filter type: CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE */
    case CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL: return L"DEVICE INTERFACE ARRIVAL";
    case CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL: return L"DEVICE INTERFACE REMOVAL";
    /* Filter type: CM_NOTIFY_FILTER_TYPE_DEVICEHANDLE */
    case CM_NOTIFY_ACTION_DEVICEQUERYREMOVE: return L"DEVICE QUERY REMOVE";
    case CM_NOTIFY_ACTION_DEVICEQUERYREMOVEFAILED: return L"DEVICE QUERY REMOVEFAILED";
    case CM_NOTIFY_ACTION_DEVICEREMOVEPENDING: return L"DEVICE REMOVE PENDING";
    case CM_NOTIFY_ACTION_DEVICEREMOVECOMPLETE: return L"DEVICE REMOVE COMPLETE";
    case CM_NOTIFY_ACTION_DEVICECUSTOMEVENT: return L"DEVICE CUSTOMEVENT";
    /* Filter type: CM_NOTIFY_FILTER_TYPE_DEVICEINSTANCE */
    case CM_NOTIFY_ACTION_DEVICEINSTANCEENUMERATED: return L"DEVICE INSTANCE ENUMERATED";
    case CM_NOTIFY_ACTION_DEVICEINSTANCESTARTED: return L"DEVICE INSTANCE STARTED";
    case CM_NOTIFY_ACTION_DEVICEINSTANCEREMOVED: return L"DEVICE INSTANCE REMOVED";
    default:
        abort(); // unknown
    }
}


DWORD PnP_callback (
    _In_ HCMNOTIFICATION       /*hNotify*/,
    _In_opt_ PVOID             /*Context*/,
    _In_ CM_NOTIFY_ACTION      Action,
    _In_reads_bytes_(EventDataSize) PCM_NOTIFY_EVENT_DATA EventData,
    _In_ DWORD                 EventDataSize
) {
    if (EventData->FilterType == CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE) {
        assert(EventDataSize >= sizeof(EventData->u.DeviceInterface));
        auto data = &EventData->u.DeviceInterface;
        
        wchar_t guid_str[39]{};
        StringFromGUID2(data->ClassGuid, guid_str, 39);
        wprintf(L"%s\n", ActionStr(Action));
        wprintf(L"  ClassGuid=%s\n", guid_str);
        wprintf(L"  SymbolicLink=%s\n", data->SymbolicLink);
    } else if (EventData->FilterType == CM_NOTIFY_FILTER_TYPE_DEVICEHANDLE) {
        assert(EventDataSize >= sizeof(EventData->u.DeviceHandle));
        auto data = &EventData->u.DeviceHandle;

        wchar_t guid_str[39]{};
        StringFromGUID2(data->EventGuid, guid_str, 39);
        
        wprintf(L"%s\n", ActionStr(Action));
        wprintf(L"  EventGuid=%s\n", guid_str);
        wprintf(L"  NameOffset=%u\n", data->NameOffset);
        wprintf(L"  DataSize=%u\n", data->DataSize);
    } else if (EventData->FilterType == CM_NOTIFY_FILTER_TYPE_DEVICEINSTANCE) {
        assert(EventDataSize >= sizeof(EventData->u.DeviceInstance));
        auto data = &EventData->u.DeviceInstance;

        wprintf(L"%s\n", ActionStr(Action));
        wprintf(L"  InstanceId=%s\n", data->InstanceId);
    }

    return 0;
}


int wmain (int /*argc*/, wchar_t* argv[]) {
    CM_NOTIFY_FILTER filter{};
    filter.cbSize = sizeof(filter);
    filter.Flags = CM_NOTIFY_FILTER_FLAG_ALL_INTERFACE_CLASSES;
    filter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
    filter.u.DeviceInterface.ClassGuid = {};

    void* context = nullptr;
    HCMNOTIFICATION hNotify = 0;
    CONFIGRET ret = CM_Register_Notification(&filter, context, PnP_callback, &hNotify);
    assert(ret == CR_SUCCESS);

    Sleep(30 * 1000); // wait for 30sec

    ret = CM_Unregister_Notification(hNotify);
    assert(ret == CR_SUCCESS);

    return 1;
}
