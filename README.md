Mock driver to simulate multi-battery setups and test Windows power management. Based on the Microsoft [Simulated Battery Driver Sample](https://github.com/microsoft/Windows-driver-samples/tree/main/simbatt) sample with some modifications to ease multi-battery testing and failure handling without requiring physical battery packs.

Tutorial: [Writing Battery Miniclass Drivers](https://learn.microsoft.com/en-us/windows-hardware/drivers/battery/writing-battery-miniclass-drivers)

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
* Build solution in Visual Studio or download binaries from [releases](../../releases).
* Copy `BatteryConfig.exe`, `BatteryMonitor.exe` and the `simbatt` folder to the target machine.
* Run `INSTALL.bat` with admin privileges to install the driver with two simulated batteries.
* Run `BatteryConfig.exe <N> <Charge>`, where `<N>` is the simulated battery index and `<Charge>` is the new charge level, to modify the battery state. Example: `BatteryConfig.exe 1 90` to set the charge level of the first battery to 90%.
* Run `BatteryMonitor.exe` to monitor power events broadcasted to all application.
* Run `UNINSTALL.bat` with admin privileges to uninstall the driver and delete simulated batteries.

### Examples
Simulate 6 battery-packs:  
![image](https://github.com/forderud/BatterySimulator/assets/2671400/fce5172f-8125-495b-ab06-864e079c19c7)

Battery status icons that can be simulated:
* Critical level: ![image](https://github.com/forderud/BatterySimulator/assets/2671400/9a6d48aa-3e21-4423-b9ef-753cff2587aa) (7% or less)
* Low level: ![image](https://github.com/forderud/BatterySimulator/assets/2671400/7e03f6c0-222e-4a87-8a33-4aec937ede94) (10% or less)
* Battery saver: ![image](https://github.com/forderud/BatterySimulator/assets/2671400/ef038cbd-33a3-43c8-8e18-531878c59004) (20% or less)
* Half full: ![image](https://github.com/forderud/BatterySimulator/assets/2671400/fdc0fc67-3628-4879-a9ef-9fa2d02feda6)
* On AC power: ![image](https://github.com/forderud/BatterySimulator/assets/2671400/d258e0a8-5876-4ca4-80da-f16367166ce6)

Notification when simulating low-battery conditions:  
![image](https://github.com/forderud/BatterySimulator/assets/2671400/80707d03-8ffc-4209-bfff-8bfaa1c4181c)

### Configuring Windows power management
Windows handling of low-battery situations can either be configured through the "Power Options" UI (accessed from "Power & sleep" settings -> "Additional power settings" -> "Change plan settings" -> "Change advanced power settings"):  
![image](https://github.com/forderud/BatterySimulator/assets/2671400/c98a64a4-1c29-43d8-9376-3feca6ce1130)

... or using [Powercfg](https://learn.microsoft.com/en-us/windows-hardware/design/device-experiences/powercfg-command-line-options):
```
:: Display current power configuration 
PowerCfg.exe /query

:: Select power scheme (SCHEME_MIN, SCHEME_MAX or SCHEME_BALANCED)
set SCHEME=SCHEME_BALANCED

:: Critical battery notification (0=off, 1=on)
PowerCfg.exe /setdcvalueindex %SCHEME% SUB_BATTERY BATFLAGSCRIT 0
PowerCfg.exe /setacvalueindex %SCHEME% SUB_BATTERY BATFLAGSCRIT 0

:: Critical battery action (0=do nothing, 1=sleep, 2=hibernate, 3=shut down)
PowerCfg.exe /setdcvalueindex %SCHEME% SUB_BATTERY BATACTIONCRIT 0
PowerCfg.exe /setacvalueindex %SCHEME% SUB_BATTERY BATACTIONCRIT 0

:: Low battery notification (0=off, 1=on)
PowerCfg.exe /setdcvalueindex %SCHEME% SUB_BATTERY BATFLAGSLOW 0
PowerCfg.exe /setacvalueindex %SCHEME% SUB_BATTERY BATFLAGSLOW 0

:: Low battery action (0=do nothing, 1=sleep, 2=hibernate, 3=shut down)
PowerCfg.exe /setdcvalueindex %SCHEME% SUB_BATTERY BATACTIONLOW 0
PowerCfg.exe /setacvalueindex %SCHEME% SUB_BATTERY BATACTIONLOW 0
```

### WMI `Win32_Battery` parameters
Battery parameters from the battery miniclass driver will automatically be exposed through the [`Win32_Battery`](https://learn.microsoft.com/en-us/windows/win32/cimwin32prov/win32-battery) WMI class, so there's no need for implementing a WMI provider yourself.

Query battery status from PowerShell: `Get-CimInstance -Namespace root\CIMV2 Win32_Battery`

Sample output:
```
PS > Get-CimInstance -Namespace root\CIMV2 Win32_Battery

Caption                     : Internal Battery
Description                 : Internal Battery
InstallDate                 :
Name                        : SimulatedBattery
Status                      : Service
Availability                : 1
ConfigManagerErrorCode      :
ConfigManagerUserConfig     :
CreationClassName           : Win32_Battery
DeviceID                    : SimulatedBattery0000
ErrorCleared                :
ErrorDescription            :
LastErrorCode               :
PNPDeviceID                 :
PowerManagementCapabilities : {1}
PowerManagementSupported    : False
StatusInfo                  :
SystemCreationClassName     : Win32_ComputerSystem
SystemName                  : WINDEV2211EVAL
BatteryStatus               : 6
Chemistry                   : 2
DesignCapacity              :
DesignVoltage               :
EstimatedChargeRemaining    : 90
EstimatedRunTime            : 0
ExpectedLife                :
FullChargeCapacity          :
MaxRechargeTime             :
SmartBatteryVersion         :
TimeOnBattery               :
TimeToFullCharge            :
BatteryRechargeTime         :
ExpectedBatteryLife         :
PSComputerName              :

Caption                     : Internal Battery
Description                 : Internal Battery
InstallDate                 :
Name                        : SimulatedBattery
Status                      : OK
Availability                : 2
ConfigManagerErrorCode      :
ConfigManagerUserConfig     :
CreationClassName           : Win32_Battery
DeviceID                    : SimulatedBattery0000
ErrorCleared                :
ErrorDescription            :
LastErrorCode               :
PNPDeviceID                 :
PowerManagementCapabilities : {1}
PowerManagementSupported    : False
StatusInfo                  :
SystemCreationClassName     : Win32_ComputerSystem
SystemName                  : WINDEV2211EVAL
BatteryStatus               : 2
Chemistry                   : 2
DesignCapacity              :
DesignVoltage               :
EstimatedChargeRemaining    : 100
EstimatedRunTime            : 0
ExpectedLife                :
FullChargeCapacity          :
MaxRechargeTime             :
SmartBatteryVersion         :
TimeOnBattery               :
TimeToFullCharge            :
BatteryRechargeTime         :
ExpectedBatteryLife         :
PSComputerName              :
```

### Windows power events
Windows applications receive [`WM_POWERBROADCAST`](https://learn.microsoft.com/en-us/windows/win32/power/wm-powerbroadcast) events when the machine transitions between AC and battery power, as well as when suspening or resuming from low-power modes. Applications can also call [`GetSystemPowerStatus`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getsystempowerstatus) to retrieve details about the power state and aggregated battery charge level. Take a look at the `BatteryMonitor` project for sample code.  
![image](https://github.com/forderud/BatterySimulator/assets/2671400/622a8e92-8535-46ce-85b9-72d4fd52b798)

## Implementation details
This section is only intended for internal bookkeeping. Testers doesn't need to care about this.

### Persistent state
Registry flags used: `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\ROOT\BATTERY\<DeviceNumber>\Device Parameters`

### simbatt modifications
Changes to the Microsoft [`simbatt`](https://github.com/microsoft/Windows-driver-samples/tree/main/simbatt) sample:
* Switch to default output folder.
* Disable faulty read-back of state from registry through `GetSimBattStateFromRegistry` function: https://github.com/microsoft/Windows-driver-samples/pull/1064
* Remove `simbatt-wdfcoinstall.inx` that's no longer needed since Windows 10: https://github.com/microsoft/Windows-driver-samples/pull/1062
* Clear BATTERY_INFORMATION `BATTERY_CAPACITY_RELATIVE` field to enable per-battery charge display in Windows Settings: https://github.com/microsoft/Windows-driver-samples/pull/1063
