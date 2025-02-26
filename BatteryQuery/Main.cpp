#include "Battery.hpp"
#include <batclass.h>
#include <initguid.h> // must be above Devpkey.h include
#include <Devpkey.h> // for DEVPKEY_xxx
#include <wrl/wrappers/corewrappers.h> // for FileHandle
#include <cassert>
#include <ShlObj.h>
#include "DeviceInstance.hpp"
#include "HidPowerDevice.hpp"
#include "DellBattery.hpp"
#include "../DevicePowerQuery/DeviceEnum.hpp"


/** RAII class for COM initialization. */
class ComInitialize {
public:
    ComInitialize(COINIT apartment /*= COINIT_MULTITHREADED*/) : m_initialized(false) {
        // REF: https://docs.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-coinitializeex
        HRESULT hr = CoInitializeEx(NULL, apartment);
        if (SUCCEEDED(hr))
            m_initialized = true;

        // enable admin calls if running as admin
        CHECK(CoInitializeSecurity(
            NULL,
            -1,                          // COM negotiates service
            NULL,                        // Authentication services
            NULL,                        // Reserved
            RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication 
            RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation
            NULL,                        // Authentication info
            EOAC_NONE,                   // Additional capabilities 
            NULL                         // Reserved
        ));
    }

    ~ComInitialize() {
        if (m_initialized)
            CoUninitialize();
    }

private:
    bool m_initialized; ///< must uninitialize in dtor
};


struct BatteryParameters {
    BatteryParameters(HANDLE dev, bool verbose) {
        DeviceName = GetBatteryInfoStr(dev, BatteryDeviceName);
        GetBatteryInfoUlong(dev, BatteryEstimatedTime, EstimatedTime, verbose);
        GranularityInformation_len = GetBatteryInfoGranularity(dev, GranularityInformation);
        GetBatteryInfoDate(dev, ManufactureDate);
        ManufactureName = GetBatteryInfoStr(dev, BatteryManufactureName);
        SerialNumber = GetBatteryInfoStr(dev, BatterySerialNumber);
        GetBatteryInfoUlong(dev, BatteryTemperature, Temperature, verbose);
        UniqueID = GetBatteryInfoStr(dev, BatteryUniqueID);
    }

    void Print() const {
        wprintf(L"  BatteryDeviceName:      %s\n", DeviceName.c_str());

        if (EstimatedTime != BATTERY_UNKNOWN_TIME)
            wprintf(L"  BatteryEstimatedTime:   %u s\n", EstimatedTime);
        else
            wprintf(L"  BatteryEstimatedTime:   <unknown>\n");

        for (unsigned int idx = 0; idx < GranularityInformation_len; ++idx)
            wprintf(L"  BatteryGranularityInformation %u: Resolution=%u mWh for Capacity<=%u mWh\n", idx, GranularityInformation[idx].Granularity, GranularityInformation[idx].Capacity);
        if (GranularityInformation_len == 0)
            wprintf(L"  BatteryGranularityInformation: <unknown>\n");

        if (ManufactureDate.Year)
            wprintf(L"  BatteryManufactureDate: %u-%u-%u\n", ManufactureDate.Year, ManufactureDate.Month, ManufactureDate.Day);
        else
            wprintf(L"  BatteryManufactureDate: <unknown>\n");

        wprintf(L"  BatteryManufactureName: %s\n", ManufactureName.c_str());
        wprintf(L"  BatterySerialNumber:    %s\n", SerialNumber.c_str());

        if (Temperature) {
            float tempCelsius = ((int)Temperature - 2731)*0.1f; // convert to Celsius
            wprintf(L"  BatteryTemperature:     %.1f Celsius\n", tempCelsius);
        } else {
            wprintf(L"  BatteryTemperature:     <unknown>\n");
        }
        wprintf(L"  BatteryUniqueID:        %s\n", UniqueID.c_str());
    }

    std::wstring DeviceName;
    ULONG EstimatedTime = BATTERY_UNKNOWN_TIME;
    BATTERY_REPORTING_SCALE GranularityInformation[4] = {};
    UINT                    GranularityInformation_len = 0;
    BATTERY_MANUFACTURE_DATE ManufactureDate = {};
    std::wstring ManufactureName;
    std::wstring SerialNumber;
    ULONG Temperature = 0; // in 10ths of a degree Kelvin
    std::wstring UniqueID;
};

