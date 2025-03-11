#pragma once
#include "device.hpp"


NTSTATUS InitializeBattery(_In_ WDFDEVICE Device);

_IRQL_requires_same_
void InitializeBatteryState(_In_ WDFDEVICE Device);

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL BattIoDeviceControl;
