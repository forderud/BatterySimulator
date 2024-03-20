#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <cassert>
#include <iostream>


#pragma comment (lib, "SetupAPI.lib")


static const wchar_t* SystemPowerStateStr(SYSTEM_POWER_STATE state) {
    switch (state) {
    //case PowerSystemUnspecified: return   L"unspecified";
    case PowerSystemWorking:   return L"working (S0)";
    case PowerSystemSleeping1: return L"sleep (S1)  ";
    case PowerSystemSleeping2: return L"sleep (S2)  ";
    case PowerSystemSleeping3: return L"sleep (S3)  ";
    case PowerSystemHibernate: return L"hibernate (S4)";
    case PowerSystemShutdown:  return L"shutdown (S5)";
    //case PowerSystemMaximum: return L"maximum";
    default:
        abort();
    }
}

static const wchar_t* DevicePowerStateStr(DEVICE_POWER_STATE state) {
    switch (state) {
    case PowerDeviceUnspecified: return L"unspecified";
    case PowerDeviceD0: return L"on (D0)";
    case PowerDeviceD1: return L"sleep (D1)";
    case PowerDeviceD2: return L"sleep (D2)";
    case PowerDeviceD3: return L"off (D3)";
    //case PowerDeviceMaximum: return L"maximum";
    default:
        abort();
    }
}

static void PrintPowerData(CM_POWER_DATA powerData) {
    wprintf(L"Current power state: %s.\n", DevicePowerStateStr(powerData.PD_MostRecentPowerState));

    wprintf(L"\nPower state mapping:\n");
    for (size_t state = PowerSystemWorking; state < PowerSystemMaximum; state++) {
        wprintf(L"  %s: %s\n", SystemPowerStateStr((SYSTEM_POWER_STATE)state), DevicePowerStateStr(powerData.PD_PowerStateMapping[state]));
    }

    wprintf(L"\nWakeup latencies:\n");
    wprintf(L"  From D1: %i ms\n", powerData.PD_D1Latency / 10); // convert 100us to ms
    wprintf(L"  From D2: %i ms\n", powerData.PD_D2Latency / 10);
    wprintf(L"  From D3: %i ms\n", powerData.PD_D3Latency / 10);

    wprintf(L"\n");
}

int GetDeviceDriverPowerData() {
    // query all connected devices
    HDEVINFO hDevInfo = SetupDiGetClassDevsW(NULL, 0, 0, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    assert(hDevInfo != INVALID_HANDLE_VALUE);

    // iterate over all devices
    for (int idx = 0; ; idx++) {
        SP_DEVINFO_DATA devInfo = {};
        devInfo.cbSize = sizeof(devInfo);

        BOOL ok = SetupDiEnumDeviceInfo(hDevInfo, idx, &devInfo);
        if (!ok) {
            DWORD err = GetLastError();
            if (err == ERROR_NO_MORE_ITEMS)
                break;

            wprintf(L"SetupDiEnumDeviceInfo failed with %d.\n", err);
            continue;
        }

        // TODO: Also query DEVPKEY_Device_PowerRelations

        CM_POWER_DATA powerData = {};
        ok = SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfo, SPDRP_DEVICE_POWER_DATA, nullptr, (BYTE*)&powerData, sizeof(powerData), nullptr);
        if (!ok) {
            DWORD res = GetLastError();
            continue;
        }

        PrintPowerData(powerData);
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);

    return 0;
}

int main() {
    GetDeviceDriverPowerData();
}
