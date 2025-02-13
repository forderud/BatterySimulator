# Script for retrieving standard battery parameters
# See DellBatteryParams.ps1 for proprietary Dell parameters

Write-Host "Parameters for connected batteries:"
Get-CimInstance -Namespace root\CIMV2 -Class Win32_Battery

Write-Host "Extra parameters for connected batteries:"
Get-WmiObject -Namespace root\wmi  -Class MSBatteryClass