int AccessBattery(const std::wstring& devInstPath, bool verbose, unsigned int newCharge = -1) {
    wprintf(L"\n");
    wprintf(L"Opening %s:\n", devInstPath.c_str());

    DeviceInstance dev(devInstPath.c_str());

    Microsoft::WRL::Wrappers::FileHandle battery(CreateFileW(dev.GetPDOPath().c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL));
    if (!battery.IsValid()) {
        DWORD err = GetLastError();
        wprintf(L"ERROR: CreateFileW (err %u).\n", err);
        return -1;
    }

    wprintf(L"\n");
    {
        BatteryParameters params(battery.Get(), verbose);

        wprintf(L"Misc. IOCTL_BATTERY_QUERY_INFORMATION parameters:\n");
        params.Print();
    }
    wprintf(L"\n");

    BatteryInformationWrap info(battery.Get());
    wprintf(L"BATTERY_INFORMATION parameters:\n");
    info.Print();
    wprintf(L"\n");

    BatteryStausWrap status(battery.Get());
    wprintf(L"BATTERY_STATUS parameters:\n");
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

void PrintReport(const std::vector<BYTE>& report) {
    wprintf(L"    {");
    for (BYTE elm : report)
        wprintf(L" %02x,", elm);
    wprintf(L"}\n");
}

bool AccessHidDevice(const std::wstring& pdoPath) {
    hid::Device dev(pdoPath.c_str(), false);
    if (!dev.IsValid())
        return false; // not a HID device

    wprintf(L"Accessing HID device %s:\n", dev.Name().c_str());
    dev.PrintInfo();
    wprintf(L"\n");

    wprintf(L"Available INPUT reports:\n");
    std::vector<HIDP_VALUE_CAPS> valCaps = dev.GetValueCaps(HidP_Input);
    for (auto& elm : valCaps) {
        wprintf(L"  ReportID: %#04x\n", elm.ReportID);
        wprintf(L"    UsagePage=%#04x, Usage=%#04x\n", elm.UsagePage, elm.NotRange.Usage);
        std::vector<BYTE> report = dev.GetReport(HidP_Input, elm.ReportID);
        //PrintReport(report);
        ULONG value = dev.GetUsageValue(HidP_Input, elm, report);
        wprintf(L"    Value=%u\n", value);
    }
    std::vector<HIDP_BUTTON_CAPS> butCaps = dev.GetButtonCaps(HidP_Input);
    for (UCHAR reportId : hid::Device::UniqueReportIDs<HIDP_BUTTON_CAPS>(butCaps)) {
        wprintf(L"  ReportID: %#04x\n", reportId);
        PrintReport(dev.GetReport(HidP_Input, reportId));
    }    

    wprintf(L"Available OUTPUT reports:\n");
    valCaps = dev.GetValueCaps(HidP_Output);
    for (auto& elm : valCaps) {
        wprintf(L"  ReportID: %#04x\n", elm.ReportID);
        wprintf(L"    UsagePage=%#04x, Usage=%#04x\n", elm.UsagePage, elm.NotRange.Usage);
        // cannot print output reports, since they're sent to the device
    }
    butCaps = dev.GetButtonCaps(HidP_Output);
    for (UCHAR reportId : hid::Device::UniqueReportIDs<HIDP_BUTTON_CAPS>(butCaps)) {
        wprintf(L"  ReportID: %#04x\n", reportId);
        // cannot print output reports, since they're sent to the device
    }

    wprintf(L"Available FEATURE reports:\n");
    valCaps = dev.GetValueCaps(HidP_Feature);
    for (auto& elm : valCaps) {
        wprintf(L"  ReportID: %#04x\n", elm.ReportID);
        wprintf(L"    UsagePage=%#04x, Usage=%#04x\n", elm.UsagePage, elm.NotRange.Usage);
        std::vector<BYTE> report = dev.GetReport(HidP_Feature, elm.ReportID);
        //PrintReport(report);
        ULONG value = dev.GetUsageValue(HidP_Feature, elm, report);
        wprintf(L"    Value=%u\n", value);
    }
    butCaps = dev.GetButtonCaps(HidP_Feature);
    for (UCHAR reportId : hid::Device::UniqueReportIDs<HIDP_BUTTON_CAPS>(butCaps)) {
        wprintf(L"  ReportID: %#04x\n", reportId);
        PrintReport(dev.GetReport(HidP_Feature, reportId));
    }

    wprintf(L"\n");
    return true;
}


void BatteryVisitor(int /*idx*/, HDEVINFO devInfo, SP_DEVINFO_DATA& devInfoData) {
    std::wstring devInstPath = GetDevPropStr(devInfo, devInfoData, &DEVPKEY_Device_InstanceId);

    try {
        AccessBattery(devInstPath, /*verbose*/true);

#if 0
        std::wstring PDOName = GetDevPropStr(devInfo, devInfoData, &DEVPKEY_Device_PDOName); // Physical Device Object
        std::wstring PDOPrefix = L"\\\\?\\GLOBALROOT";
        AccessHidDevice(PDOPrefix + PDOName); // check if it's also a HID device
#endif
    } catch (const std::exception& err) {
        printf("ERROR: Unable to access battery parameters (%s)\n", err.what());
        printf("Continuing battery scan...\n");
    }
}


int wmain(int argc, wchar_t* argv[]) {
    // initialize COM to enable WMI calls
    ComInitialize com(COINIT_APARTMENTTHREADED);

    if (argc < 2) {
        wprintf(L"Querying all batteries:\n");
        EnumerateInterfaces(GUID_DEVICE_BATTERY, BatteryVisitor);
        return 0;
    }

    const wchar_t* instanceId = argv[1]; // 1 is first battery
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

    int res = AccessBattery(instanceId, /*verbose*/true, newCharge); // access battery APIs
#if 0
    AccessHidDevice(pdoPath); // check if it's also a HID device
#endif
    return res;
}
