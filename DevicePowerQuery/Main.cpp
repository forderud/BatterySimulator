#include <windows.h>
#include <setupapi.h>
#include <cassert>
#include <iostream>
#include <initguid.h>
#include <Devpkey.h>

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

    wprintf(L"\n");
    wprintf(L"Power state mapping:\n");
    for (size_t state = PowerSystemWorking; state < PowerSystemMaximum; state++)
        wprintf(L"  %s: %s\n", SystemPowerStateStr((SYSTEM_POWER_STATE)state), DevicePowerStateStr(powerData.PD_PowerStateMapping[state]));

    if (powerData.PD_D1Latency | powerData.PD_D2Latency | powerData.PD_D3Latency) {
        wprintf(L"\n");
        wprintf(L"Wakeup latencies:\n");
        wprintf(L"  From D1: %u ms\n", powerData.PD_D1Latency/10); // convert 100us unit to ms
        wprintf(L"  From D2: %u ms\n", powerData.PD_D2Latency/10);
        wprintf(L"  From D3: %u ms\n", powerData.PD_D3Latency/10);
    }

    wprintf(L"\n");
}

static std::wstring GetDevRegPropStr(HDEVINFO hDevInfo, SP_DEVINFO_DATA& devInfo, DWORD property) {
    std::wstring result;
    result.resize(128, L'\0');
    DWORD requiredSize = 0;
    DWORD dataType = 0;
    BOOL ok = SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfo, property, &dataType, (BYTE*)result.data(), (DWORD)result.size()*sizeof(wchar_t), &requiredSize);
    if (!ok) {
        DWORD res = GetLastError(); res;
        return {};
    }

    if (dataType == REG_SZ) {
        // single string
        result.resize(requiredSize / sizeof(wchar_t) - 1); // exclude null-termination
    } else if (dataType == REG_MULTI_SZ) {
        // multiple zero-terminated strings
        size_t len = wcslen(result.c_str());
        result.resize(len); // return first string
    } else {
        assert(false);
    }

    return result;
}

static std::wstring GetDevPropStr(HDEVINFO hDevInfo, SP_DEVINFO_DATA& devInfo, const DEVPROPKEY*  property) {
    std::wstring result;
    result.resize(128, L'\0');
    DWORD requiredSize = 0;
    DEVPROPTYPE dataType = 0;
    BOOL ok = SetupDiGetDevicePropertyW(hDevInfo, &devInfo, property, &dataType, (BYTE*)result.data(), (DWORD)result.size() * sizeof(wchar_t), &requiredSize, 0);
    if (!ok) {
        DWORD res = GetLastError(); res;
        return {};
    }
    assert(dataType == DEVPROP_TYPE_STRING);

    // single string
    result.resize(requiredSize / sizeof(wchar_t) - 1); // exclude null-termination
    return result;
}

int GetDeviceDriverPowerData() {
    // query all connected devices
    GUID ClassGuid{}; // no filtering
    HDEVINFO hDevInfo = SetupDiGetClassDevsW(&ClassGuid, 0, 0, DIGCF_ALLCLASSES | DIGCF_PRESENT);
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
            abort();
        }

        wprintf(L"\n== Device %i: %s ==\n", idx, GetDevRegPropStr(hDevInfo, devInfo, SPDRP_DEVICEDESC).c_str()); // SPDRP_FRIENDLYNAME or SPDRP_DEVICEDESC
        wprintf(L"HWID: %s\n", GetDevRegPropStr(hDevInfo, devInfo, SPDRP_HARDWAREID).c_str());
        wprintf(L"PDO : %s\n", GetDevPropStr(hDevInfo, devInfo, &DEVPKEY_Device_PDOName).c_str());

#if 1
        // TODO: Also query DEVPKEY_Device_PowerRelations

        CM_POWER_DATA powerData = {};
        ok = SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfo, SPDRP_DEVICE_POWER_DATA, nullptr, (BYTE*)&powerData, sizeof(powerData), nullptr);
        if (!ok) {
            DWORD res = GetLastError(); res;
            continue;
        }

        PrintPowerData(powerData);
#endif
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);

    return 0;
}

int main() {
    GetDeviceDriverPowerData();
}
