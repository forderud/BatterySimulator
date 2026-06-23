#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>
#include <dbt.h>

// This GUID is for all USB serial host PnP drivers, but you can replace it 
// with any valid device class guid.
GUID WceusbshGUID = { 0x25dbce51, 0x6c8f, 0x4a72, 0x8a,0x6d,0xb5,0x4c,0x2b,0x4f,0xc8,0x35 };

void ErrorHandler(LPCTSTR lpszFunction) {
    DWORD dw = GetLastError();

    wprintf(L"ERROR: %s (err %u)\n", lpszFunction, dw);
}


INT_PTR WINAPI WinProcCallback(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
// Routine Description:
//     Simple Windows callback for handling messages.
//     This is where all the work is done because the example
//     is using a window to process messages. This logic would be handled 
//     differently if registering a service instead of a window.

// Parameters:
//     hWnd - the window handle being registered for events.

//     message - the message being interpreted.

//     wParam and lParam - extended information provided to this
//          callback by the message sender.

//     For more information regarding these parameters and return value,
//     see the documentation for WNDCLASSEX and CreateWindowEx.
{
    LRESULT lRet = 1;

    switch (message)
    {
    case WM_DEVICECHANGE:
    {
        // This is the actual message from the interface via Windows messaging.
        // This code includes some additional decoding for this particular device type
        // and some common validation checks.
        //
        // Note that not all devices utilize these optional parameters in the same
        // way. Refer to the extended information for your particular device type 
        // specified by your GUID.
        auto b = (DEV_BROADCAST_DEVICEINTERFACE_W*)lParam;

        switch (wParam) {
        case DBT_DEVICEARRIVAL:
        {
            WCHAR guid_str[39];
            StringFromGUID2(b->dbcc_classguid, guid_str, 39);
            wprintf(L"Message: DBT_DEVICEARRIVAL (class: %s)\n", guid_str);
            break;
        }
        case DBT_DEVICEREMOVECOMPLETE:
            wprintf(L"Message: DBT_DEVICEREMOVECOMPLETE\n");
            break;
        case DBT_DEVNODES_CHANGED:
            wprintf(L"Message: DBT_DEVNODES_CHANGED\n");
            break;
        default:
            wprintf(L"Message: WM_DEVICECHANGE message received, value %Iu unhandled.\n", wParam);
            break;
        }
        break;
    }
    default:
        // Send all other messages on to the default windows handler.
        lRet = DefWindowProc(hWnd, message, wParam, lParam);
        break;
    }

    return lRet;
}

#define WND_CLASS_NAME TEXT("SampleAppWindowClass")

int wmain (int argc, wchar_t* argv[]) {
    WNDCLASSEXW wndClass{};
    wndClass.cbSize = sizeof(wndClass);
    wndClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wndClass.hInstance = reinterpret_cast<HINSTANCE>(GetModuleHandle(0));
    wndClass.lpfnWndProc = reinterpret_cast<WNDPROC>(WinProcCallback);
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hIcon = LoadIcon(0, IDI_APPLICATION);
    wndClass.hbrBackground = CreateSolidBrush(RGB(192, 192, 192));
    wndClass.hCursor = LoadCursor(0, IDC_ARROW);
    wndClass.lpszClassName = WND_CLASS_NAME;
    wndClass.lpszMenuName = NULL;
    wndClass.hIconSm = wndClass.hIcon;

    if (!RegisterClassExW(&wndClass)) {
        ErrorHandler(L"RegisterClassEx");
        return -1;
    }

    // Main app window
    HWND hWnd = CreateWindowExW(
        WS_EX_CLIENTEDGE | WS_EX_APPWINDOW,
        WND_CLASS_NAME,
        argv[0], // EXE name
        WS_OVERLAPPEDWINDOW, // style
        CW_USEDEFAULT, 0,
        640, 480,
        NULL, NULL,
        nullptr,
        NULL);
    if (hWnd == NULL) {
        ErrorHandler(L"CreateWindowEx: main appwindow hWnd");
        return -1;
    }

    // Subscribe to PnP device notifications
    HDEVNOTIFY hDeviceNotify = nullptr;
    {
        DEV_BROADCAST_DEVICEINTERFACE_W NotificationFilter{};
        NotificationFilter.dbcc_size = sizeof(NotificationFilter);
        NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        NotificationFilter.dbcc_classguid = WceusbshGUID;

        hDeviceNotify = RegisterDeviceNotificationW(
            hWnd,                       // events recipient
            &NotificationFilter,        // type of device
            DEVICE_NOTIFY_WINDOW_HANDLE // type of recipient handle
        );
        if (NULL == hDeviceNotify) {
            ErrorHandler(L"RegisterDeviceNotification");
            return -1;
        }
    }

    // The message pump loops until the window is destroyed.
    {
        // Get all messages for any window that belongs to this thread,
        // without any filtering. Potential optimization could be
        // obtained via use of filter values if desired.
        MSG msg;
        int retVal;
        while ((retVal = GetMessageW(&msg, NULL, 0, 0)) != 0) {
            if (retVal == -1) {
                ErrorHandler(L"GetMessage");
                break;
            } else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    if (!UnregisterDeviceNotification(hDeviceNotify)) {
        ErrorHandler(L"UnregisterDeviceNotification");
    }

    return 1;
}
