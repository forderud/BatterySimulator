#pragma once
#include <windows.h>
#include <cfgmgr32.h> // for CM_Get_Device_Interface_List
#include <Hidclass.h> // for GUID_DEVINTERFACE_HID
#include <hidsdi.h>
#include <comdef.h>
#include <wrl/wrappers/corewrappers.h> // for FileHandle RAII wrapper

#include <algorithm> // for std::sort
#include <cassert>
#include <string>
#include <vector>

#pragma comment(lib, "hid.lib")
#pragma comment(lib, "mincore.lib")

namespace hid {

/** RAII wrapper of PHIDP_PREPARSED_DATA. */
class PreparsedData {
public:
    PreparsedData() = default;

    PreparsedData(PreparsedData&& obj) noexcept {
        std::swap(report, obj.report); // avoid double delete
    }

    ~PreparsedData() {
        Close();
    }

    BOOLEAN Open(HANDLE hid_dev) {
        assert(!report);
        return HidD_GetPreparsedData(hid_dev, &report);
    }
    void Close() {
        if (report) {
            HidD_FreePreparsedData(report);
            report = nullptr;
        }
    }

    operator PHIDP_PREPARSED_DATA() const {
        return report;
    }

private:
    PHIDP_PREPARSED_DATA report = nullptr; // report descriptor for top-level collection
};

/** Win32 HID device wrapper class.
    Simplifies accss to INPUT, OUTPUT and FEATURE reports, as well as device strings. */
class Device {
public:
    Device() = default;

    Device(const wchar_t* deviceName, bool verbose) : m_devName(deviceName) {
        m_dev.Attach(CreateFileW(deviceName,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL));
        if (!m_dev.IsValid()) {
            DWORD err = GetLastError();
            if (verbose) {
                // ERROR_ACCESS_DENIED (5) expected for keyboard and mouse since Windows opens them for exclusive use (https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/keyboard-and-mouse-hid-client-drivers#important-notes)
                wprintf(L"WARNING: %s (%u) when attempting to open %s\n", _com_error(HRESULT_FROM_WIN32(err)).ErrorMessage(), err, deviceName);
            }

            return;
        }

        if (!HidD_GetAttributes(m_dev.Get(), &m_attr)) {
            DWORD err = GetLastError(); err;
            assert((err == ERROR_NOT_FOUND) || (err == ERROR_INVALID_PARAMETER));
            m_dev.Close(); // invalidate object
            return;
        }

        if (!m_preparsed.Open(m_dev.Get())) {
            // Have never encountered this problem
            m_dev.Close(); // invalidate object
            return;
        }

        if (HidP_GetCaps(m_preparsed, &m_caps) != HIDP_STATUS_SUCCESS) {
            // Have never encountered this problem
            m_dev.Close(); // invalidate object
            return;
        }

#ifndef NDEBUG
        m_Manufacturer = GetManufacturer();
        m_Product = GetProduct();
        m_SerialNumber = GetSerialNumber();
#endif
    }

    bool IsValid() const {
        return m_dev.IsValid();
    }

    operator HANDLE () {
        return m_dev.Get();
    }

    std::wstring GetManufacturer() const {
        wchar_t man_buffer[128] = L""; // max USB length is 126 wchar's
        HidD_GetManufacturerString(m_dev.Get(), man_buffer, (ULONG)std::size(man_buffer)); // ignore errors
        return man_buffer;
    }

    std::wstring GetProduct() const {
        wchar_t prod_buffer[128] = L""; // max USB length is 126 wchar's
        HidD_GetProductString(m_dev.Get(), prod_buffer, (ULONG)std::size(prod_buffer)); // ignore erorrs
        return prod_buffer;
    }

    std::wstring GetSerialNumber() const {
        wchar_t sn_buffer[128] = L""; // max USB length is 126 wchar's
        HidD_GetSerialNumberString(m_dev.Get(), sn_buffer, (ULONG)std::size(sn_buffer)); // ignore erorrs
        return sn_buffer;
    }

    std::wstring GetString(ULONG idx) const {
        wchar_t buffer[128] = L""; // max USB length is 126 wchar's
        BOOLEAN ok = HidD_GetIndexedString(m_dev.Get(), idx, buffer, (ULONG)std::size(buffer));
        assert(ok); ok;
        return buffer;
    }

