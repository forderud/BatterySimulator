## simbatt implementation notes
This section is only intended for internal bookkeeping. Testers doesn't need to care about this.

### Persistent state
Registry flags used: `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\ROOT\BATTERY\<DeviceNumber>\Device Parameters`

### simbatt modifications
The most prominent changes to the Microsoft [`simbatt`](https://github.com/microsoft/Windows-driver-samples/tree/main/simbatt) sample:
* Switch to default output folder.
* ~~Disable faulty read-back of state from registry through `GetSimBattStateFromRegistry` function: https://github.com/microsoft/Windows-driver-samples/pull/1064~~
* Clear BATTERY_INFORMATION `BATTERY_CAPACITY_RELATIVE` field to enable per-battery charge display in Windows Settings: https://github.com/microsoft/Windows-driver-samples/pull/1063
* Provide instructions for how to test the driver: https://github.com/microsoft/Windows-driver-samples/pull/1187
