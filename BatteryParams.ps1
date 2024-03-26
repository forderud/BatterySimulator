# Script for retrieving battery parameters

Write-Host "Parameters for connected batteries:"
Get-CimInstance -Namespace root\CIMV2 Win32_Battery
