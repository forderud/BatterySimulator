#pragma once


static std::wstring GetDevRegPropStr(HDEVINFO hDevInfo, SP_DEVINFO_DATA& devInfo, DWORD property) {
    std::wstring result;
    result.resize(128, L'\0');
    DWORD requiredSize = 0;
    DWORD dataType = 0;
    BOOL ok = SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfo, property, &dataType, (BYTE*)result.data(), (DWORD)result.size() * sizeof(wchar_t), &requiredSize);
    if (!ok) {
        DWORD res = GetLastError(); res;
        return {};
    }

    if (dataType == REG_SZ) {
        // single string
        result.resize(requiredSize / sizeof(wchar_t) - 1); // exclude null-termination
    } else if (dataType == REG_MULTI_SZ) {
        // multiple zero-terminated strings
        size_t len = wcslen(result.c_str());
        result.resize(len); // return first string
    }
    else {
        assert(false);
    }

    return result;
}

static std::wstring GetDevPropStr(HDEVINFO hDevInfo, SP_DEVINFO_DATA& devInfo, const DEVPROPKEY* property) {
    std::wstring result;
    result.resize(128, L'\0');
    DWORD requiredSize = 0;
    DEVPROPTYPE propertyType = 0;
    BOOL ok = SetupDiGetDevicePropertyW(hDevInfo, &devInfo, property, &propertyType, (BYTE*)result.data(), (DWORD)result.size() * sizeof(wchar_t), &requiredSize, 0);
    if (!ok) {
        DWORD res = GetLastError(); res;
        return {};
    }
    assert(propertyType == DEVPROP_TYPE_STRING);

    // single string
    result.resize(requiredSize / sizeof(wchar_t) - 1); // exclude null-termination
    return result;
}
