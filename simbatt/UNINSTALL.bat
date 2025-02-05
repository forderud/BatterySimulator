@echo off
:: Goto current directory
cd /d "%~dp0"

:: Uninstall driver
pnputil.exe /delete-driver simbatt.inf /uninstall /force

pause
