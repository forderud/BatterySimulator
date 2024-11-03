#include <windows.h>
#include <setupapi.h>
#include <cassert>
#include <iostream>
#include <initguid.h>
#include <Devpkey.h>
#include "DeviceParams.hpp"
#include "PowerData.hpp"
#include <Devguid.h>  // for GUID_DEVCLASS_*
#include <Batclass.h> // for GUID_DEVICE_BATTERY
#include <Hidclass.h> // for GUID_DEVINTERFACE_HID
#include <Usbiodef.h> // for GUID_DEVINTERFACE_USB_DEVICE

#pragma comment (lib, "SetupAPI.lib")

typedef void (*DeviceVisitor)(int idx, HDEVINFO devInfo, SP_DEVINFO_DATA& devInfoData);
typedef int (*EnumerateFunction)(GUID classGuid, DeviceVisitor visitor);

GUID ToGUID(const wchar_t* str) {
    GUID guid{};
    if (FAILED(CLSIDFromString(str, &guid)))
        abort();
    return guid;
}

void VisitDeviceBasic(int idx, HDEVINFO devInfo, SP_DEVINFO_DATA& devInfoData) {
    std::wstring friendlyName = GetDevRegPropStr(devInfo, devInfoData, SPDRP_FRIENDLYNAME).c_str();
    if (friendlyName.empty())
        friendlyName = L"<unknown>";

    wprintf(L"\n");
    wprintf(L"== Device %i: %s ==\n", idx, friendlyName.c_str());
    wprintf(L"Description: %s\n", GetDevRegPropStr(devInfo, devInfoData, SPDRP_DEVICEDESC).c_str());
    wprintf(L"HWID       : %s\n", GetDevRegPropStr(devInfo, devInfoData, SPDRP_HARDWAREID).c_str()); // HW type ID
    wprintf(L"InstanceID : %s\n", GetDevPropStr(devInfo, devInfoData, &DEVPKEY_Device_InstanceId).c_str()); // HWID with instance suffix
    wprintf(L"PDO        : %s\n", GetDevPropStr(devInfo, devInfoData, &DEVPKEY_Device_PDOName).c_str()); // Physical Device Object
    wprintf(L"\n");

    // L"\\\\?\\GLOBALROOT" + PDOName can be passed to CreateFile (see https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file)
}

void VisitDevicePowerData(int idx, HDEVINFO devInfo, SP_DEVINFO_DATA& devInfoData) {
    VisitDeviceBasic(idx, devInfo, devInfoData); // print basic parameters first

    // TODO: Also query DEVPKEY_Device_PowerRelations

    // then print power data
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

void PrintDevicePath(HDEVINFO devInfo, SP_DEVICE_INTERFACE_DATA& interfaceData) {
    DWORD detailDataSize = 0;
    BOOL ok = SetupDiGetDeviceInterfaceDetailW(devInfo, &interfaceData, nullptr, 0, &detailDataSize, nullptr);
    if (!ok) {
        DWORD err = GetLastError();
        assert(err == ERROR_INSUFFICIENT_BUFFER);
    }

    std::unique_ptr<SP_DEVICE_INTERFACE_DETAIL_DATA_W, decltype(&free)> detailData{static_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(malloc(detailDataSize)), &free};
    detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

    ok = SetupDiGetDeviceInterfaceDetailW(devInfo, &interfaceData, detailData.get(), detailDataSize, &detailDataSize, nullptr);
    assert(ok);

    wprintf(L"DeviceInterfacePath: %s\n", detailData->DevicePath); // can be passsed to CreateFile
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
        ok = SetupDiGetDeviceInterfaceDetailW(devInfo, &interfaceData, nullptr, 0, nullptr, &devInfoData);
        if (!ok) {
            DWORD err = GetLastError();
            assert(err == ERROR_INSUFFICIENT_BUFFER);
        }

        visitor(idx, devInfo, devInfoData);
        PrintDevicePath(devInfo, interfaceData);
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    return idx;
}


int wmain(int argc, wchar_t* argv[]) {
    GUID device_class = GUID_NULL;
    GUID interface_class = GUID_NULL;
    EnumerateFunction enumerator = EnumerateDevices;
    DeviceVisitor visitor = VisitDeviceBasic; // only print basic device information

    const wchar_t usage_helpstring[] = L"USAGE DevicePowerQuery.exe [--all | --usb | --hid | --battery] [--devices | --interfaces] [--power]\n";

    // Parse command-line arguments
    if (argc < 2) {
        wprintf(usage_helpstring);
        return 1;
    }
    for (int idx = 1; idx < argc; idx++) {
        std::wstring arg = argv[idx];
        if (arg == L"--power") {
            visitor = VisitDevicePowerData; // also print power data
        } else if (arg == L"--all") {
            device_class = GUID_NULL; // devices mode (DOES includes logical devices beneath a composite USB device)
            interface_class = GUID_NULL; // (empty search)
        } else if (arg == L"--usb") {
            //device_class = GUID_DEVCLASS_USB;
            device_class = ToGUID(L"{88bae032-5a81-49f0-bc3d-a4ff138216d6}"); // "USB Device" device setup class for custom devices that doesn't belong to another class (DOES includes logical devices beneath a composite USB device)
            interface_class = GUID_DEVINTERFACE_USB_DEVICE; // physical USB devices (does _not_ include logical devices beneath a composite USB device)
        } else if (arg == L"--hid") {
            device_class = GUID_DEVCLASS_HIDCLASS;// "HID Device" device setup class
            interface_class = GUID_DEVINTERFACE_HID; // HID devices
        } else if (arg == L"--battery") {
            // "Battery Device" device interface class
            device_class = GUID_DEVCLASS_BATTERY; // detects both batteries and AC adapters
            interface_class = GUID_DEVICE_BATTERY; // only detects batteries, and _not_ AC adapters
            assert(GUID_DEVCLASS_BATTERY == GUID_DEVICE_BATTERY);
        } else if (arg == L"--devices") {
            enumerator = EnumerateDevices;
        } else if (arg == L"--interfaces") {
            enumerator = EnumerateInterfaces;
        } else {
            wprintf(usage_helpstring);
            return 1;
        }
    }

    // search for devices or interfaces
    if (enumerator == EnumerateDevices)
        enumerator(device_class, visitor);
    else if (enumerator == EnumerateInterfaces)
        enumerator(interface_class, visitor);

    return 0;
}
