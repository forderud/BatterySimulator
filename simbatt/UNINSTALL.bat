@echo off
:: Goto current directory
cd /d "%~dp0"

:: remove batterie(s)
devgen.exe /remove SWD\DEVGEN\0000
devgen.exe /remove SWD\DEVGEN\0001
devgen.exe /remove SWD\DEVGEN\0002

:: TODO: Switch to PNPUTIL in Win11 21H2
::pnputil.exe /remove-device /deviceid "{6B34C467-CE1F-4c0d-A3E4-F98A5718A9D6}\SimBatt"

:: uninstall driver
pnputil.exe /delete-driver simbatt.inf /uninstall /force /reboot

pause
