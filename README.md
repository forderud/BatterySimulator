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
* Copy `BatteryConfig.exe` and the `simbatt` folder to the target machine.
* Run `INSTALL.bat` with admin privileges to install the drivers. Run the script multiple times to simulate additional batteries.
* Run `BatteryConfig.exe <N>` where `<N>` is the simulated battery index to modify the battery charge level. The script will decrease the charge by 10% for each run while alternating between charge and discharge mode. Example: `BatteryConfig.exe 0`.
* Run `INSTALL.bat` with admin privileges to uninstall the driver and delete simulated batteries.

Example Windows 10 screenshot of a simulated 6-battery pack setup:  
![image](https://github.com/forderud/BatterySimulator/assets/2671400/fce5172f-8125-495b-ab06-864e079c19c7)

## Implementation details
Registry flags used: `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\ROOT\BATTERY\<DeviceNumber>\Device Parameters`

## simbatt modifications
Changes to the Microsoft [`simbatt`](https://github.com/microsoft/Windows-driver-samples/tree/main/simbatt) sample:
* Switch to default output folder.
* Disable faulty read-back of state from registry through `GetSimBattStateFromRegistry` function.
* Remove `simbatt-wdfcoinstall.inx` that's no longer needed since Windows 10.
* Clear BATTERY_INFORMATION `BATTERY_CAPACITY_RELATIVE` field to enable per-battery charge display in Windows Settings.
