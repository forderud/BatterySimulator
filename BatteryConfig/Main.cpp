#define INITGUID
#include "Battery.hpp"
#include "DeviceInstance.hpp"
#include <wrl/wrappers/corewrappers.h> // for FileHandle
#include <cassert>


int wmain(int argc, wchar_t* argv[]) {
    if (argc < 3) {
        wprintf(L"USAGE: \"BatteryConfig.exe <N> <Charge>\" where <N> is the battery index and <Charge> is the new charge.\n");
        return 1;
    }

    const unsigned int batteryIdx = _wtoi(argv[1]); // 0 is first battery
    const unsigned int newCharge = _wtoi(argv[2]);

    wchar_t deviceInstancePath[18] = {};
    swprintf_s(deviceInstancePath, L"SWD\\DEVGEN\\%i", batteryIdx); // add device index suffix
    wprintf(L"DeviceInstancePath: %s\n", deviceInstancePath);

    std::wstring pdoPath;
    try {
        DeviceInstance dev(deviceInstancePath);

        auto ver = dev.GetDriverVersion();
        wprintf(L"  Driver version: %s.\n", ver.c_str());
        auto time = dev.GetDriverDate();
        wprintf(L"  Driver date: %s.\n", DeviceInstance::FileTimeToDateStr(time).c_str());

        pdoPath = dev.GetPDOPath();
    } catch (std::exception& e) {
        wprintf(L"ERROR: Unable to locate battery %s\n", deviceInstancePath);
        wprintf(L"ERROR: what: %hs\n", e.what());
        return -1;
    }

    wprintf(L"Opening %s\n", pdoPath.c_str());
    Microsoft::WRL::Wrappers::FileHandle battery(CreateFileW(pdoPath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL));
    if (!battery.IsValid()) {
        DWORD err = GetLastError();
        wprintf(L"ERROR: CreateFileW (err=%i).\n", err);
        return -1;
    }

    BatteryInformationWrap info(battery.Get());
    wprintf(L"\n"); 
    wprintf(L"Battery information:\n");
    info.Print();
    wprintf(L"\n");

    BatteryStausWrap status(battery.Get());
    wprintf(L"Battery status (before update):\n");
    status.Print();
    wprintf(L"\n");

#if 0
    // update battery information
    info.Set(battery.Get());
#endif

    // update battery charge level
    {
        // toggle between charge and dischage
        if (newCharge > status.Capacity)
            status.PowerState = BATTERY_POWER_ON_LINE | BATTERY_CHARGING; // charging while on AC power
        else if (newCharge < status.Capacity)
            status.PowerState = BATTERY_DISCHARGING; // discharging
        else
            status.PowerState = 0; // same charge as before

        // update charge level
        status.Capacity = newCharge;

        status.Rate = BATTERY_UNKNOWN_RATE; // was 0
        status.Voltage = BATTERY_UNKNOWN_VOLTAGE; // was -1

        status.Set(battery.Get());
    }

    wprintf(L"Battery status (after update):\n");
    status.Print();

    return 0;
}
