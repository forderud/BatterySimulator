@echo off
:: Goto current directory
cd /d "%~dp0"

:: Trust driver certificate
certutil.exe -addstore root simbatt.cer
certutil.exe -f -addstore trustedpublisher simbatt.cer

:: Install driver
pnputil.exe /add-driver simbatt.inf /install

pause
