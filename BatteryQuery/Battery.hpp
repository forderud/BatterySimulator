#pragma once
#include <Windows.h>
#include <poclass.h> // for IOCTL_BATTERY_QUERY_TAG
#include "../simbatt/simbattdriverif.h"
#include <string>
#include <stdexcept>
#include <cassert>


/** Convenience function for getting the battery tag that's needed for some IOCTL calls.
    The battery tag will change if it's removed /reinserted, replaced or if static information like BATTERY_INFORMATION changes. */
ULONG GetBatteryTag(HANDLE device) {
    // get battery tag (needed in later calls)
    ULONG battery_tag = 0;
    ULONG wait = 0;
    DWORD bytes_returned = 0;
    BOOL ok = DeviceIoControl(device, IOCTL_BATTERY_QUERY_TAG, &wait, sizeof(wait), &battery_tag, sizeof(battery_tag), &bytes_returned, nullptr);
    if (!ok) {
        DWORD err = GetLastError();
        wprintf(L"ERROR: IOCTL_BATTERY_QUERY_TAG (err=%u).\n", err);
        return (ULONG)-1;
    }
    return battery_tag;
}

/** Convenience C++ wrapper for BATTERY_STATUS. */
struct BatteryStausWrap : BATTERY_STATUS {
    BatteryStausWrap(HANDLE device = INVALID_HANDLE_VALUE) : BATTERY_STATUS{} {
        if (device != INVALID_HANDLE_VALUE)
            Get(device);
    }

    /** Standard getter. */
    void Get(HANDLE device) {
        // query BATTERY_STATUS status
        BATTERY_WAIT_STATUS wait_status = {};
        wait_status.BatteryTag = GetBatteryTag(device);
        DWORD bytes_returned = 0;
        BOOL ok = DeviceIoControl(device, IOCTL_BATTERY_QUERY_STATUS, &wait_status, sizeof(wait_status), this, sizeof(*this), &bytes_returned, nullptr);
        if (!ok) {
            //DWORD err = GetLastError();
            throw std::runtime_error("IOCTL_BATTERY_QUERY_STATUS error");
        }
    }

    /** SimBatt-specific setter. */
    void Set(HANDLE device) {
        BOOL ok = DeviceIoControl(device, IOCTL_SIMBATT_SET_STATUS, this, sizeof(*this), nullptr, 0, nullptr, nullptr);
        if (!ok) {
            //DWORD err = GetLastError();
            throw std::runtime_error("IOCTL_SIMBATT_SET_STATUS error");
        }
    }

    void Print() {
        wprintf(L"  PowerState=");
        if (PowerState) {
            ULONG pstate = PowerState;
            if (pstate & BATTERY_CHARGING) {
                wprintf(L"| CHARGING ");
                pstate &= ~BATTERY_CHARGING;
            }
            if (pstate & BATTERY_CRITICAL) {
                wprintf(L"| CRITICAL ");
                pstate &= ~BATTERY_CRITICAL;
            }
            if (pstate & BATTERY_DISCHARGING) {
                wprintf(L"| DISCHARGING ");
                pstate &= ~BATTERY_DISCHARGING;
            }
            if (pstate & BATTERY_POWER_ON_LINE) {
                wprintf(L"| POWER_ON_LINE ");
                pstate &= ~BATTERY_POWER_ON_LINE;
            }
            if (pstate) {
                // residual bits
                wprintf(L"| %x ", pstate);
            }
        } else {
            wprintf(L"0");
        }
        wprintf(L"\n");

        if (Capacity != BATTERY_UNKNOWN_CAPACITY)
            wprintf(L"  Capacity=%u mWh\n", Capacity);
        else
            wprintf(L"  Capacity=<unknown>\n");

        if (Voltage != BATTERY_UNKNOWN_VOLTAGE)
            wprintf(L"  Voltage=%u mV\n", Voltage);
        else
            wprintf(L"  Voltage=<unknown>\n");

        if (Rate != BATTERY_UNKNOWN_RATE)
            wprintf(L"  Rate=%i mW\n", Rate);
        else
            wprintf(L"  Rate=<unknown>\n");
    }
};
static_assert(sizeof(BatteryStausWrap) == sizeof(BATTERY_STATUS));


/** Convenience C++ wrapper for BATTERY_INFORMATION. */
struct BatteryInformationWrap : BATTERY_INFORMATION {
    BatteryInformationWrap(HANDLE device = INVALID_HANDLE_VALUE) : BATTERY_INFORMATION{} {
        if (device != INVALID_HANDLE_VALUE)
            Get(device);
    }

    /** Standard getter. */
    void Get(HANDLE device) {
        BATTERY_QUERY_INFORMATION bqi = {};
        bqi.InformationLevel = BatteryInformation;
        bqi.BatteryTag = GetBatteryTag(device);
        DWORD bytes_returned = 0;
        BOOL ok = DeviceIoControl(device, IOCTL_BATTERY_QUERY_INFORMATION, &bqi, sizeof(bqi), this, sizeof(*this), &bytes_returned, nullptr);
        if (!ok) {
            //DWORD err = GetLastError();
            throw std::runtime_error("IOCTL_BATTERY_QUERY_INFORMATION error");
        }
    }

    /** SimBatt-specific setter. */
    void Set(HANDLE device) {
        BOOL ok = DeviceIoControl(device, IOCTL_SIMBATT_SET_INFORMATION, this, sizeof(*this), nullptr, 0, nullptr, nullptr);
        if (!ok) {
            //DWORD err = GetLastError();
            throw std::runtime_error("IOCTL_SIMBATT_SET_INFORMATION error");
        }
    }

