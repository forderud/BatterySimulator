#pragma once


static std::wstring GetDevRegPropStr(HDEVINFO hDevInfo, SP_DEVINFO_DATA& devInfo, DWORD property) {
    DWORD requiredSize = 0;
    BOOL ok = SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfo, property, nullptr, nullptr, 0, &requiredSize);
    if (!ok) {
        DWORD err = GetLastError();
        if (err != ERROR_INSUFFICIENT_BUFFER)
            return {};
    }

    DWORD dataType = 0;
    std::wstring result(requiredSize/sizeof(wchar_t), L'\0');
    ok = SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfo, property, &dataType, (BYTE*)result.data(), (DWORD)result.size()*sizeof(wchar_t), &requiredSize);
    if (!ok) {
        DWORD err = GetLastError(); err;
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
        DWORD err = GetLastError();
        if (err != ERROR_INSUFFICIENT_BUFFER)
            return {};
    }

    std::wstring result(requiredSize/sizeof(wchar_t), L'\0');
    ok = SetupDiGetDevicePropertyW(hDevInfo, &devInfo, property, &propertyType, (BYTE*)result.data(), (DWORD)result.size()*sizeof(wchar_t), &requiredSize, 0);
    if (!ok) {
        DWORD err = GetLastError(); err;
        return {};
    }
    assert(propertyType == DEVPROP_TYPE_STRING);

    // single string
    result.resize(result.size() - 1); // exclude null-termination
    return result;
}

static std::wstring GetDeviceInterfacePath(HDEVINFO devInfo, SP_DEVICE_INTERFACE_DATA& interfaceData) {
    DWORD detailDataSize = 0;
    BOOL ok = SetupDiGetDeviceInterfaceDetailW(devInfo, &interfaceData, nullptr, 0, &detailDataSize, nullptr);
    if (!ok) {
        DWORD err = GetLastError();
        assert(err == ERROR_INSUFFICIENT_BUFFER); err;
    }

    std::unique_ptr<SP_DEVICE_INTERFACE_DETAIL_DATA_W, decltype(&free)> detailData{ static_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(malloc(detailDataSize)), &free };
    detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

    ok = SetupDiGetDeviceInterfaceDetailW(devInfo, &interfaceData, detailData.get(), detailDataSize, &detailDataSize, nullptr);
    assert(ok);

    return detailData->DevicePath; // can be passsed to CreateFile
}
