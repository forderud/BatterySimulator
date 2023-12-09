#include <windows.h>
#include <powrprof.h>
#pragma comment(lib, "Powrprof.lib")
#include <cassert>
#include <iostream>


/** Power event callback that's called when entering sleep or resuming. */
ULONG SuspendResumeEvent(void* context, ULONG type, void* setting) {
    wprintf(L"SuspendResumeEvent:\n");
    assert(context == nullptr);

    if (type == PBT_APMSUSPEND)
        wprintf(L"  PBT_APMSUSPEND\n");
    else if (type == PBT_APMRESUMESUSPEND)
        wprintf(L"  PBT_APMRESUMESUSPEND\n");
    else if (type == PBT_APMRESUMEAUTOMATIC)
        wprintf(L"  PBT_APMRESUMEAUTOMATIC\n");
    else
        wprintf(L"  type=0x%x\n", (unsigned int)type);

    return ERROR_SUCCESS;
}

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
        // TODO: Figure out why this is never called.
    } else if (wParam == PBT_APMRESUMEAUTOMATIC) {
        // followed by PBT_APMRESUMESUSPEND if triggered by user interaction
        wprintf(L"  Resuming from low-power state.");
        // TODO: Figure out why this is never called.
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
        // register to suspend and resume events
        DEVICE_NOTIFY_SUBSCRIBE_PARAMETERS nsp = {};
        nsp.Callback = SuspendResumeEvent;
        nsp.Context = nullptr;
        HPOWERNOTIFY hsr = 0; 
        DWORD err = PowerRegisterSuspendResumeNotification(DEVICE_NOTIFY_CALLBACK, &nsp, &hsr);
        assert(err == ERROR_SUCCESS);
        // unregister with PowerUnregisterSuspendResumeNotification(hsr);
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