    /** Get the next INPUT report as byte array with ReportID prefix.
        WARNING: Will fail with error 1 (Incorrect function) if another driver or application is already continuously obtaining HID input reports. */
    std::vector<BYTE> Read () const {
        std::vector<BYTE> report(m_caps.InputReportByteLength, (BYTE)0);

        DWORD bytesRead = 0;
        BOOL ok = ReadFile(m_dev.Get(), report.data(), (DWORD)report.size(), &bytesRead, nullptr);
        if (!ok) {
            DWORD err = GetLastError();
            wprintf(L"ERROR: ReadFile failure (err %u).\n", err);
            assert(ok);
            return {};
        }

        report.resize(bytesRead);
        return report;
    }

    /** Get typed FEATURE or INPUT report with ReportID prefix. */
    template <class REPORT>
    REPORT GetReport(HIDP_REPORT_TYPE type, BYTE reportId = 0) const {
        REPORT report{};
        if (reportId)
            (reinterpret_cast<BYTE*>(&report))[0] = reportId; // set report ID prefix

        BOOLEAN ok = false;
        if (type == HidP_Input) {
            assert(sizeof(report) == m_caps.InputReportByteLength);
            ok = HidD_GetInputReport(m_dev.Get(), &report, sizeof(report));
        }  else if (type == HidP_Feature) {
            assert(sizeof(report) == m_caps.FeatureReportByteLength);
            ok = HidD_GetFeature(m_dev.Get(), &report, sizeof(report));
        } else {
            // there's no HidD_GetOutputReport function
            abort();
        }
        if (!ok) {
            DWORD err = GetLastError();
            wprintf(L"ERROR: HidD_GetFeature or HidD_GetInputReport failure (err %u).\n", err);
            assert(ok);
            return {};
        }

        return report;
    }

    /** Get FEATURE or INPUT report as byte array with ReportID prefix. */
    std::vector<BYTE> GetReport(HIDP_REPORT_TYPE type, BYTE reportId) const {
        std::vector<BYTE> report(1, (BYTE)0);
        report[0] = reportId; // report ID prefix

        BOOLEAN ok = false;
        if (type == HidP_Input) {
            report.resize(m_caps.InputReportByteLength, (BYTE)0);
            ok = HidD_GetInputReport(m_dev.Get(), report.data(), (ULONG)report.size());
        } else if (type == HidP_Feature) {
            report.resize(m_caps.FeatureReportByteLength, (BYTE)0);
            ok = HidD_GetFeature(m_dev.Get(), report.data(), (ULONG)report.size());
        } else {
            // there's no HidD_GetOutputReport function
            abort();
        }
        if (!ok) {
            DWORD err = GetLastError();
            wprintf(L"ERROR: HidD_GetFeature or HidD_GetInputReport failure (err %u).\n", err);
            assert(ok);
            return {};
        }

        return report;
    }

    /** Set FEATURE or OUTPUT report with ReportID prefix. */
    template <class REPORT>
    bool SetReport(HIDP_REPORT_TYPE type, const REPORT& report) {
        BOOLEAN ok = false;
        if (type == HidP_Output) {
            assert(sizeof(report) == m_caps.OutputReportByteLength);
            ok = HidD_SetOutputReport(m_dev.Get(), const_cast<void*>(static_cast<const void*>(&report)), sizeof(report));
        } else if (type == HidP_Feature) {
            assert(sizeof(report) == m_caps.FeatureReportByteLength);
            ok = HidD_SetFeature(m_dev.Get(), const_cast<void*>(static_cast<const void*>(&report)), sizeof(report));
        }
        if (!ok) {
            DWORD err = GetLastError();
            wprintf(L"ERROR: HidD_SetFeature failure (err %u).\n", err);
            assert(ok);
            return {};
        }

        return ok;
    }

    std::vector<HIDP_VALUE_CAPS> GetValueCaps(HIDP_REPORT_TYPE type) const {
        USHORT valueCapsLen = 0;
        if (type == HidP_Input)
            valueCapsLen = m_caps.NumberInputValueCaps;
        else if (type == HidP_Output)
            valueCapsLen = m_caps.NumberOutputValueCaps;
        else if (type == HidP_Feature)
            valueCapsLen = m_caps.NumberFeatureValueCaps;
        if (valueCapsLen == 0)
            return {};

        std::vector<HIDP_VALUE_CAPS> valueCaps(valueCapsLen, HIDP_VALUE_CAPS{});
        NTSTATUS status = HidP_GetValueCaps(type, valueCaps.data(), &valueCapsLen, m_preparsed);
        if (status == HIDP_STATUS_INVALID_PREPARSED_DATA) {
            wprintf(L"WARNING: Invalid preparsed data.\n");
            return {};
        }

        assert(status == HIDP_STATUS_SUCCESS); status;
        return valueCaps;
    }

