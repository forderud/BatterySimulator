#define INITGUID
#include "Battery.hpp"
#include <Cfgmgr32.h>
#pragma comment(lib, "Cfgmgr32.lib")
#include <Devpkey.h> // for DEVPKEY_Device_PDOName
#include <wrl/wrappers/corewrappers.h> // for FileHandle
#include <cassert>
#include <vector>


std::vector<BYTE> GetDevInstProperty(DEVINST dnDevInst, const DEVPROPKEY& propertyKey, /*out*/DEVPROPTYPE& propertyType) {
    std::vector<BYTE> buffer(1024, 0);
    ULONG buffer_size = (ULONG)buffer.size();
    CONFIGRET res = CM_Get_DevNode_PropertyW(dnDevInst, &propertyKey, &propertyType, buffer.data(), &buffer_size, 0);
    if (res != CR_SUCCESS) {
        wprintf(L"ERROR: CM_Get_DevNode_PropertyW (res=%i).\n", res);
        return {};
    }
    buffer.resize(buffer_size);
    return buffer;
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
        DEVPROPTYPE PropertyType = 0;
        std::vector<BYTE> buffer = GetDevInstProperty(dnDevInst, DEVPKEY_Device_DriverVersion, PropertyType);
        assert(PropertyType == DEVPROP_TYPE_STRING);
        wprintf(L"Driver version: %s.\n", reinterpret_cast<wchar_t*>(buffer.data()));
    }
    {
        // get driver date
        DEVPROPTYPE PropertyType = 0;
        std::vector<BYTE> buffer = GetDevInstProperty(dnDevInst, DEVPKEY_Device_DriverDate, PropertyType);
        assert(PropertyType == DEVPROP_TYPE_FILETIME);

        SYSTEMTIME time = {};
        BOOL ok = FileTimeToSystemTime(reinterpret_cast<FILETIME*>(buffer.data()), &time);
        if (!ok) {
            DWORD err = GetLastError();
            wprintf(L"ERROR: FileTimeToSystemTime failure (res=%i).\n", err);
            return {};
        }

        std::vector<wchar_t> dateString(128, L'\0');
        int char_count = GetDateFormatW(LOCALE_SYSTEM_DEFAULT, NULL, &time, NULL, dateString.data(), (int)dateString.size());
        dateString.resize(char_count);
        wprintf(L"Driver date: %s.\n", dateString.data());
    }

    std::wstring pdoPath;
    {
        // get PDO path
        DEVPROPTYPE PropertyType = 0;
        std::vector<BYTE> buffer = GetDevInstProperty(dnDevInst, DEVPKEY_Device_PDOName, PropertyType);
        assert(PropertyType == DEVPROP_TYPE_STRING);

        pdoPath = L"\\\\?\\GLOBALROOT"; // PDO prefix
        pdoPath += reinterpret_cast<wchar_t*>(buffer.data()); // append PDO name
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

    BatteryInformationWrap info;
    info.Get(battery.Get());
    wprintf(L"Battery information:\n");
    info.Print();
    wprintf(L"\n");

    BatteryStausWrap status;
    status.Get(battery.Get());
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
