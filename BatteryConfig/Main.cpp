#define INITGUID
#include "Battery.hpp"
#include <Cfgmgr32.h>
#pragma comment(lib, "Cfgmgr32.lib")
#include <Devpkey.h> // for DEVPKEY_Device_PDOName
#include <wrl/wrappers/corewrappers.h> // for FileHandle
#include <cassert>
#include <variant>
#include <vector>


std::variant<std::wstring, FILETIME> GetDevInstProperty(DEVINST dnDevInst, const DEVPROPKEY& propertyKey) {
    DEVPROPTYPE propertyType = 0;
    std::vector<BYTE> buffer(1024, 0);
    ULONG buffer_size = (ULONG)buffer.size();
    CONFIGRET res = CM_Get_DevNode_PropertyW(dnDevInst, &propertyKey, &propertyType, buffer.data(), &buffer_size, 0);
    if (res != CR_SUCCESS) {
        wprintf(L"ERROR: CM_Get_DevNode_PropertyW (res=%i).\n", res);
        return {};
    }
    buffer.resize(buffer_size);

    if (propertyType == DEVPROP_TYPE_STRING) {
        return reinterpret_cast<wchar_t*>(buffer.data());
    } else if (propertyType == DEVPROP_TYPE_FILETIME) {
        return *reinterpret_cast<FILETIME*>(buffer.data());
    }

    throw std::runtime_error("Unsupported CM_Get_DevNode_PropertyW type");
}

std::wstring FileTimeToDateStr(FILETIME& fileTime) {
    SYSTEMTIME time = {};
    BOOL ok = FileTimeToSystemTime(&fileTime, &time);
    if (!ok) {
        DWORD err = GetLastError();
        wprintf(L"ERROR: FileTimeToSystemTime failure (res=%i).\n", err);
        return {};
    }

    std::wstring dateString(128, L'\0');
    int char_count = GetDateFormatW(LOCALE_SYSTEM_DEFAULT, NULL, &time, NULL, dateString.data(), (int)dateString.size());
    dateString.resize(char_count-1); // exclude zero-termination
    return dateString;
}


/** Get the virtual file physical device object (PDO) path of a device driver instance. */
static std::wstring GetPDOPath(wchar_t* deviceInstancePath) {
    DEVINST dnDevInst = 0;
    CONFIGRET res = CM_Locate_DevNodeW(&dnDevInst, deviceInstancePath, CM_LOCATE_DEVNODE_NORMAL);
    if (res != CR_SUCCESS) {
        wprintf(L"ERROR: CM_Locate_DevNodeW (res=%i).\n", res);
        return {};
    }

    {
        // get driver version
        auto res = GetDevInstProperty(dnDevInst, DEVPKEY_Device_DriverVersion);
        wprintf(L"Driver version: %s.\n", std::get<std::wstring>(res).c_str());
    }
    {
        // get driver date
        auto res = GetDevInstProperty(dnDevInst, DEVPKEY_Device_DriverDate);
        wprintf(L"Driver date: %s.\n", FileTimeToDateStr(std::get<FILETIME>(res)).c_str());
    }

    std::wstring pdoPath;
    {
        // get PDO path
        auto res = GetDevInstProperty(dnDevInst, DEVPKEY_Device_PDOName);

        pdoPath = L"\\\\?\\GLOBALROOT"; // PDO prefix
        pdoPath += std::get<std::wstring>(res); // append PDO name
    }

    return pdoPath;
}


int wmain(int argc, wchar_t* argv[]) {
    if (argc < 3) {
        wprintf(L"USAGE: \"BatteryConfig.exe <N> <Charge>\" where <N> is the battery index and <Charge> is the new charge.\n");
        return 1;
    }

    const unsigned int batteryIdx = _wtoi(argv[1]); // 0 is first battery
    const unsigned int newCharge = _wtoi(argv[2]);

    wchar_t deviceInstancePath[] = L"ROOT\\BATTERY\\????"; // first simulated battery
    swprintf_s(deviceInstancePath, L"ROOT\\BATTERY\\%04i", batteryIdx); // replace ???? with a 4-digit integer 

    std::wstring fileName = GetPDOPath(deviceInstancePath);
    if (fileName.empty()) {
        wprintf(L"ERROR: Unable to locate battery %s\n", deviceInstancePath);
        return -1;
    }

    wprintf(L"Attempting to open %s\n", fileName.c_str());
    Microsoft::WRL::Wrappers::FileHandle battery(CreateFileW(fileName.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL));
    if (!battery.IsValid()) {
        DWORD err = GetLastError();
        wprintf(L"ERROR: CreateFileW (err=%i).\n", err);
        return -1;
    }

    wprintf(L"Battery opened...\n");

    BatteryInformationWrap info(battery.Get());
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
            status.PowerState = BATTERY_CHARGING;
        else if (newCharge < status.Capacity)
            status.PowerState = BATTERY_DISCHARGING;
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
