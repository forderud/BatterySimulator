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

    // L"\\\\?\\GLOBALROOT" + PDOName can be passed to CreateFile (see https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file)
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


int EnumerateDevices(GUID classGuid, DeviceVisitor visitor) {
    DWORD flags = DIGCF_PRESENT;
    if (classGuid == GUID_NULL)
        flags |= DIGCF_ALLCLASSES; // query all connected devices

    HDEVINFO devInfo = SetupDiGetClassDevsW(&classGuid, 0, 0, flags);
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

int EnumerateInterfaces(GUID classGuid, DeviceVisitor visitor) {
    HDEVINFO devInfo = SetupDiGetClassDevsW(&classGuid, 0, 0, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    assert(devInfo != INVALID_HANDLE_VALUE);

    // iterate over all interfaces
    int idx = 0;
    for (; ; idx++) {
        SP_DEVICE_INTERFACE_DATA interfaceData{};
        interfaceData.cbSize = sizeof(interfaceData);
        BOOL ok = SetupDiEnumDeviceInterfaces(devInfo, nullptr, &classGuid, idx, &interfaceData);
        if (!ok) {
            DWORD err = GetLastError();
            if (err == ERROR_NO_MORE_ITEMS)
                break;
            abort();
        }

        SP_DEVINFO_DATA devInfoData = {};
        devInfoData.cbSize = sizeof(devInfoData);
        unsigned long detailDataSize = 0;
        ok = SetupDiGetDeviceInterfaceDetailW(devInfo, &interfaceData, nullptr, 0, &detailDataSize, &devInfoData);
        if (!ok) {
            DWORD err = GetLastError();
            assert(err == ERROR_INSUFFICIENT_BUFFER);
        }

        visitor(idx, devInfo, devInfoData);

#if 0
        std::unique_ptr<SP_DEVICE_INTERFACE_DETAIL_DATA, decltype(&free)> detailData{ static_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(malloc(detailDataSize)), &free };
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        ok = SetupDiGetDeviceInterfaceDetailW(devInfo, &interfaceData, detailData.get(), detailDataSize, &detailDataSize, nullptr);
        if (!ok) {
            DWORD err = GetLastError();
            assert(err == ERROR_INSUFFICIENT_BUFFER);
        }

        wprintf(L"DeviceInterfacePath: %s\n", detailData->DevicePath); // can be passsed to CreateFile
#endif
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    return idx;
}

enum class SCAN_MODE {
    ALL_DEVICES,
    USB_DEVICES,
    USB_INTERFACES,
};


int main() {
    bool printPowerData = true;
    DeviceVisitor visitor = VisitDevicePlain;
    if (printPowerData)
        visitor = VisitDevicePowerData;

    SCAN_MODE mode = SCAN_MODE::USB_DEVICES;
    switch (mode) {
    case SCAN_MODE::ALL_DEVICES:
        // search DOES includes logical devices beneath a composite USB device
        EnumerateDevices(GUID_NULL, visitor);
        break;
    case SCAN_MODE::USB_DEVICES:
        {
            // "USB Device" device setup class
            wchar_t USBDevice_str[] = L"{88bae032-5a81-49f0-bc3d-a4ff138216d6}";
            GUID classGuid{};
            CLSIDFromString(USBDevice_str, &classGuid);
            // search DOES includes logical devices beneath a composite USB device
            EnumerateDevices(classGuid, visitor);
        }
        break;
    case SCAN_MODE::USB_INTERFACES:
        // search does NOT include logical devices beneath a composite USB device
        EnumerateInterfaces(GUID_DEVINTERFACE_USB_DEVICE, visitor);
        break;
    }

    return 0;
}
