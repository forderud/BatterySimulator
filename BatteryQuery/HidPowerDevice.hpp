#pragma once
#include "HID.hpp"


namespace hid {

/** HID Power Device (https://www.usb.org/sites/default/files/pdcv11.pdf) parser.
    Parses standard parameters that are not already parsed by the Windows HidBatt driver.
    Please delete this class if the HidBatt driver is either updated (requests: https://aka.ms/AAu4w9g , https://aka.ms/AAu4w8p),
    or another driver-based alternative becomes available. */
class HidPowerDevice : public Device {
public:
    HidPowerDevice(const wchar_t* deviceName, bool verbose) : Device(deviceName, verbose) {
        if (IsValid())
            m_value_caps = GetValueCaps(HidP_Feature);
    }

    ~HidPowerDevice() {
    }

    bool IsValid() const {
        if (!Device::IsValid())
            return false;

        // HID power device check
        if (caps.UsagePage != 0x84) // Power Device
            return false;
        if (caps.Usage != 0x04) // UPS
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
