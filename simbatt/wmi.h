#pragma once
#include "simbatt.h"

void InitializeWMI(WDFDEVICE Device);


WMI_QUERY_REGINFO_CALLBACK SimBattQueryWmiRegInfo;
WMI_QUERY_DATABLOCK_CALLBACK SimBattQueryWmiDataBlock;
