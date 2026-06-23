#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>
#include <dbt.h>
#include <cassert>

// USB serial host GUID
GUID WCE_USB_SH_GUID = { 0x25dbce51, 0x6c8f, 0x4a72, 0x8a,0x6d,0xb5,0x4c,0x2b,0x4f,0xc8,0x35 };

void LogError (const wchar_t* message) {
    DWORD err = GetLastError();
    wprintf(L"ERROR: %s (err %u)\n", message, err);
}


INT_PTR WINAPI WinProcCallback(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_DEVICECHANGE:
    {
        auto hdr = (DEV_BROADCAST_HDR*)lParam;

        switch (wParam) {
        case DBT_DEVICEARRIVAL:
        {
            if (hdr->dbch_devicetype == DBT_DEVTYP_PORT) {
                auto hdr_port = (DEV_BROADCAST_PORT_W*)lParam;
                wprintf(L"Message: DBT_DEVICEARRIVAL DBT_DEVTYP_PORT (name: %s)\n", hdr_port->dbcp_name);
            } else if (hdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
                auto hdr_di = (DEV_BROADCAST_DEVICEINTERFACE_W*)lParam;
                wchar_t guid_str[39]{};
                StringFromGUID2(hdr_di->dbcc_classguid, guid_str, 39);
                wprintf(L"Message: DBT_DEVICEARRIVAL DBT_DEVTYP_DEVICEINTERFACE (class: %s, name %s)\n", guid_str, hdr_di->dbcc_name);
            } else {
                wprintf(L"Message: DBT_DEVICEARRIVAL unknown type %Iu\n", wParam);
            }
            break;
        }
        case DBT_DEVICEREMOVECOMPLETE:
            wprintf(L"Message: DBT_DEVICEREMOVECOMPLETE\n");
            break;
        case DBT_DEVNODES_CHANGED:
            //wprintf(L"Message: DBT_DEVNODES_CHANGED\n");
            break;
        default:
            wprintf(L"Message: WM_DEVICECHANGE message received, value %Iu unhandled.\n", wParam);
            break;
        }
        break;
    }
    default:
        // Send all other messages on to the default windows handler.
        return DefWindowProc(hWnd, message, wParam, lParam);
        break;
    }

    return 1;
}


int wmain (int /*argc*/, wchar_t* argv[]) {
    static const wchar_t WND_CLASS_NAME[] = L"DeviceMonitorClass";

    // register custom window class
    WNDCLASSEXW wndClass{};
    wndClass.cbSize = sizeof(wndClass);
    wndClass.lpfnWndProc = WinProcCallback;
    wndClass.hInstance = GetModuleHandleW(nullptr);
    wndClass.lpszClassName = WND_CLASS_NAME;
    if (!RegisterClassExW(&wndClass)) {
        LogError(L"RegisterClassEx");
        return -1;
    }

    // offscreen window for PnP events
    HWND hWnd = CreateWindowExW(
        0, // ext. style
        WND_CLASS_NAME,
        argv[0], // EXE name
        0, // style
        0, 0, 0, 0, // pos & size
        NULL, // parent
        NULL, // menu
        GetModuleHandleW(nullptr), // instance
        NULL); // app. data
    if (hWnd == NULL) {
        LogError(L"CreateWindowEx: main appwindow hWnd");
        return -1;
    }

    // subscribe to PnP device notifications
    HDEVNOTIFY hDeviceNotify = nullptr;
    {
        DEV_BROADCAST_DEVICEINTERFACE_W NotificationFilter{};
        NotificationFilter.dbcc_size = sizeof(NotificationFilter);
        NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE; // or DBT_DEVTYP_HANDLE
        NotificationFilter.dbcc_classguid = {}; // ignored when DEVICE_NOTIFY_ALL_INTERFACE_CLASSES is set

        hDeviceNotify = RegisterDeviceNotificationW(
            hWnd,                       // events recipient
            &NotificationFilter,        // type of device
            DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES // or DEVICE_NOTIFY_SERVICE_HANDLE
        );
        if (NULL == hDeviceNotify) {
            LogError(L"RegisterDeviceNotification");
            return -1;
        }
    }

    // The message pump loops until the window is destroyed.
    {
        // Get all messages for any window that belongs to this thread,
        // without any filtering. Potential optimization could be
        // obtained via use of filter values if desired.
        MSG msg{};
        int retVal = 0;
        while ((retVal = GetMessageW(&msg, NULL, 0, 0)) != 0) {
            if (retVal == -1) {
                LogError(L"GetMessage");
                break;
            } else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    if (!UnregisterDeviceNotification(hDeviceNotify)) {
        LogError(L"UnregisterDeviceNotification");
    }

    return 1;
}