    std::vector<HIDP_BUTTON_CAPS> GetButtonCaps(HIDP_REPORT_TYPE type) const {
        USHORT buttonCapsLen = 0;
        if (type == HidP_Input)
            buttonCapsLen = m_caps.NumberInputButtonCaps;
        else if (type == HidP_Output)
            buttonCapsLen = m_caps.NumberOutputButtonCaps;
        else if (type == HidP_Feature)
            buttonCapsLen = m_caps.NumberFeatureButtonCaps;
        if (buttonCapsLen == 0)
            return {};

        std::vector<HIDP_BUTTON_CAPS> buttonCaps(buttonCapsLen, HIDP_BUTTON_CAPS{});
        NTSTATUS status = HidP_GetButtonCaps(type, buttonCaps.data(), &buttonCapsLen, m_preparsed);
        if (status == HIDP_STATUS_INVALID_PREPARSED_DATA) {
            wprintf(L"WARNING: Invalid preparsed data.\n");
            return {};
        }

        assert(status == HIDP_STATUS_SUCCESS); status;
        return buttonCaps;
    }

    /** Return a sorted list of unique ReportID values found in the input. */
    template <class CAPS> // CAPS might be HIDP_VALUE_CAPS or HIDP_BUTTON_CAPS
    static std::vector<BYTE> UniqueReportIDs(const std::vector<CAPS> input) {
        std::vector<BYTE> result;
        for (auto& elm : input)
            result.push_back(elm.ReportID);

        // sort and remove duplicates
        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end()), result.end());

        return result;
    }

    /** Get the current value for a given Usage. */
    template <class CAPS> // CAPS might be HIDP_VALUE_CAPS or HIDP_BUTTON_CAPS
    ULONG GetUsageValue(HIDP_REPORT_TYPE type, CAPS caps, const std::vector<BYTE>& report) const {
        ULONG value = 0;
        NTSTATUS status = HidP_GetUsageValue(type, caps.UsagePage, caps.LinkCollection, caps.NotRange.Usage, &value, m_preparsed, (CHAR*)report.data(), (ULONG)report.size());
        assert(status == HIDP_STATUS_SUCCESS); status;
        return value;
    }

    /** Get list of all usages set to ON. */
    std::vector<USAGE> GetUsages(HIDP_REPORT_TYPE type, HIDP_BUTTON_CAPS caps, const std::vector<BYTE>& report) const {
        ULONG usage_count = HidP_MaxUsageListLength(type, caps.UsagePage, m_preparsed);
        std::vector<USAGE> usages(usage_count, 0);
        NTSTATUS res = HidP_GetUsages(type, caps.UsagePage, caps.LinkCollection, usages.data(), &usage_count, m_preparsed, (CHAR*)report.data(), (ULONG)report.size());
        assert(res == HIDP_STATUS_SUCCESS);
        usages.resize(usage_count);
        return usages;
    }

    /** Create a HID report with a given usage value. */
    template <class CAPS> // CAPS might be HIDP_VALUE_CAPS or HIDP_BUTTON_CAPS
    std::vector<BYTE> CreateReportValue(HIDP_REPORT_TYPE type, CAPS caps, ULONG value) const {
        ULONG reportLen = 0;
        if (type == HidP_Input)
            reportLen = m_caps.InputReportByteLength;
        if (type == HidP_Output)
            reportLen = m_caps.OutputReportByteLength;
        else if (type == HidP_Feature)
            reportLen = m_caps.FeatureReportByteLength;
        else
            abort();

        std::vector<BYTE> report(reportLen, (BYTE)0);
        NTSTATUS status = HidP_SetUsageValue(type, caps.UsagePage, caps.LinkCollection, caps.NotRange.Usage, value, m_preparsed, (CHAR*)report.data(), (ULONG)report.size());
        assert(status == HIDP_STATUS_SUCCESS); status;
        return report;
    }

    std::wstring Name() const {
        std::wstring prod = GetProduct();
        if (prod.empty())
            prod = L"<unknown>";
        std::wstring manuf = GetManufacturer();
        if (manuf.empty())
            manuf = L"<unknown>";

        wchar_t vid_pid[] = L"VID_0000&PID_0000";
        swprintf_s(vid_pid, L"VID_%04X&PID_%04X", m_attr.VendorID, m_attr.ProductID);

        std::wstring result = L"\"" + prod + L"\"";
        result += L" by ";
        result += L"\"" + manuf + L"\"";
        result += L" (";
        result += vid_pid;
        result += L")";
        return result;
    }

    void PrintInfo() const {
        wprintf(L"Device capabilities:\n");
        wprintf(L"  Usage=0x%04X, UsagePage=0x%04X\n", m_caps.Usage, m_caps.UsagePage);
        wprintf(L"  InputReportByteLength=%u, OutputReportByteLength=%u, FeatureReportByteLength=%u, NumberLinkCollectionNodes=%u\n", m_caps.InputReportByteLength, m_caps.OutputReportByteLength, m_caps.FeatureReportByteLength, m_caps.NumberLinkCollectionNodes);
        wprintf(L"  NumberInputButtonCaps=%u, NumberInputValueCaps=%u, NumberInputDataIndices=%u\n", m_caps.NumberInputButtonCaps, m_caps.NumberInputValueCaps, m_caps.NumberInputDataIndices);
        wprintf(L"  NumberOutputButtonCaps=%u, NumberOutputValueCaps=%u, NumberOutputDataIndices=%u\n", m_caps.NumberOutputButtonCaps, m_caps.NumberOutputValueCaps, m_caps.NumberOutputDataIndices);
        wprintf(L"  NumberFeatureButtonCaps=%u, NumberFeatureValueCaps=%u, NumberFeatureDataIndices=%u\n", m_caps.NumberFeatureButtonCaps, m_caps.NumberFeatureValueCaps, m_caps.NumberFeatureDataIndices);
    }

