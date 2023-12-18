#define INITGUID
#include "Battery.hpp"
#include <Cfgmgr32.h>
#pragma comment(lib, "Cfgmgr32.lib")
#include <Devpkey.h> // for DEVPKEY_Device_PDOName
#include <wrl/wrappers/corewrappers.h> // for FileHandle
#include <cassert>
#include <variant>
#include <vector>




class DeviceInstance {
public:
    DeviceInstance(wchar_t* deviceInstancePath) {
        CONFIGRET res = CM_Locate_DevNodeW(&m_devInst, deviceInstancePath, CM_LOCATE_DEVNODE_NORMAL);
        if (res != CR_SUCCESS)
            throw std::runtime_error("ERROR: CM_Locate_DevNodeW");
    }

    ~DeviceInstance() {
    }

    std::wstring GetDriverVersion() const {
        auto res = GetDevInstProperty(DEVPKEY_Device_DriverVersion);
        return std::get<std::wstring>(res);
    }

    FILETIME GetDriverDate() const {
        auto res = GetDevInstProperty(DEVPKEY_Device_DriverDate);
        return std::get<FILETIME>(res);
    }

    /** Get the virtual file physical device object (PDO) path of a device driver instance. */
    std::wstring GetPDOPath() const {
        auto res = GetDevInstProperty(DEVPKEY_Device_PDOName);

        std::wstring pdoPath = L"\\\\?\\GLOBALROOT"; // PDO prefix
        pdoPath += std::get<std::wstring>(res); // append PDO name
        return pdoPath;

    }

    std::variant<std::wstring, FILETIME> GetDevInstProperty(const DEVPROPKEY& propertyKey) const {
        DEVPROPTYPE propertyType = 0;
        std::vector<BYTE> buffer(1024, 0);
        ULONG buffer_size = (ULONG)buffer.size();
        CONFIGRET res = CM_Get_DevNode_PropertyW(m_devInst, &propertyKey, &propertyType, buffer.data(), &buffer_size, 0);
        if (res != CR_SUCCESS) {
            wprintf(L"ERROR: CM_Get_DevNode_PropertyW (res=%i).\n", res);
            return {};
        }
        buffer.resize(buffer_size);

        if (propertyType == DEVPROP_TYPE_STRING) {
            return reinterpret_cast<wchar_t*>(buffer.data());
        }
        else if (propertyType == DEVPROP_TYPE_FILETIME) {
            return *reinterpret_cast<FILETIME*>(buffer.data());
        }

        throw std::runtime_error("Unsupported CM_Get_DevNode_PropertyW type");
    }

    static std::wstring FileTimeToDateStr(FILETIME fileTime) {
        SYSTEMTIME time = {};
        BOOL ok = FileTimeToSystemTime(&fileTime, &time);
        if (!ok) {
            DWORD err = GetLastError();
            wprintf(L"ERROR: FileTimeToSystemTime failure (res=%i).\n", err);
            return {};
        }

        std::wstring dateString(128, L'\0');
        int char_count = GetDateFormatW(LOCALE_SYSTEM_DEFAULT, NULL, &time, NULL, dateString.data(), (int)dateString.size());
        dateString.resize(char_count - 1); // exclude zero-termination
        return dateString;
    }

    DEVINST m_devInst = 0;
};


int wmain(int argc, wchar_t* argv[]) {
    if (argc < 3) {
        wprintf(L"USAGE: \"BatteryConfig.exe <N> <Charge>\" where <N> is the battery index and <Charge> is the new charge.\n");
        return 1;
    }

    const unsigned int batteryIdx = _wtoi(argv[1]); // 0 is first battery
    const unsigned int newCharge = _wtoi(argv[2]);

    wchar_t deviceInstancePath[] = L"ROOT\\BATTERY\\????"; // first simulated battery
    swprintf_s(deviceInstancePath, L"ROOT\\BATTERY\\%04i", batteryIdx); // replace ???? with a 4-digit integer 

    std::wstring pdoPath;
    try {
        DeviceInstance dev(deviceInstancePath);

        auto ver = dev.GetDriverVersion();
        wprintf(L"Driver version: %s.\n", ver.c_str());
        auto time = dev.GetDriverDate();
        wprintf(L"Driver date: %s.\n", DeviceInstance::FileTimeToDateStr(time).c_str());

        pdoPath = dev.GetPDOPath();
    } catch (std::exception& e) {
        wprintf(L"ERROR: Unable to locate battery %s\n", deviceInstancePath);
        wprintf(L"ERROR: what: %hs\n", e.what());
        return -1;
    }

    wprintf(L"Attempting to open %s\n", pdoPath.c_str());
    Microsoft::WRL::Wrappers::FileHandle battery(CreateFileW(pdoPath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL));
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
