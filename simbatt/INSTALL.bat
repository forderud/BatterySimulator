@echo off
:: Goto current directory
cd /d "%~dp0"

:: Install driver certificate
certmgr.exe /add simbatt.cer /s /r localMachine root
certmgr.exe /add simbatt.cer /s /r localMachine trustedpublisher

:: Install driver
pnputil.exe /add-driver simbatt.inf /install /reboot

:: Create simulated batteries
devgen /add /instanceid 1 /hardwareid "{6B34C467-CE1F-4c0d-A3E4-F98A5718A9D6}\SimBatt"
devgen /add /instanceid 2 /hardwareid "{6B34C467-CE1F-4c0d-A3E4-F98A5718A9D6}\SimBatt"

pause
