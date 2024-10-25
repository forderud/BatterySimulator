#include <windows.h>
#include <setupapi.h>
#include <cassert>
#include <iostream>
#include <initguid.h>
#include <Devpkey.h>
#include "DeviceParams.hpp"
#include "PowerData.hpp"

#pragma comment (lib, "SetupAPI.lib")


int EnumerateDevices(bool printPowerData) {
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

        wprintf(L"\n");
        wprintf(L"== Device %i: %s ==\n", idx, GetDevRegPropStr(hDevInfo, devInfo, SPDRP_FRIENDLYNAME).c_str());
        wprintf(L"Description: %s\n", GetDevRegPropStr(hDevInfo, devInfo, SPDRP_DEVICEDESC).c_str());
        wprintf(L"HWID       : %s\n", GetDevRegPropStr(hDevInfo, devInfo, SPDRP_HARDWAREID).c_str()); // HW type ID
        wprintf(L"InstanceID : %s\n", GetDevPropStr(hDevInfo, devInfo, &DEVPKEY_Device_InstanceId).c_str()); // HWID with instance suffix
        wprintf(L"PDO        : %s\n", GetDevPropStr(hDevInfo, devInfo, &DEVPKEY_Device_PDOName).c_str()); // Physical Device Object
        wprintf(L"\n");

        if (printPowerData) {
            // TODO: Also query DEVPKEY_Device_PowerRelations

            CM_POWER_DATA powerData = {};
            ok = SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfo, SPDRP_DEVICE_POWER_DATA, nullptr, (BYTE*)&powerData, sizeof(powerData), nullptr);
            if (!ok) {
                DWORD res = GetLastError(); res;
                continue;
            }

            PrintPowerData(powerData);
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);

    return 0;
}


int main() {
    EnumerateDevices(true);
    return 0;
}