private:
    std::wstring                         m_devName;
    Microsoft::WRL::Wrappers::FileHandle m_dev;
public:
    HIDD_ATTRIBUTES                      m_attr = {}; // VendorID, ProductID, VersionNumber
private:
    PreparsedData                        m_preparsed; // opaque ptr
public:
    HIDP_CAPS                            m_caps = {}; // Usage, UsagePage, report sizes

#ifndef NDEBUG
    // fields to aid debugging
    std::wstring                         m_Manufacturer;
    std::wstring                         m_Product;
    std::wstring                         m_SerialNumber;
#endif
};

/** Human Interface Devices (HID) device search class. */
class Scan {
public:
    struct Criterion {
        USHORT VendorID = 0;
        USHORT ProductID = 0;
        USHORT Usage = 0;
        USHORT UsagePage = 0;
    };

    static std::vector<Device> FindDevices (const Criterion& crit, bool verbose) {
        const ULONG searchScope = CM_GET_DEVICE_INTERFACE_LIST_PRESENT; // only currently 'live' device interfaces

        ULONG deviceInterfaceListLength = 0;
        CONFIGRET cr = CM_Get_Device_Interface_List_SizeW(&deviceInterfaceListLength, (GUID*)&GUID_DEVINTERFACE_HID, NULL, searchScope);
        assert(cr == CR_SUCCESS);

        // symbolic link name of interface instances
        std::wstring deviceInterfaceList(deviceInterfaceListLength, L'\0');
        cr = CM_Get_Device_Interface_ListW((GUID*)&GUID_DEVINTERFACE_HID, NULL, deviceInterfaceList.data(), deviceInterfaceListLength, searchScope);
        assert(cr == CR_SUCCESS);

        std::vector<Device> results;
        for (const wchar_t * currentInterface = deviceInterfaceList.c_str(); *currentInterface; currentInterface += wcslen(currentInterface) + 1) {
            Device dev(currentInterface, verbose);

            if (CheckDevice(dev, crit))
                results.push_back(std::move(dev));
        }

        return results;
    }

private:
    static bool CheckDevice(Device & dev, const Criterion& crit) {
        if (!dev.IsValid())
            return false;

        if (crit.VendorID) {
            if (crit.VendorID != dev.m_attr.VendorID)
                return false;
        }
        if (crit.ProductID) {
            if (crit.ProductID != dev.m_attr.ProductID)
                return false;
        }

        if (crit.Usage) {
            if (crit.Usage != dev.m_caps.Usage)
                return false;
        }
        if (crit.UsagePage) {
            if (crit.UsagePage != dev.m_caps.UsagePage)
                return false;
        }

        // found matching device
        return true;
    }
};

} // namespace hid
