#pragma once
#include <cassert>
#include <Windows.h>
#include <atlbase.h>
#include <Wbemidl.h>

#pragma comment(lib, "wbemuuid.lib") // CLSID_WbemLocator


/** WMI-centered COM HRESULT checker. */
static void CHECK(HRESULT hr) {
    if (SUCCEEDED(hr))
        return;

    // try to get error details
    CComPtr<IErrorInfo> errInfo;
    GetErrorInfo(0, &errInfo); // might succeed but still return a nullptr
    if (errInfo) {
        CComBSTR errDesc;
        errInfo->GetDescription(&errDesc);
        if (errDesc.Length() > 0)
            wprintf(L"ERROR DESCRIPTION: %s\n", errDesc.m_str);

        CComBSTR source;
        errInfo->GetSource(&source);
        if (source.Length() > 0)
            wprintf(L"ERROR SOURCE: %s\n", source.m_str);

        CComBSTR helpFile;
        errInfo->GetHelpFile(&helpFile);
        if (helpFile.Length() > 0)
            wprintf(L"ERROR HELPFILE: %s\n", helpFile.m_str);
    }

    if (hr == WBEM_E_INVALID_METHOD_PARAMETERS)
        wprintf(L"ERROR: WBEM_E_INVALID_METHOD_PARAMETERS\n");
    else if (hr == WBEM_E_INVALID_METHOD)
        wprintf(L"ERROR: WBEM_E_INVALID_METHOD\n");
    else if (hr == WBEM_E_ILLEGAL_OPERATION)
        wprintf(L"ERROR: WBEM_E_ILLEGAL_OPERATION\n");
    else if (hr == WBEM_E_TYPE_MISMATCH)
        wprintf(L"ERROR: WBEM_E_TYPE_MISMATCH\n");
    else if (hr == WBEM_E_NOT_FOUND)
        wprintf(L"ERROR: WBEM_E_NOT_FOUND\n");
    else
        wprintf(L"ERROR: COM HRESULT=0x%x\n", hr);

    abort();
}


// The function connects to the namespace specified by the user.
// DOC: https://learn.microsoft.com/en-us/windows/win32/wmisdk/example-creating-a-wmi-application
CComPtr<IWbemServices> ConnectToNamespace(_In_ const wchar_t* chNamespace) {
    CComPtr<IWbemLocator> wbemLocator;
    CHECK(CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&wbemLocator));

    // Using the locator, connect to COM in the given namespace.
    CComPtr<IWbemServices> wbemServices;
    CHECK(wbemLocator->ConnectServer(
        CComBSTR(chNamespace),
        NULL,   // current account
        NULL,   // current password
        0L,     // locale
        0L,     // securityFlags
        NULL,   // authority (domain for NTLM)
        NULL,   // context
        &wbemServices));

    return wbemServices;
}

// The function returns the first WMI instance with matching class name
CComPtr<IWbemClassObject> GetInstanceReference(IWbemServices& pIWbemServices, _In_ const CComBSTR className) {
    // Get Instance Enumerator Interface.
    CComPtr<IEnumWbemClassObject> enumInst;
    HRESULT hr = pIWbemServices.CreateInstanceEnum(
        className,              // Name of the root class.
        WBEM_FLAG_FORWARD_ONLY, // Forward-only enumeration.
        NULL,                   // Context.
        &enumInst);             // pointer to class enumerator

    if (hr != WBEM_S_NO_ERROR || enumInst == NULL) {
        wprintf(L"Error: CreateInstanceEnum failed. This operation require Admin privileges.\n");
        return nullptr;
    }

    // Get pointer to the instance.
    hr = WBEM_S_NO_ERROR;
    while (hr == WBEM_S_NO_ERROR) {
        ULONG count = 0;
        CComPtr<IWbemClassObject> inst;
        hr = enumInst->Next(
            1000,      // one second timeout
            1,         // return just one instance.
            &inst,    // pointer to instance.
            &count); // Number of instances returned.

        if (count > 0)
            return inst;
    }

    return nullptr;
}

/** Dell Data Vault (DDV) parser for the "DDVWmiMethodFunction" WMI interface.
    Parses standard parameters that are not already parsed by the Windows ACPI driver.
    Please delete this class if Dell improves their battery integration. */
