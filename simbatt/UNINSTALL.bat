@echo off
:: Goto current directory
cd /d "%~dp0"

:: Delete simulated battery
devcon.exe /r remove "{6B34C467-CE1F-4c0d-A3E4-F98A5718A9D6}\SimBatt"
::devgen /remove "{6B34C467-CE1F-4c0d-A3E4-F98A5718A9D6}\SimBatt"
:: TODO: Switch to PNPUTIL in Win11 21H2
::pnputil.exe /remove-device /deviceid "{6B34C467-CE1F-4c0d-A3E4-F98A5718A9D6}\SimBatt"

:: Uninstall driver
pnputil.exe /delete-driver simbatt.inf /uninstall /force /reboot

pause
