#define INITGUID
#include <iostream>
#include <Windows.h>
#include <Cfgmgr32.h>
#pragma comment(lib, "Cfgmgr32.lib")
#include <Devpropdef.h>
#include <Devpkey.h> // for DEVPKEY_Device_PDOName
#include <wrl/wrappers/corewrappers.h>
#include <vector>

#include "../simbatt/simbattdriverif.h"
#include <poclass.h>

using FileHandle = Microsoft::WRL::Wrappers::FileHandle;


/** Get the virtual file physical device object (PDO) path of a device driver instance. */
static std::wstring GetPDOPath(wchar_t* deviceInstancePath) {
    DEVINST dnDevInst = 0;
    CONFIGRET res = CM_Locate_DevNodeW(&dnDevInst, deviceInstancePath, CM_LOCATE_DEVNODE_NORMAL);
    if (res != CR_SUCCESS) {
        wprintf(L"ERROR: CM_Locate_DevNodeW (res=%i).\n", res);
        return {};
    }

    DEVPROPTYPE PropertyType = 0;
    std::vector<BYTE> buffer(1024, 0);
    ULONG buffer_size = (ULONG)buffer.size();
    res = CM_Get_DevNode_PropertyW(dnDevInst, &DEVPKEY_Device_PDOName, &PropertyType, buffer.data(), &buffer_size, 0);
    if (res != CR_SUCCESS) {
        wprintf(L"ERROR: CM_Get_DevNode_PropertyW (res=%i).\n", res);
        return {};
    }
    buffer.resize(buffer_size);

    std::wstring pdoPath = L"\\\\?\\Global\\GLOBALROOT"; // PDO prefix
    pdoPath += reinterpret_cast<wchar_t*>(buffer.data()); // append PDO name
    return pdoPath;
}