    void Print() {
        wprintf(L"  Capabilities=");
        if (Capabilities) {
            ULONG cap = Capabilities;
            if (cap & BATTERY_CAPACITY_RELATIVE) {
                wprintf(L"| RELATIVE ");
                cap &= ~BATTERY_CAPACITY_RELATIVE;
            }
            if (cap & BATTERY_IS_SHORT_TERM) {
                wprintf(L"| IS_SHORT_TERM ");
                cap &= ~BATTERY_IS_SHORT_TERM;
            }
            if (cap & BATTERY_SET_CHARGE_SUPPORTED) {
                wprintf(L"| SET_CHARGE_SUPPORTED ");
                cap &= ~BATTERY_SET_CHARGE_SUPPORTED;
            }
            if (cap & BATTERY_SET_DISCHARGE_SUPPORTED) {
                wprintf(L"| SET_DISCHARGE_SUPPORTED ");
                cap &= ~BATTERY_SET_DISCHARGE_SUPPORTED;
            }
            if (cap & BATTERY_SYSTEM_BATTERY) {
                wprintf(L"| SYSTEM_BATTERY ");
                cap &= ~BATTERY_SYSTEM_BATTERY;
            }
            if (cap) {
                // residual bits
                wprintf(L"| %x ", cap);
            }
        }
        else {
            wprintf(L"0");
        }
        wprintf(L"\n");

        if (Technology == 0)
            wprintf(L"  Technology=Nonrechargeable\n");
        else if (Technology == 1)
            wprintf(L"  Technology=Rechargeable\n");
        else
            wprintf(L"  Technology=<unknown>\n");

        wprintf(L"  Chemistry=%hs\n", std::string((char*)Chemistry, 4).c_str()); // not null-terminated

        std::wstring unit = L" mWh";
        if (Capabilities & BATTERY_CAPACITY_RELATIVE)
            unit = L"";

        wprintf(L"  DesignedCapacity=%u%s\n", DesignedCapacity, unit.c_str());
        wprintf(L"  FullChargedCapacity=%u%s\n", FullChargedCapacity, unit.c_str());
        wprintf(L"  DefaultAlert1=%u%s\n", DefaultAlert1, unit.c_str());
        wprintf(L"  DefaultAlert2=%u%s\n", DefaultAlert2, unit.c_str());
        wprintf(L"  CriticalBias=%u%s\n", CriticalBias, unit.c_str());
        wprintf(L"  CycleCount=%u\n", CycleCount);
    }
};
static_assert(sizeof(BatteryInformationWrap) == sizeof(BATTERY_INFORMATION));


std::wstring GetBatteryInfoStr(HANDLE device, BATTERY_QUERY_INFORMATION_LEVEL level) {
    assert((level == BatteryDeviceName) || (level == BatteryManufactureName) || (level == BatterySerialNumber) || (level == BatteryUniqueID));
    BATTERY_QUERY_INFORMATION bqi = {};
    bqi.InformationLevel = level;
    bqi.BatteryTag = GetBatteryTag(device);

    wchar_t buffer[1024] = {}; // null terminated
    DWORD bytes_returned = 0;
    BOOL ok = DeviceIoControl(device, IOCTL_BATTERY_QUERY_INFORMATION, &bqi, sizeof(bqi), buffer, sizeof(buffer), &bytes_returned, nullptr);
    if (!ok) {
        //DWORD err = GetLastError();
        throw std::runtime_error("IOCTL_BATTERY_QUERY_INFORMATION error");
    }

    return std::wstring(buffer);
}

bool GetBatteryInfoUlong(HANDLE device, BATTERY_QUERY_INFORMATION_LEVEL level, ULONG& value) {
    assert((level == BatteryEstimatedTime) || (level == BatteryTemperature));
    BATTERY_QUERY_INFORMATION bqi = {};
    bqi.InformationLevel = level;
    bqi.BatteryTag = GetBatteryTag(device);

    DWORD bytes_returned = 0;
    BOOL ok = DeviceIoControl(device, IOCTL_BATTERY_QUERY_INFORMATION, &bqi, sizeof(bqi), &value, sizeof(value), &bytes_returned, nullptr);
    return ok;
}

bool GetBatteryInfoDate(HANDLE device, BATTERY_MANUFACTURE_DATE& date) {
    BATTERY_QUERY_INFORMATION bqi = {};
    bqi.InformationLevel = BatteryManufactureDate;
    bqi.BatteryTag = GetBatteryTag(device);

    DWORD bytes_returned = 0;
    BOOL ok = DeviceIoControl(device, IOCTL_BATTERY_QUERY_INFORMATION, &bqi, sizeof(bqi), &date, sizeof(date), &bytes_returned, nullptr);
    return ok;
}

unsigned int GetBatteryInfoGranularity(HANDLE device, BATTERY_REPORTING_SCALE scale[4]) {
    BATTERY_QUERY_INFORMATION bqi = {};
    bqi.InformationLevel = BatteryGranularityInformation;
    bqi.BatteryTag = GetBatteryTag(device);

    DWORD bytes_returned = 0;
    BOOL ok = DeviceIoControl(device, IOCTL_BATTERY_QUERY_INFORMATION, &bqi, sizeof(bqi), scale, 4*sizeof(BATTERY_REPORTING_SCALE), &bytes_returned, nullptr);
    ok;
    return bytes_returned/sizeof(BATTERY_REPORTING_SCALE);
}
