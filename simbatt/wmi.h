#pragma once
#include "simbatt.h"

void RegisterWMI(WDFDEVICE Device);
void UnregisterWMI(WDFDEVICE Device);


WMI_QUERY_REGINFO_CALLBACK SimBattQueryWmiRegInfo;
WMI_QUERY_DATABLOCK_CALLBACK SimBattQueryWmiDataBlock;
