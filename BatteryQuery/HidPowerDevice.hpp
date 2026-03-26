#pragma once
#include <atlbase.h>
#include "HID.hpp"


/** Values >=22000 mean Windows 11. */
int WindowsBuildNumber() {
    CRegKey reg;
    LSTATUS res = reg.Open(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", KEY_READ);
    assert(res == ERROR_SUCCESS);

    wchar_t buildStr[128] = {};
    ULONG charCount = std::size(buildStr);
    res = reg.QueryStringValue(L"CurrentBuild", buildStr, &charCount);
    assert(res == ERROR_SUCCESS);

    int buildNum = _wtoi(buildStr);
    return buildNum;
}

namespace hid {

/** HID Power Device (https://www.usb.org/sites/default/files/pdcv11.pdf) parser.
    Parses standard parameters that are not already parsed by the Windows HidBatt driver in old Windows versions.
    The class is no longer needed in Windows 11 builds >= 29550.1000. */
class HidPowerDevice : public Device {
public:
    HidPowerDevice(const wchar_t* deviceName, bool verbose) : Device(deviceName, verbose) {
        if (IsValid())
            m_value_caps = GetValueCaps(HidP_Feature);
    }

    ~HidPowerDevice() {
    }

    bool IsValid() const {
        if (WindowsBuildNumber() >= 29550)
            return false; // Inbuilt HidBatt driver has been improved. Class therefore no longer needed.

        if (!Device::IsValid())
            return false;

        // HID power device check
        if (m_caps.UsagePage != 0x84) // Power Device
            return false;
        if (m_caps.Usage != 0x04) // UPS
            return false;

        return true; // we have a match
    }

    /** Get CycleCount with UsagePage=x85 (Battery System) and UsageID=0x6B. */
    ULONG GetCycleCount() const {
        for (const HIDP_VALUE_CAPS& elm : m_value_caps) {
            if (elm.UsagePage != 0x85) // Battery System
                continue;
            if (elm.NotRange.Usage != 0x6B) // CycleCount
                continue;

            std::vector<BYTE> report = GetReport(HidP_Feature, elm.ReportID);
            return GetUsageValue(HidP_Feature, elm, report);
        }

        return 0; // not found
    }

    /** Get BatteryTemperature with UsagePage=x84 (Power Device) and UsageID=0x36.
        Unit: 10ths of a degree Kelvin (https://learn.microsoft.com/en-us/windows/win32/power/ioctl-battery-query-information). */
    ULONG GetTemperature() const {
        for (const HIDP_VALUE_CAPS& elm : m_value_caps) {
            if (elm.UsagePage != 0x84) // Power Device
                continue;
            if (elm.NotRange.Usage != 0x36) // Temperature
                continue;

            std::vector<BYTE> report = GetReport(HidP_Feature, elm.ReportID);
            ULONG hidTemp = GetUsageValue(HidP_Feature, elm, report); // degrees Kelvin
            return 10 * hidTemp; // convert to 10ths of a degree Kelvin
        }

        return 0; // not found
    }

private:
    std::vector<HIDP_VALUE_CAPS> m_value_caps;

};

} // namespace hid
