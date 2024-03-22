# Script for building and packaging releases of this project.
# Run from "Developer PowerShell for VS 2022"

# stop script on first error
$ErrorActionPreference = "Stop"

### Build solution in x64|Release
msbuild /nologo /verbosity:minimal /property:Configuration="Release"`;Platform="x64" BatterySimulator.sln
if ($LastExitCode -ne 0) {
    throw "msbuild failure"
}

# Compress driver and command-line utility
Compress-Archive -Path x64\Release\simbatt, x64\Release\BatteryConfig.exe  -DestinationPath simbatt.zip