class DellBattery {
public:
    DellBattery(const std::wstring& devInstPath) {
        // devInstPath example "ACPI\PNP0C0A\1"
        if (devInstPath.find(L"ACPI\\PNP0C0A") == std::wstring::npos)
            return; // not ACPI compliant control method battery (CmBatt driver)

        size_t tagIdx = devInstPath.find_last_of(L"\\");
        if (tagIdx != std::wstring::npos) {
            std::wstring tagStr = devInstPath.substr(tagIdx + 1);
            m_battery_tag = _wtoi(tagStr.c_str());
        }

        m_wbem = ConnectToNamespace(L"root\\WMI");

        const CComBSTR DellWMIClass = L"DDVWmiMethodFunction";
        if (IsUserAnAdmin())
            m_ddv_inst = GetInstanceReference(*m_wbem, DellWMIClass); // will fail unless running as Admin
        if (m_ddv_inst)
            CHECK(m_wbem->GetObject(DellWMIClass, 0, NULL, &m_ddv_class, NULL));
    }

    ~DellBattery() {
    }

    bool IsValid() const {
        return m_ddv_inst && m_ddv_class;
    }

    ULONG GetCycleCount() {
        // void BatteryCycleCount([in] uint32 arg2, [out] uint32 argr);
        INT cycleCount = CallMethod(L"BatteryCycleCount", m_battery_tag);
        return cycleCount;
    }

    BATTERY_MANUFACTURE_DATE GetManufactureDat() {
        // void BatteryManufactureDate([in] uint32 arg2, [out] uint32 argr);
        USHORT dateEnc = (USHORT)CallMethod(L"BatteryManufactureDate", m_battery_tag);;

        // Date parameter encoding : "(year – 1980)*512 + month*32 + day"
        BATTERY_MANUFACTURE_DATE date{};
        date.Day = dateEnc % 32;
        date.Month = (dateEnc >> 5) % 16;
        date.Year = 1980 + (dateEnc >> 9);
        return date;
    }

    ULONG GetTemperature() {
        // void BatteryTemperature([in] uint32 arg2, [out] uint32 argr);
        auto temp = (USHORT)CallMethod(L"BatteryTemperature", m_battery_tag);;
        return temp; // 10ths of a degree Kelvin
    }

private:
    INT CallMethod(const wchar_t methodName[], INT arg2Val) {
        CComVariant pathVariable;
        //The IWbemClassObject::Get method retrieves the specified property value, if it exists.
        CHECK(m_ddv_inst->Get(_bstr_t(L"__PATH"), 0, &pathVariable, NULL, NULL));
        //wprintf(L"Class Path: %s\n", pathVariable.bstrVal);

        CComPtr<IWbemClassObject> inParams;
        CComPtr<IWbemClassObject> outParams; // TODO: Figure out if parameter is needed
        CHECK(m_ddv_class->GetMethod(_bstr_t(methodName), 0, &inParams, &outParams));

        CComPtr<IWbemClassObject> classInstance;
        CHECK(inParams->SpawnInstance(0, &classInstance));

        // pass "arg2" input argument
        CComVariant arg2;
        arg2 = arg2Val; // VT_I4
        CHECK(classInstance->Put(L"arg2", 0/*reserved*/, &arg2, CIM_UINT32));

        // call method
        CComPtr<IWbemClassObject> callOutParams;
        CComPtr<IWbemCallResult> result;
        CHECK(m_wbem->ExecMethod(pathVariable.bstrVal, _bstr_t(methodName), 0/*synchronous*/, 0/*ctx*/ , classInstance, &callOutParams, &result));

        // retrieve "argr" output argument
        CComVariant argr;
        CHECK(callOutParams->Get(L"argr", 0, &argr, NULL, 0));
        return argr.intVal;
    }


    CComPtr<IWbemServices>    m_wbem;
    ULONG                     m_battery_tag = 0;
    CComPtr<IWbemClassObject> m_ddv_inst;  // CIM class object instances
    CComPtr<IWbemClassObject> m_ddv_class; // CIM class definition
};
