#pragma once
#include <Cfgmgr32.h>
#pragma comment(lib, "Cfgmgr32.lib")
#include <Devpkey.h> // for DEVPKEY_Device_PDOName
#include <variant>
#include <vector>


class DeviceInstance {
public:
    DeviceInstance(const wchar_t* instanceId) {
        // device instance prefix search order
        // try fake batteries first with fallback to real batteries
        static const wchar_t* InstancePathPrefix[] = {
            L"SWD\\DEVGEN\\%s",   // fake DevGen SW battery (disappears on reboot)
            L"ROOT\\DEVGEN\\%s",  // fake DevGen "HW" battery (persists across reboots)
            L"ACPI\\PNP0C0A\\%s", // real ACPI compliant control method battery
        };

        for (size_t i = 0; i < std::size(InstancePathPrefix); ++i) {
            wchar_t deviceInstancePath[32] = {};
            swprintf_s(deviceInstancePath, InstancePathPrefix[i], instanceId); // device instance ID

            // try to open device
            CONFIGRET res = CM_Locate_DevNodeW(&m_devInst, deviceInstancePath, CM_LOCATE_DEVNODE_NORMAL);
            if (res == CR_SUCCESS) {
                wprintf(L"DeviceInstancePath: %s\n", deviceInstancePath);
                return; // found a match
            }
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
