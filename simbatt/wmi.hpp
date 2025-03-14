#pragma once
#include "battery.hpp"

void RegisterWMI(WDFDEVICE Device);
void UnregisterWMI(WDFDEVICE Device);


WMI_QUERY_REGINFO_CALLBACK QueryWmiRegInfo;
WMI_QUERY_DATABLOCK_CALLBACK QueryWmiDataBlock;
