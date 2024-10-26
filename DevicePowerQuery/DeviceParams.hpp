#pragma once


static std::wstring GetDevRegPropStr(HDEVINFO hDevInfo, SP_DEVINFO_DATA& devInfo, DWORD property) {
    DWORD requiredSize = 0;
    BOOL ok = SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfo, property, nullptr, nullptr, 0, &requiredSize);
    if (!ok) {
        DWORD res = GetLastError(); res;
        if (res != ERROR_INSUFFICIENT_BUFFER)
            return {};
    }

    DWORD dataType = 0;
    std::wstring result(requiredSize/sizeof(wchar_t), L'\0');
    ok = SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfo, property, &dataType, (BYTE*)result.data(), (DWORD)result.size()*sizeof(wchar_t), &requiredSize);
    if (!ok) {
        DWORD res = GetLastError(); res;
        return {};
    }

    if (dataType == REG_SZ) {
        // single string
        result.resize(result.size()-1); // exclude null-termination
    } else if (dataType == REG_MULTI_SZ) {
        // multiple zero-terminated strings
        size_t len = wcslen(result.c_str());
        result.resize(len); // return first string
    } else {
        assert(false);
    }

    return result;
}

static std::wstring GetDevPropStr(HDEVINFO hDevInfo, SP_DEVINFO_DATA& devInfo, const DEVPROPKEY* property) {
    DWORD requiredSize = 0;
    DEVPROPTYPE propertyType = 0;
    BOOL ok = SetupDiGetDevicePropertyW(hDevInfo, &devInfo, property, &propertyType, nullptr, 0, &requiredSize, 0);
    if (!ok) {
        DWORD res = GetLastError();
        if (res != ERROR_INSUFFICIENT_BUFFER)
            return {};
    }

    std::wstring result(requiredSize/sizeof(wchar_t), L'\0');
    ok = SetupDiGetDevicePropertyW(hDevInfo, &devInfo, property, &propertyType, (BYTE*)result.data(), (DWORD)result.size()*sizeof(wchar_t), &requiredSize, 0);
    if (!ok) {
        DWORD res = GetLastError(); res;
        return {};
    }
    assert(propertyType == DEVPROP_TYPE_STRING);

    // single string
    result.resize(result.size() - 1); // exclude null-termination
    return result;
}
