@echo off
:: Goto current directory
cd /d "%~dp0"

:: Delete simulated battery
devgen /remove "SWD\DEVGEN\1"
devgen /remove "SWD\DEVGEN\2"
:: TODO: Switch to PNPUTIL in Win11 21H2
::pnputil.exe /remove-device /deviceid "SWD\DEVGEN\1"
::pnputil.exe /remove-device /deviceid "SWD\DEVGEN\2"

:: Uninstall driver
pnputil.exe /delete-driver simbatt.inf /uninstall /force /reboot

pause
