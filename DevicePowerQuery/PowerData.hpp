#pragma once


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

static void PrintPowerData(const CM_POWER_DATA& powerData) {
    wprintf(L"Current power state: %s.\n", DevicePowerStateStr(powerData.PD_MostRecentPowerState));

    wprintf(L"\n");
    wprintf(L"Power state mapping:\n");
    for (size_t state = PowerSystemWorking; state < PowerSystemMaximum; state++)
        wprintf(L"  %s: %s\n", SystemPowerStateStr((SYSTEM_POWER_STATE)state), DevicePowerStateStr(powerData.PD_PowerStateMapping[state]));

    if (powerData.PD_D1Latency | powerData.PD_D2Latency | powerData.PD_D3Latency) {
        wprintf(L"\n");
        wprintf(L"Wakeup latencies:\n");
        wprintf(L"  From D1: %u ms\n", powerData.PD_D1Latency / 10); // convert 100us unit to ms
        wprintf(L"  From D2: %u ms\n", powerData.PD_D2Latency / 10);
        wprintf(L"  From D3: %u ms\n", powerData.PD_D3Latency / 10);
    }

    wprintf(L"\n");
}
