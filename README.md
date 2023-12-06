Mock driver to simulate multi-battery scenarios and failure modes on Windows. Based on the Microsoft [Simulated Battery Driver Sample](https://github.com/microsoft/Windows-driver-samples/tree/main/simbatt) sample with minor modifications.

WMI classes:
* [Win32_Battery](https://learn.microsoft.com/en-us/windows/win32/cimwin32prov/win32-battery) WMI class
* [Win32_PortableBattery](https://learn.microsoft.com/en-us/windows/win32/cimwin32prov/win32-portablebattery) WMI class
* [CIM_Battery ](https://learn.microsoft.com/en-us/windows/win32/cimwin32prov/cim-battery) base class

Tutorials:
* [Writing Battery Miniclass Drivers](https://learn.microsoft.com/en-us/windows-hardware/drivers/battery/writing-battery-miniclass-drivers)

## Registry flags
`HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\ROOT\BATTERY\<DeviceNumber>\Device Parameters`

## Query battery status
From PowerShell: `Get-CimInstance -Namespace root\CIMV2 Win32_Battery`
