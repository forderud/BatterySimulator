#define INITGUID
#include "Battery.hpp"
#include "DeviceInstance.hpp"
#include <wrl/wrappers/corewrappers.h> // for FileHandle
#include <cassert>


int wmain(int argc, wchar_t* argv[]) {
    if (argc < 2) {
        wprintf(L"USAGE: \"BatteryQuery.exe <N> <Charge>\" where <N> is the battery index and <Charge> is the new charge.\n");
        return 1;
    }

    const wchar_t* instanceId = argv[1]; // 0 is first battery
    unsigned int newCharge = static_cast<unsigned int>(-1); // skip updating by default
    if (argc >= 3)
        newCharge = _wtoi(argv[2]); // in [0,100] range

    std::wstring pdoPath;
    try {
        DeviceInstance dev(instanceId);

        auto ver = dev.GetDriverVersion();
        wprintf(L"  Driver version: %s.\n", ver.c_str());
        auto time = dev.GetDriverDate();
        wprintf(L"  Driver date: %s.\n", DeviceInstance::FileTimeToDateStr(time).c_str());

        pdoPath = dev.GetPDOPath();
    } catch (std::exception& e) {
        wprintf(L"ERROR: Unable to locate battery %s\n", instanceId);
        wprintf(L"ERROR: what: %hs\n", e.what());
        return -1;
    }

    wprintf(L"\n");
    wprintf(L"Opening %s\n", pdoPath.c_str());
    Microsoft::WRL::Wrappers::FileHandle battery(CreateFileW(pdoPath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL));
    if (!battery.IsValid()) {
        DWORD err = GetLastError();
        wprintf(L"ERROR: CreateFileW (err=%u).\n", err);
        return -1;
    }

    {
        wprintf(L"\n");
        wprintf(L"Battery information fields:\n");
        wprintf(L"  BatteryDeviceName:      %s\n", GetBatteryInfoStr(battery.Get(), BatteryDeviceName).c_str());

        ULONG estimatedTime = BATTERY_UNKNOWN_TIME;
        GetBatteryInfoUlong(battery.Get(), BatteryEstimatedTime, estimatedTime);
        if (estimatedTime != BATTERY_UNKNOWN_TIME)
            wprintf(L"  BatteryEstimatedTime:   %u\n", estimatedTime);
        else
            wprintf(L"  BatteryEstimatedTime:   <unknown>\n");
        
        BATTERY_REPORTING_SCALE scale[4] = {};
        unsigned int count = GetBatteryInfoGranularity(battery.Get(), scale);
        for (unsigned int idx = 0; idx < count; ++idx)
            wprintf(L"  BatteryGranularityInformation %u: Granularity=%u mWh, Capacity=%u mWh\n", idx, scale[idx].Granularity, scale[idx].Capacity);
        if (count == 0)
            wprintf(L"  BatteryGranularityInformation: <unknown>\n");

        BATTERY_MANUFACTURE_DATE date = {};
        if (GetBatteryInfoDate(battery.Get(), date))
            wprintf(L"  BatteryManufactureDate: %u-%u-%u\n", date.Year, date.Month, date.Day);
        else
            wprintf(L"  BatteryManufactureDate: <unknown>\n");
        
        wprintf(L"  BatteryManufactureName: %s\n", GetBatteryInfoStr(battery.Get(), BatteryManufactureName).c_str());
        wprintf(L"  BatterySerialNumber:    %s\n", GetBatteryInfoStr(battery.Get(), BatterySerialNumber).c_str());
        {
            ULONG temp = 0;
            if (GetBatteryInfoUlong(battery.Get(), BatteryTemperature, temp))
                wprintf(L"  BatteryTemperature:     %u\n", temp);
            else
                wprintf(L"  BatteryTemperature:     <unknown>\n");
        }
        wprintf(L"  BatteryUniqueID:        %s\n", GetBatteryInfoStr(battery.Get(), BatteryUniqueID).c_str());
    }
    wprintf(L"\n");

    BatteryInformationWrap info(battery.Get());
    wprintf(L"BATTERY_INFORMATION parameters:\n");
    info.Print();
    wprintf(L"\n");

    BatteryStausWrap status(battery.Get());
    wprintf(L"BATTERY_STATUS parameters (before update):\n");
    status.Print();
    wprintf(L"\n");

#if 0
    // update battery information
    info.Set(battery.Get());
#endif

    // update battery charge level
    if (newCharge != static_cast<unsigned int>(-1)) {
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

        wprintf(L"BATTERY_STATUS parameters (after update):\n");
        status.Print();
    }

    return 0;
}
