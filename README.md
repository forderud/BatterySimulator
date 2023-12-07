Mock driver to simulate multi-battery scenarios and failure modes on Windows. Based on the Microsoft [Simulated Battery Driver Sample](https://github.com/microsoft/Windows-driver-samples/tree/main/simbatt) sample with minor modifications.

Tutorial:
* [Writing Battery Miniclass Drivers](https://learn.microsoft.com/en-us/windows-hardware/drivers/battery/writing-battery-miniclass-drivers)

## Test instructions
Commands:
* Run `INSTALL.bat` with admin privileges to install the driver and create a simulated battery pack. Run the script multiple times to create additional battery packs.
* Run `UNINSTALL.bat` with admin privileges to uninstall the driver and delete simulated battery packs.

## Registry flags
`HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\ROOT\BATTERY\<DeviceNumber>\Device Parameters`

## simbatt modifications
Changes to the Microsoft `simbatt` sample:
* Switch to default output folder.
* Disable faulty read-back of state from registry through `GetSimBattStateFromRegistry` function.
