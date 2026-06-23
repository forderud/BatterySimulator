#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>
#include <dbt.h>

// This GUID is for all USB serial host PnP drivers, but you can replace it 
// with any valid device class guid.
GUID WceusbshGUID = { 0x25dbce51, 0x6c8f, 0x4a72, 0x8a,0x6d,0xb5,0x4c,0x2b,0x4f,0xc8,0x35 };

void OutputMessage(
    HWND hOutWnd,
    WPARAM wParam,
    LPARAM lParam)
// Routine Description:
//     Support routine.
//     Send text to the output window, scrolling if necessary.

// Parameters:
//     hOutWnd - Handle to the output window.
//     wParam  - Standard windows message code, not used.
//     lParam  - String message to send to the window.

// Return Value:
//     None

// Note:
//     This routine assumes the output window is an edit control
//     with vertical scrolling enabled.

//     This routine has no error-checking.
{
    LRESULT   lResult;
    LONG      bufferLen;
    LONG      numLines;
    LONG      firstVis;

    // Make writable and turn off redraw.
    lResult = SendMessage(hOutWnd, EM_SETREADONLY, FALSE, 0L);
    lResult = SendMessage(hOutWnd, WM_SETREDRAW, FALSE, 0L);

    // Obtain current text length in the window.
    bufferLen = SendMessage(hOutWnd, WM_GETTEXTLENGTH, 0, 0L);
    numLines = SendMessage(hOutWnd, EM_GETLINECOUNT, 0, 0L);
    firstVis = SendMessage(hOutWnd, EM_GETFIRSTVISIBLELINE, 0, 0L);
    lResult = SendMessage(hOutWnd, EM_SETSEL, bufferLen, bufferLen);

    // Write the new text.
    lResult = SendMessage(hOutWnd, EM_REPLACESEL, 0, lParam);

    // See whether scrolling is necessary.
    if (numLines > (firstVis + 1)) {
        int        lineLen = 0;
        int        lineCount = 0;
        int        charPos;

        // Find the last nonblank line.
        numLines--;
        while (!lineLen)
        {
            charPos = SendMessage(
                hOutWnd, EM_LINEINDEX, (WPARAM)numLines, 0L);
            lineLen = SendMessage(
                hOutWnd, EM_LINELENGTH, charPos, 0L);
            if (!lineLen)
                numLines--;
        }
        // Prevent negative value finding min.
        lineCount = numLines - firstVis;
        lineCount = (lineCount >= 0) ? lineCount : 0;

        // Scroll the window.
        lResult = SendMessage(
            hOutWnd, EM_LINESCROLL, 0, (LPARAM)lineCount);
    }

    // Done, make read-only and allow redraw.
    lResult = SendMessage(hOutWnd, WM_SETREDRAW, TRUE, 0L);
    lResult = SendMessage(hOutWnd, EM_SETREADONLY, TRUE, 0L);
}

void ErrorHandler(LPCTSTR lpszFunction) {
    DWORD dw = GetLastError();

    wprintf(L"ERROR: %s (err %u)\n", lpszFunction, dw);
}

BOOL DoRegisterDeviceInterfaceToHwnd(
    IN GUID InterfaceClassGuid,
    IN HWND hWnd,
    OUT HDEVNOTIFY* hDeviceNotify
)
// Routine Description:
//     Registers an HWND for notification of changes in the device interfaces
//     for the specified interface class GUID. 

// Parameters:
//     InterfaceClassGuid - The interface class GUID for the device 
//         interfaces. 

//     hWnd - Window handle to receive notifications.

//     hDeviceNotify - Receives the device notification handle. On failure, 
//         this value is NULL.

// Return Value:
//     If the function succeeds, the return value is TRUE.
//     If the function fails, the return value is FALSE.

// Note:
//     RegisterDeviceNotification also allows a service handle be used,
//     so a similar wrapper function to this one supporting that scenario
//     could be made from this template.
{
    DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;

    ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
    NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    NotificationFilter.dbcc_classguid = InterfaceClassGuid;

    *hDeviceNotify = RegisterDeviceNotification(
        hWnd,                       // events recipient
        &NotificationFilter,        // type of device
        DEVICE_NOTIFY_WINDOW_HANDLE // type of recipient handle
    );
    if (NULL == *hDeviceNotify) {
        ErrorHandler(L"RegisterDeviceNotification");
        return FALSE;
    }

    return TRUE;
}

void MessagePump(HWND hWnd)
// Routine Description:
//     Simple main thread message pump.
//

// Parameters:
//     hWnd - handle to the window whose messages are being dispatched

// Return Value:
//     None.
{
    MSG msg;
    int retVal;

    // Get all messages for any window that belongs to this thread,
    // without any filtering. Potential optimization could be
    // obtained via use of filter values if desired.

    while ((retVal = GetMessage(&msg, NULL, 0, 0)) != 0) {
        if (retVal == -1) {
            ErrorHandler(L"GetMessage");
            break;
        } else {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}

INT_PTR WINAPI WinProcCallback(
    HWND hWnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
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

        // Output some messages to the window.
        switch (wParam) {
        case DBT_DEVICEARRIVAL:
            wprintf(L"Message: DBT_DEVICEARRIVAL\n");
            break;
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
    }
    break;
    default:
        // Send all other messages on to the default windows handler.
        lRet = DefWindowProc(hWnd, message, wParam, lParam);
        break;
    }

    return lRet;
}

#define WND_CLASS_NAME TEXT("SampleAppWindowClass")

int wmain (int argc, wchar_t* argv[]) {
    WNDCLASSEX wndClass{};
    wndClass.cbSize = sizeof(WNDCLASSEX);
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

    if (!RegisterClassEx(&wndClass)) {
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
    HDEVNOTIFY hDeviceNotify;
    if (!DoRegisterDeviceInterfaceToHwnd(
        WceusbshGUID,
        hWnd,
        &hDeviceNotify)) {
        // Terminate on failure.
        ErrorHandler(L"DoRegisterDeviceInterfaceToHwnd");
        ExitProcess(1);
    }


    // The message pump loops until the window is destroyed.
    MessagePump(hWnd);

    if (!UnregisterDeviceNotification(hDeviceNotify)) {
        ErrorHandler(L"UnregisterDeviceNotification");
    }

    return 1;
}
