#pragma once
#include <initguid.h>

//
// Simulated battery ioctl interface
//

// {DAD1F940-CDD0-461f-B23F-C2C663D6E9EB}
DEFINE_GUID(SIMBATT_DEVINTERFACE_GUID,
    0xdad1f940, 0xcdd0, 0x461f, 0xb2, 0x3f, 0xc2, 0xc6, 0x63, 0xd6, 0xe9, 0xeb);

#define SIMBATT_IOCTL(_index_) \
    CTL_CODE(FILE_DEVICE_BATTERY, _index_, METHOD_BUFFERED, FILE_WRITE_DATA)

#define IOCTL_SIMBATT_SET_STATUS                        SIMBATT_IOCTL(0x800)
#define IOCTL_SIMBATT_SET_INFORMATION                   SIMBATT_IOCTL(0x804)
#define SIMBATT_RATE_CALCULATE                          0x7fffffff
