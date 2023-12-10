@echo off
:: Goto current directory
cd /d "%~dp0"

:: Install driver
pnputil.exe /add-driver simbatt.inf /install /reboot

:: Add batterie(s)
devgen /add /instanceid 0000 /hardwareid "{6B34C467-CE1F-4c0d-A3E4-F98A5718A9D6}\SimBatt"
devgen /add /instanceid 0001 /hardwareid "{6B34C467-CE1F-4c0d-A3E4-F98A5718A9D6}\SimBatt"
devgen /add /instanceid 0002 /hardwareid "{6B34C467-CE1F-4c0d-A3E4-F98A5718A9D6}\SimBatt"

:: Use PnpUtil for installation (succeeds but driver isn't loaded)
::devgen /add /hardwareid "{6B34C467-CE1F-4c0d-A3E4-F98A5718A9D6}\SimBatt"

pause
