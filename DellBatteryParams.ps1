# Script for retrieving Dell battery parameters that are not exposed through the standard APIs
# Unofficial doc: https://docs.kernel.org/wmi/devices/dell-wmi-ddv.html
# A C++ implementation can be found in BatteryQuery\DellBattery.hpp 

# NOTICE: Script MUST run as Administrator.

Write-Host "Dell battery parameters:"
$dell = Get-CimInstance -Namespace root\WMI -Class DDVWmiMethodFunction

$cycleCount = Invoke-CimMethod -InputObject $dell -MethodName BatteryCycleCount -Arguments @{arg2=1}
$cycleCount = $cycleCount.argr
Write-Host BatteryCycleCount=$cycleCount


$date = Invoke-CimMethod -InputObject $dell -MethodName BatteryManufactureDate  -Arguments @{arg2=1}
# Date field encoding: "(year – 1980)*512 + month*32 + day"
$day = $date.argr % 32
$month = ($date.argr -shr 5) % 16
$year = 1980 + ($date.argr -shr 9)
Write-Host BatteryManufactureDate: Day=$day, Month=$month, Year=$year
