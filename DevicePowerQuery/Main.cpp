#include <windows.h>
#include <setupapi.h>
#include <cassert>
#include <iostream>
#include <initguid.h>
#include <Devpkey.h>
#include "DeviceParams.hpp"
#include "PowerData.hpp"

#pragma comment (lib, "SetupAPI.lib")

typedef void (*DeviceVisitor)(int idx, HDEVINFO hDevInfo, SP_DEVINFO_DATA devInfo);


void VisitDevicePlain(int idx, HDEVINFO hDevInfo, SP_DEVINFO_DATA devInfo) {
    wprintf(L"\n");
    wprintf(L"== Device %i: %s ==\n", idx, GetDevRegPropStr(hDevInfo, devInfo, SPDRP_FRIENDLYNAME).c_str());
    wprintf(L"Description: %s\n", GetDevRegPropStr(hDevInfo, devInfo, SPDRP_DEVICEDESC).c_str());
    wprintf(L"HWID       : %s\n", GetDevRegPropStr(hDevInfo, devInfo, SPDRP_HARDWAREID).c_str()); // HW type ID
    wprintf(L"InstanceID : %s\n", GetDevPropStr(hDevInfo, devInfo, &DEVPKEY_Device_InstanceId).c_str()); // HWID with instance suffix
    wprintf(L"PDO        : %s\n", GetDevPropStr(hDevInfo, devInfo, &DEVPKEY_Device_PDOName).c_str()); // Physical Device Object
    wprintf(L"\n");
}

void VisitDevicePowerData(int idx, HDEVINFO hDevInfo, SP_DEVINFO_DATA devInfo) {
    VisitDevicePlain(idx, hDevInfo, devInfo);

    // TODO: Also query DEVPKEY_Device_PowerRelations

    CM_POWER_DATA powerData = {};
    BOOL ok = SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfo, SPDRP_DEVICE_POWER_DATA, nullptr, (BYTE*)&powerData, sizeof(powerData), nullptr);
    if (ok) {
        PrintPowerData(powerData);
    } else {
        DWORD err = GetLastError();
        err;
        abort();
    }
}


int EnumerateDevices(GUID ClassGuid, DeviceVisitor visitor) {
    DWORD flags = DIGCF_PRESENT;
    if (ClassGuid == GUID_NULL)
        flags |= DIGCF_ALLCLASSES; // query all connected devices

    HDEVINFO hDevInfo = SetupDiGetClassDevsW(&ClassGuid, 0, 0, flags);
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

        visitor(idx, hDevInfo, devInfo);
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);

    return 0;
}


int main() {
    GUID ClassGuid{};
#ifdef ONLY_USB_DEVICES
    // "USB Device" device setup class
    wchar_t USBDevice_str[] = L"{88bae032-5a81-49f0-bc3d-a4ff138216d6}";
    CLSIDFromString(USBDevice_str, &ClassGuid);
#endif

    EnumerateDevices(ClassGuid, VisitDevicePowerData);
    return 0;
}
