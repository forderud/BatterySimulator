#pragma once
#include <Cfgmgr32.h>
#pragma comment(lib, "Cfgmgr32.lib")
#include <Devpkey.h> // for DEVPKEY_Device_PDOName
#include <variant>
#include <vector>


class DeviceInstance {
public:
    DeviceInstance(const wchar_t* instanceId) {
        wchar_t deviceInstancePath1[32] = {}; // DevGen SW device (disappears on reboot)
        swprintf_s(deviceInstancePath1, L"SWD\\DEVGEN\\%s", instanceId); // device instance ID

        wchar_t deviceInstancePath2[32] = {}; // DevGen "HW" device (persists across reboots)
        swprintf_s(deviceInstancePath2, L"ROOT\\DEVGEN\\%s", instanceId); // device instance ID

        wchar_t deviceInstancePath3[32] = {}; // Real ACPI battery
        swprintf_s(deviceInstancePath3, L"ACPI\\PNP0C0A\\%s", instanceId); // device instance ID

        // try to open as DevGen SW device first
        CONFIGRET res = CM_Locate_DevNodeW(&m_devInst, deviceInstancePath1, CM_LOCATE_DEVNODE_NORMAL);
        if (res == CR_SUCCESS) {
            wprintf(L"DeviceInstancePath: %s\n", deviceInstancePath1);
            return;
        }

        // fallback to opening as DevGen HW device
        res = CM_Locate_DevNodeW(&m_devInst, deviceInstancePath2, CM_LOCATE_DEVNODE_NORMAL);
        if (res == CR_SUCCESS) {
            wprintf(L"DeviceInstancePath: %s\n", deviceInstancePath2);
            return;
        }

        // fallback to real ACPI battery
        res = CM_Locate_DevNodeW(&m_devInst, deviceInstancePath3, CM_LOCATE_DEVNODE_NORMAL);
        if (res == CR_SUCCESS) {
            wprintf(L"DeviceInstancePath: %s\n", deviceInstancePath3);
            return;
        }

        throw std::runtime_error("ERROR: CM_Locate_DevNodeW");
    }

    ~DeviceInstance() {
    }

    std::wstring GetDriverVersion() const {
        auto res = GetProperty(DEVPKEY_Device_DriverVersion);
        return std::get<std::wstring>(res);
    }

    FILETIME GetDriverDate() const {
        auto res = GetProperty(DEVPKEY_Device_DriverDate);
        return std::get<FILETIME>(res);
    }

    /** Get the virtual file physical device object (PDO) path of a device driver instance. */
    std::wstring GetPDOPath() const {
        auto res = GetProperty(DEVPKEY_Device_PDOName);
        return L"\\\\?\\GLOBALROOT" + std::get<std::wstring>(res); // add prefix before PDO name
    }

    std::variant<std::wstring, FILETIME> GetProperty(const DEVPROPKEY& propertyKey) const {
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
        } else if (propertyType == DEVPROP_TYPE_FILETIME) {
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

private:
    DEVINST m_devInst = 0;
};