int wmain(int argc, wchar_t* argv[]) {
    unsigned int batteryIdx = 0; // default to first battery
    if (argc >= 2) {
        batteryIdx = _wtoi(argv[1]);
    }

    wchar_t deviceInstancePath[] = L"ROOT\\BATTERY\\????"; // first simulated battery
    swprintf_s(deviceInstancePath, L"ROOT\\BATTERY\\%04i", batteryIdx); // replace ???? with a 4-digit integer 

    std::wstring fileName = GetPDOPath(deviceInstancePath);
    if (fileName.empty()) {
        wprintf(L"ERROR: Unable to locate battery %s\n", deviceInstancePath);
        return -1;
    }

    wprintf(L"Attempting to open %s\n", fileName.c_str());
    FileHandle battery(CreateFileW(fileName.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL));
    if (!battery.IsValid()) {
        DWORD err = GetLastError();
        wprintf(L"ERROR: CreateFileW (err=%i).\n", err);
        return -1;
    }

    wprintf(L"Battery opened...\n");

    BATTERY_STATUS status = {};
    BATTERY_INFORMATION info = {};
    {
        ULONG battery_tag = 0;
        ULONG wait = 0;
        DWORD bytes_returned = 0;
        BOOL ok = DeviceIoControl(battery.Get(), IOCTL_BATTERY_QUERY_TAG, &wait, sizeof(wait), &battery_tag, sizeof(battery_tag), &bytes_returned, nullptr);
        if (!ok) {
            DWORD err = GetLastError();
            wprintf(L"ERROR: IOCTL_BATTERY_QUERY_TAG (err=%i).\n", err);
            return -1;
        }

        // query BATTERY_STATUS status
        BATTERY_WAIT_STATUS wait_status = {};
        wait_status.BatteryTag = battery_tag;
        ok = DeviceIoControl(battery.Get(), IOCTL_BATTERY_QUERY_STATUS, &wait_status, sizeof(wait_status), &status, sizeof(status), &bytes_returned, nullptr);
        if (!ok) {
            DWORD err = GetLastError();
            wprintf(L"ERROR: IOCTL_BATTERY_QUERY_STATUS (err=%i).\n", err);
            return -1;
        }

        // query BATTERY_INFORMATION info
        BATTERY_QUERY_INFORMATION bqi = {};
        bqi.InformationLevel = BatteryInformation;
        bqi.BatteryTag = battery_tag;
        ok = DeviceIoControl(battery.Get(), IOCTL_BATTERY_QUERY_INFORMATION, &bqi, sizeof(bqi), &info, sizeof(info), &bytes_returned, nullptr);
        if (!ok) {
            DWORD err = GetLastError();
            wprintf(L"ERROR: BatteryInformation (err=%i).\n", err);
            return -1;
        }

#if 0
        // query BATTERY_REPORTING_SCALE scale (fails with error 1 "Incorrect function")
        bqi = {};
        bqi.InformationLevel = BatteryGranularityInformation;
        bqi.BatteryTag = battery_tag;
        BATTERY_REPORTING_SCALE scale[4] = {};
        ok = DeviceIoControl(battery.Get(), IOCTL_BATTERY_QUERY_INFORMATION, &bqi, sizeof(bqi), &scale, sizeof(scale), &bytes_returned, nullptr);
        if (!ok) {
            DWORD err = GetLastError();
            wprintf(L"ERROR: BatteryGranularityInformation (err=%i).\n", err);
            return -1;
        }
#endif
    }

    {
        wprintf(L"Battery information:\n");
        wprintf(L"  Capabilities=%i\n", info.Capabilities);
        wprintf(L"  Chemistry=%hs\n", std::string((char*)info.Chemistry, 4).c_str()); // not null-terminated
        wprintf(L"  CriticalBias=%i\n", info.CriticalBias);
        wprintf(L"  CycleCount=%i\n", info.CycleCount);
        wprintf(L"  DefaultAlert1=%i\n", info.DefaultAlert1);
        wprintf(L"  DefaultAlert2=%i\n", info.DefaultAlert2);
        wprintf(L"  DesignedCapacity=%i\n", info.DesignedCapacity);
        wprintf(L"  FullChargedCapacity=%i\n", info.FullChargedCapacity);
        wprintf(L"  Technology=%i\n", info.Technology);
        wprintf(L"\n");
        wprintf(L"Battery status (before update):\n");
        wprintf(L"  Capacity=%i\n", status.Capacity);
        wprintf(L"  PowerState=%i\n", status.PowerState);
        wprintf(L"  Rate=%i\n", status.Rate);
        wprintf(L"  Voltage=%i\n", status.Voltage);
        wprintf(L"\n");
    }

#if 0
    // update battery information
    {
        BOOL ok = DeviceIoControl(battery.Get(), IOCTL_SIMBATT_SET_INFORMATION, &info, sizeof(info), nullptr, 0, nullptr, nullptr);
        if (!ok) {
            DWORD err = GetLastError();
            wprintf(L"ERROR: IOCTL_SIMBATT_SET_INFORMATION (err=%i).\n", err);
            return -1;
        }
    }
#endif

    // update battery charge level
    {
        // toggle between charge and dischage
        if (status.PowerState == BATTERY_DISCHARGING)
            status.PowerState = BATTERY_CHARGING;
        else
            status.PowerState = BATTERY_DISCHARGING;

        // decrease charge by 10%
        status.Capacity = (status.Capacity - 10 + info.FullChargedCapacity) % info.FullChargedCapacity;

        status.Rate = BATTERY_UNKNOWN_RATE; // was 0
        status.Voltage = BATTERY_UNKNOWN_VOLTAGE; // was -1

        BOOL ok = DeviceIoControl(battery.Get(), IOCTL_SIMBATT_SET_STATUS, &status, sizeof(status), nullptr, 0, nullptr, nullptr);
        if (!ok) {
            DWORD err = GetLastError();
            wprintf(L"ERROR: IOCTL_SIMBATT_SET_STATUS (err=%i).\n", err);
            return -1;
        }
    }

    {
        wprintf(L"Battery status (after update):\n");
        wprintf(L"  Capacity=%i\n", status.Capacity);
        wprintf(L"  PowerState=%i\n", status.PowerState);
        wprintf(L"  Rate=%x\n", status.Rate);
        wprintf(L"  Voltage=%i\n", status.Voltage);
    }
    return 0;
}
