# Script for building and packaging versioned releases of this project.
# Run from "Developer PowerShell for VS 2022" from a commit with a git tag.

# stop script on first error
$ErrorActionPreference = "Stop"

$p = Start-Process -FilePath "git.exe" -ArgumentList "tag --points-at HEAD" -NoNewWindow -Wait -RedirectStandardOutput  "tagname.txt"
$tagname = Get-Content -Path "tagname.txt"
if ($tagname -eq $null) {
    throw "No git tag found for current commit"
}

$ver_tokens = $tagname.Split(".")
while ($ver_tokens.Count -le 4) {
    $ver_tokens += "0" # set patch & build to 0 if unspecified
}
$MajorVer = $ver_tokens[0].Substring(1) # remove "v" prefix
$MinorVer = $ver_tokens[1]
$PatchVer = $ver_tokens[2]
$BuildVer = $ver_tokens[3]

### Build solution in x64|Release
msbuild /nologo /verbosity:minimal /property:Configuration="Release"`;Platform="x64"`;MajorVer=$MajorVer`;MinorVer=$MinorVer`;PatchVer=$PatchVer`;BuildVer=$BuildVer BatterySimulator.sln
if ($LastExitCode -ne 0) {
    throw "msbuild failure"
}
