# PowerShell script must run as admin
$ErrorActionPreference = "Stop"

function PrintPowerSetting ($setting, $acValue, $dcValue) {
    Write-Host ID: $setting.InstanceID
    Write-Host Name: $setting.ElementName
    Write-Host Description: $setting.Description
    Write-Host AC value: $acValue
    Write-Host DC value: $dcValue
}

# Get current power plan (Balanced, High performance, Power saver, ..)
$powerplan = Get-WmiObject -Namespace "root\cimv2\power" -Class Win32_powerplan | where {$_.IsActive}

$settings = $powerplan.GetRelated("Win32_PowerSettingDataIndex")
foreach ($settingidx in $settings) {
    # will iterate over S0_AC, S0_DC, S1_AC, S1_DC, S2_AC, S2_DC, ...
    $acdc = $settingidx.InstanceID.split("\")[2] # determine if AC or DC setting
    if ($acdc -eq "AC") {
        $acValue = $settingidx.SettingIndexValue
        $acID = $settingidx.InstanceID.split("\")[3]
        continue # corresponding DC value will come in next iteration
    } else {
        $dcValue = $settingidx.SettingIndexValue
        $dcID = $settingidx.InstanceID.split("\")[3]
    }

    if ($acID -ne $dcID) { # sanity check
        Write-Error "AC vs. DC InstanceID mismatch"
        Exit
    }

    $setting = $settingidx.GetRelated("Win32_PowerSetting")
    Write-Host # blank line
    PrintPowerSetting $setting $acValue $dcValue

    # uncomment to modify DC settings to match AC
    #$settingidx.SettingIndexValue = $acValue
    #[void]$settingidx.Put() # store change

    #break
}
