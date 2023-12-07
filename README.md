Mock driver to simulate multi-battery scenarios and failure modes on Windows. Based on the Microsoft [Simulated Battery Driver Sample](https://github.com/microsoft/Windows-driver-samples/tree/main/simbatt) sample with some modifications to ease multi-battery testing and failure handling without requiring physical battery packs.

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
You can with the driver **make Windows believe that it’s being powered by one or more battery packs** – even if running from a AC-powered VM.

Steps:
* Build solution in Visual Studio.
* Copy `BatteryConfig.exe` and the `simbatt` folder to the target machine.
* Run `INSTALL.bat` with admin privileges to install the drivers. Run the script multiple times to simulate additional batteries.
* Run `BatteryConfig.exe <N> <Charge>`, where `<N>` is the simulated battery index and `<Charge>` is the new charge level, to modify the battery state. Example: `BatteryConfig.exe 0 90` to set the charge level of the first battery to 90%.
* Run `UNINSTALL.bat` with admin privileges to uninstall the driver and delete simulated batteries.

### Examples
Simulate 6 battery-packs:  
![image](https://github.com/forderud/BatterySimulator/assets/2671400/fce5172f-8125-495b-ab06-864e079c19c7)

Warning displayed when simulating low-battery conditions:  
![image](https://github.com/forderud/BatterySimulator/assets/2671400/80707d03-8ffc-4209-bfff-8bfaa1c4181c)

Windows handling of low-battery situations can be configured through "Power Options" (or using [Powercfg](https://learn.microsoft.com/en-us/windows-hardware/design/device-experiences/powercfg-command-line-options)):  
![image](https://github.com/forderud/BatterySimulator/assets/2671400/c98a64a4-1c29-43d8-9376-3feca6ce1130)

## Implementation details
This section is only intended for internal bookkeeping. Testers doesn't need to care about this.

### Persistent state
Registry flags used: `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\ROOT\BATTERY\<DeviceNumber>\Device Parameters`

### simbatt modifications
Changes to the Microsoft [`simbatt`](https://github.com/microsoft/Windows-driver-samples/tree/main/simbatt) sample:
* Switch to default output folder.
* Disable faulty read-back of state from registry through `GetSimBattStateFromRegistry` function.
* Remove `simbatt-wdfcoinstall.inx` that's no longer needed since Windows 10.
* Clear BATTERY_INFORMATION `BATTERY_CAPACITY_RELATIVE` field to enable per-battery charge display in Windows Settings.
