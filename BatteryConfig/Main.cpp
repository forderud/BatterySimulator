#define INITGUID
#include <iostream>
#include <Windows.h>
#include <Cfgmgr32.h>
#pragma comment(lib, "Cfgmgr32.lib")
#include <Devpropdef.h>
#include <Devpkey.h> // for DEVPKEY_Device_PDOName
#include <wrl/wrappers/corewrappers.h>
#include <vector>

#include "../simbatt/simbattdriverif.h"
#include <poclass.h>

using FileHandle = Microsoft::WRL::Wrappers::FileHandle;


/** Get the virtual file physical device object (PDO) path of a device driver instance. */
static std::wstring GetPDOPath(wchar_t* deviceInstancePath) {
    DEVINST dnDevInst = 0;
    CONFIGRET res = CM_Locate_DevNodeW(&dnDevInst, deviceInstancePath, CM_LOCATE_DEVNODE_NORMAL);
    if (res != CR_SUCCESS) {
        printf("ERROR: CM_Locate_DevNodeW (res=%i).\n", res);
        return {};
    }

    DEVPROPTYPE PropertyType = 0;
    std::vector<BYTE> buffer(1024, 0);
    ULONG buffer_size = (ULONG)buffer.size();
    // DEVPKEY_Device_PDOName matches CM_DRP_PHYSICAL_DEVICE_OBJECT_NAME and SPDRP_PHYSICAL_DEVICE_OBJECT_NAME
    res = CM_Get_DevNode_PropertyW(dnDevInst, &DEVPKEY_Device_PDOName, &PropertyType, buffer.data(), &buffer_size, 0);
    if (res != CR_SUCCESS) {
        printf("ERROR: CM_Get_DevNode_PropertyW (res=%i).\n", res);
        return {};
    }
    buffer.resize(buffer_size);

    const std::wstring prefix = L"\\\\?\\Global\\GLOBALROOT";
    std::wstring fileName = prefix + reinterpret_cast<wchar_t*>(buffer.data()); // append PDO name
    return fileName;
}


int main() {
    wchar_t deviceInstancePath[] = L"ROOT\\BATTERY\\0000"; // first simulated battery
    std::wstring fileName = GetPDOPath(deviceInstancePath);
    if (fileName.empty())
        return -1;

    FileHandle battery(CreateFileW(fileName.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL));
    if (!battery.IsValid()) {
        DWORD err = GetLastError();
        printf("ERROR: CreateFileW (err=%i).\n", err);
        return -1;
    }

    printf("Simulated battery opened...\n");

    BATTERY_STATUS status = {};
    BATTERY_INFORMATION info = {};
    {
        ULONG battery_tag = 0;
        ULONG wait = 0;
        DWORD bytes_returned = 0;
        BOOL ok = DeviceIoControl(battery.Get(), IOCTL_BATTERY_QUERY_TAG, &wait, sizeof(wait), &battery_tag, sizeof(battery_tag), &bytes_returned, nullptr);
        if (!ok) {
            DWORD err = GetLastError();
            printf("ERROR: DeviceIoControl (err=%i).\n", err);
            return -1;
        }

        // query BATTERY_STATUS status 
        BATTERY_WAIT_STATUS wait_status = {};
        wait_status.BatteryTag = battery_tag;
        ok = DeviceIoControl(battery.Get(), IOCTL_BATTERY_QUERY_STATUS, &wait_status, sizeof(wait_status), &status, sizeof(status), &bytes_returned, nullptr);
        if (!ok) {
            DWORD err = GetLastError();
            printf("ERROR: DeviceIoControl (err=%i).\n", err);
            return -1;
        }

        // query BATTERY_INFORMATION info 
        BATTERY_QUERY_INFORMATION bqi = {};
        bqi.InformationLevel = BatteryInformation;
        bqi.BatteryTag = battery_tag;
        ok = DeviceIoControl(battery.Get(), IOCTL_BATTERY_QUERY_INFORMATION, &bqi, sizeof(bqi), &info, sizeof(info), &bytes_returned, nullptr);
        if (!ok) {
            DWORD err = GetLastError();
            printf("ERROR: DeviceIoControl (err=%i).\n", err);
            return -1;
        }
    }

    {
        printf("Battery information:\n");
        printf("  Capabilities=%i\n", info.Capabilities);
        printf("  Chemistry=%s\n", std::string((char*)info.Chemistry, 4).c_str()); // not null-terminated
        printf("  CriticalBias=%i\n", info.CriticalBias);
        printf("  CycleCount=%i\n", info.CycleCount);
        printf("  DefaultAlert1=%i\n", info.DefaultAlert1);
        printf("  DefaultAlert2=%i\n", info.DefaultAlert2);
        printf("  DesignedCapacity=%i\n", info.DesignedCapacity);
        printf("  FullChargedCapacity=%i\n", info.FullChargedCapacity);
        printf("  Technology=%i\n", info.Technology);

        printf("Battery status:\n");
        printf("  Capacity=%i\n", status.Capacity);
        printf("  PowerState=%i\n", status.PowerState);
        printf("  Rate=%i\n", status.Rate);
        printf("  Voltage=%i\n", status.Voltage);
    }

    // Send IOCTL calls to battery driver
#if 0
    status.PowerState = BATTERY_CHARGING; // was 0;
    status.Capacity = 50; // "gas gauge" display by dividing it by FullChargedCapacity
    //status.Rate = 0;
    //status.Voltage = BATTERY_UNKNOWN_VOLTAGE;
    BOOL ok = DeviceIoControl(battery.Get(), IOCTL_SIMBATT_SET_STATUS, &status, sizeof(status), nullptr, 0, nullptr, nullptr);
    if (!ok) {
        DWORD err = GetLastError();
        printf("ERROR: DeviceIoControl (err=%i).\n", err);
        return -1;
    }
#endif

    return 0;
}
