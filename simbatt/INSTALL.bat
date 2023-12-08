@echo off
:: Goto current directory
cd /d "%~dp0"

:: Use DevCon for installation, since it allows providing HWID
devcon.exe /r install simbatt.inf "{6B34C467-CE1F-4c0d-A3E4-F98A5718A9D6}\SimBatt"

:: Use PnpUtil for installation (succeeds but driver isn't loaded)
::devgen /add /hardwareid "{6B34C467-CE1F-4c0d-A3E4-F98A5718A9D6}\SimBatt"
::pnputil.exe /add-driver simbatt.inf /install /reboot

pause
