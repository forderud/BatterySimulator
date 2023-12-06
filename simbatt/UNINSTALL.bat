@echo off
:: Goto current directory
cd /d "%~dp0"

:: delete the devnode
devcon /r remove "{6B34C467-CE1F-4c0d-A3E4-F98A5718A9D6}\SimBatt"
:: TODO: Switch to PNPUTIL in Win11 21H2
::PNPUTIL /remove-device /deviceid "{6B34C467-CE1F-4c0d-A3E4-F98A5718A9D6}\SimBatt"

:: uninstall driver
::devgen /remove /hardwareid "{6B34C467-CE1F-4c0d-A3E4-F98A5718A9D6}\SimBatt"
PNPUTIL /delete-driver simbatt.inf /uninstall /force /reboot

pause
