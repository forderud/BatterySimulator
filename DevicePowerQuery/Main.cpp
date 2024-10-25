#include <windows.h>
#include <setupapi.h>
#include <cassert>
#include <iostream>
#include <initguid.h>
#include <Devpkey.h>
#include "DeviceParams.hpp"
#include "PowerData.hpp"
#include <Usbiodef.h> // for GUID_DEVINTERFACE_USB_DEVICE

#pragma comment (lib, "SetupAPI.lib")

typedef void (*DeviceVisitor)(int idx, HDEVINFO devInfo, SP_DEVINFO_DATA devInfoData);


void VisitDevicePlain(int idx, HDEVINFO devInfo, SP_DEVINFO_DATA devInfoData) {
    wprintf(L"\n");
    wprintf(L"== Device %i: %s ==\n", idx, GetDevRegPropStr(devInfo, devInfoData, SPDRP_FRIENDLYNAME).c_str());
    wprintf(L"Description: %s\n", GetDevRegPropStr(devInfo, devInfoData, SPDRP_DEVICEDESC).c_str());
    wprintf(L"HWID       : %s\n", GetDevRegPropStr(devInfo, devInfoData, SPDRP_HARDWAREID).c_str()); // HW type ID
    wprintf(L"InstanceID : %s\n", GetDevPropStr(devInfo, devInfoData, &DEVPKEY_Device_InstanceId).c_str()); // HWID with instance suffix
    wprintf(L"PDO        : %s\n", GetDevPropStr(devInfo, devInfoData, &DEVPKEY_Device_PDOName).c_str()); // Physical Device Object
    wprintf(L"\n");
}

void VisitDevicePowerData(int idx, HDEVINFO devInfo, SP_DEVINFO_DATA devInfoData) {
    VisitDevicePlain(idx, devInfo, devInfoData);

    // TODO: Also query DEVPKEY_Device_PowerRelations

    CM_POWER_DATA powerData = {};
    BOOL ok = SetupDiGetDeviceRegistryPropertyW(devInfo, &devInfoData, SPDRP_DEVICE_POWER_DATA, nullptr, (BYTE*)&powerData, sizeof(powerData), nullptr);
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

    HDEVINFO devInfo = SetupDiGetClassDevsW(&ClassGuid, 0, 0, flags);
    assert(devInfo != INVALID_HANDLE_VALUE);

    // iterate over all devices
    int idx = 0;
    for (; ; idx++) {
        SP_DEVINFO_DATA devInfoData = {};
        devInfoData.cbSize = sizeof(devInfoData);

        BOOL ok = SetupDiEnumDeviceInfo(devInfo, idx, &devInfoData);
        if (!ok) {
            DWORD err = GetLastError();
            if (err == ERROR_NO_MORE_ITEMS)
                break;
            abort();
        }

        visitor(idx, devInfo, devInfoData);
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    return idx;
}

int EnumerateInterfaces(GUID ClassGuid, DeviceVisitor visitor) {
    HDEVINFO devInfo = SetupDiGetClassDevsW(&ClassGuid, 0, 0, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    assert(devInfo != INVALID_HANDLE_VALUE);

    // iterate over all interfaces
    int idx = 0;
    for (; ; idx++) {
        SP_DEVICE_INTERFACE_DATA interfaceData{};
        interfaceData.cbSize = sizeof(interfaceData);
        BOOL ok = SetupDiEnumDeviceInterfaces(devInfo, nullptr, &ClassGuid, idx, &interfaceData);
        if (!ok) {
            DWORD err = GetLastError();
            if (err == ERROR_NO_MORE_ITEMS)
                break;
            abort();
        }

        SP_DEVINFO_DATA devInfoData = {};
        devInfoData.cbSize = sizeof(devInfoData);
        unsigned long deviceInfoDataSize = 0;
        ok = SetupDiGetDeviceInterfaceDetailW(devInfo, &interfaceData, nullptr, 0, &deviceInfoDataSize, &devInfoData);
        if (!ok) {
            DWORD err = GetLastError();
            assert(err == ERROR_INSUFFICIENT_BUFFER);
        }

#if 0
        std::unique_ptr<SP_DEVICE_INTERFACE_DETAIL_DATA, decltype(&free)> detailData{ static_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(malloc(deviceInfoDataSize)), &free };
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        ok = SetupDiGetDeviceInterfaceDetailW(hDevInfo, &interfaceData, detailData.get(), deviceInfoDataSize, &deviceInfoDataSize, &devInfo);
        if (!ok) {
            DWORD err = GetLastError();
            assert(err == ERROR_INSUFFICIENT_BUFFER);
        }
#endif

        visitor(idx, devInfo, devInfoData);
}

    SetupDiDestroyDeviceInfoList(devInfo);
    return idx;
}


int main() {
    bool enumDevices = false;
    if (enumDevices) {
        GUID ClassGuid{};
#ifdef ONLY_USB_DEVICES
        // "USB Device" device setup class
        wchar_t USBDevice_str[] = L"{88bae032-5a81-49f0-bc3d-a4ff138216d6}";
        CLSIDFromString(USBDevice_str, &ClassGuid);
#endif
        EnumerateDevices(ClassGuid, VisitDevicePowerData);
    } else {
        EnumerateInterfaces(GUID_DEVINTERFACE_USB_DEVICE, VisitDevicePowerData);
    }
    return 0;
}
