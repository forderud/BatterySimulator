#include <windows.h>
#include <cassert>
#include <iostream>


/** Process WM_POWERBROADCAST events. */
void ProcessPowerEvent(WPARAM wParam) {
    wprintf(L"Power broadcast message:\n");

    if (wParam == PBT_APMPOWERSTATUSCHANGE) {
        wprintf(L"  Power status change.\n");

        SYSTEM_POWER_STATUS status = {};
        BOOL ok = GetSystemPowerStatus(&status);
        assert(ok); ok;

        // display battery/AC status
        if (status.ACLineStatus == 1)
            wprintf(L"  Connected to AC.\n");
        else
            wprintf(L"  On battery power: %i%%.\n", status.BatteryLifePercent);
    } else if (wParam == PBT_APMSUSPEND) {
        wprintf(L"  Suspending to low-power state.\n");
    } else if (wParam == PBT_APMRESUMEAUTOMATIC) {
        wprintf(L"  Resuming from low-power state.\n");
        // followed by PBT_APMRESUMESUSPEND _if_ triggered by user interaction
    } else if (wParam == PBT_APMRESUMESUSPEND) {
        // only delivered if resume is triggered by user interaction 
        wprintf(L"  Resumed operation after being suspended.\n");
    } else {
        // other power event
        wprintf(L"  wParam=0x%x\n", (unsigned int)wParam);
    }
}


/** Window procedure for processing messages. */
LRESULT CALLBACK WindowProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_POWERBROADCAST:
        ProcessPowerEvent(wParam);
        break;
#if 0
    case WM_DEVICECHANGE:
        wprintf(L"HW attached or detached.\n");
        break;
#endif
    }

    return DefWindowProc(wnd, msg, wParam, lParam);
}


int WINAPI wmain () {
    HINSTANCE instance = GetModuleHandleW(NULL);

    // register window class
    const wchar_t CLASS_NAME[] = L"BatteryMonitor class";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClassW(&wc);

    // create offscreen window 
    HWND wnd = CreateWindowExW(0, // optional window styles
        CLASS_NAME,               // window class
        L"BatteryMonitor",        // window text
        WS_OVERLAPPEDWINDOW,      // window style
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, // size & position
        NULL,      // parent
        NULL,      // menu
        instance,  // instance handle
        NULL       // additional data
    );
    assert(wnd);

    {
        // subscribe to PBT_APMSUSPEND, PBT_APMRESUMEAUTOMATIC & PBT_APMRESUMESUSPEND events
        HPOWERNOTIFY hp = RegisterSuspendResumeNotification(wnd, DEVICE_NOTIFY_WINDOW_HANDLE);
        assert(hp);
        // unregister with UnregisterSuspendResumeNotification(hp);
    }

    // don't call ShowWindow to keep the window hidden

    // run message loop
    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
