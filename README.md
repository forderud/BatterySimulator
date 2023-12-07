Mock driver to simulate multi-battery scenarios and failure modes on Windows. Based on the Microsoft [Simulated Battery Driver Sample](https://github.com/microsoft/Windows-driver-samples/tree/main/simbatt) sample with minor modifications.

Tutorial:
* [Writing Battery Miniclass Drivers](https://learn.microsoft.com/en-us/windows-hardware/drivers/battery/writing-battery-miniclass-drivers)

## How to test
It's recommended to **test in a disposable virtual machine (VM)** during development, since faulty drivers might crash or corrupt the computer. You can use the "checkpoint" feature to roll back the machine to a known good state in case of driver installations problems.

### Prerequisites
Prerequisites for the _host_ computer:
* Install [Visual Studio](https://visualstudio.microsoft.com/).
* Install [Windows Driver Kit (WDK)](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk).

Prerequisites for the _target_ computer:
* Disable Secure Boot in UEFI/BIOS.
* Enable test-signed drivers: [`bcdedit /set testsigning on`](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/the-testsigning-boot-configuration-option)
* Enable kernel debugging: `bcdedit /debug on` (optional)

### Test instruction
* Build solution in Visual Studio.
* Copy the `BatteryConfig.exe` and `simbatt` folder to the target machine.
* Run `INSTALL.bat` with admin privileges to install the drivers. Run the script multiple times to simulate additional batteries.
* Run `BatteryConfig.exe <N>` where `<N>` is the simulated battery index to modify the battery charge level. Example: BatteryConfig.exe 0`.
* Run `INSTALL.bat` with admin privileges to uninstall the driver and delete simulated batteries.

## Implementation details
Registry flags used: `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\ROOT\BATTERY\<DeviceNumber>\Device Parameters`

## simbatt modifications
Changes to the Microsoft `simbatt` sample:
* Switch to default output folder.
* Disable faulty read-back of state from registry through `GetSimBattStateFromRegistry` function.
