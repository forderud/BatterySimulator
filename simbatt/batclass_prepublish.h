#pragma once
#include <batclass.h>

//-------------------------------------------------- Would be Public Definitions

#ifndef BATTERY_CLASS_MINOR_VERSION_1

typedef struct {
    USHORT                          MajorVersion;
    USHORT                          MinorVersion;

    PVOID                           Context;        // Miniport context

    BCLASS_QUERY_TAG                QueryTag;
    BCLASS_QUERY_INFORMATION        QueryInformation;
    BCLASS_SET_INFORMATION          SetInformation;
    BCLASS_QUERY_STATUS             QueryStatus;
    BCLASS_SET_STATUS_NOTIFY        SetStatusNotify;
    BCLASS_DISABLE_STATUS_NOTIFY    DisableStatusNotify;
    PDEVICE_OBJECT                  Pdo;
    PUNICODE_STRING                 DeviceName;
    PDEVICE_OBJECT                  Fdo;
} BATTERY_MINIPORT_INFO_V1_1, *PBATTERY_MINIPORT_INFO_V1_1;

#define BATTERY_CLASS_MINOR_VERSION_1   0x0001

#endif // BATTERY_CLASS_MINOR_VERSION_1
