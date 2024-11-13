#include "DeviceEnum.hpp"
#include <initguid.h>
#include <Devpkey.h>
#include "PowerData.hpp"
#include <Devguid.h>  // for GUID_DEVCLASS_*
#include <poclass.h>  // for GUID_DEVICE_BATTERY
#include <Hidclass.h> // for GUID_DEVINTERFACE_HID
#include <Usbiodef.h> // for GUID_DEVINTERFACE_USB_DEVICE


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
        enumerator(device_class, visitor, true);
    else if (enumerator == EnumerateInterfaces)
        enumerator(interface_class, visitor, true);

    return 0;
}
