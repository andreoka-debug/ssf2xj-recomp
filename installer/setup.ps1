param(
    [string]$Destination = (Join-Path $env:LOCALAPPDATA 'Programs\SSF2X Recomp'),
    [switch]$NoLaunch
)
$ErrorActionPreference = 'Stop'

$files = @(
    'ssf2xj.exe', 'ssf2xj-launcher.exe',
    'SDL3.dll', 'libgcc_s_seh-1.dll', 'libstdc++-6.dll',
    'libwinpthread-1.dll', 'zlib1.dll', 'libiconv-2.dll',
    'LICENSE', 'README.md', 'Uninstall.ps1'
)
foreach ($name in $files) {
    if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot $name))) {
        throw "Arquivo ausente no instalador: $name"
    }
}

New-Item -ItemType Directory -Force -Path $Destination | Out-Null
foreach ($name in $files) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $Destination $name) -Force
}

$startMenu = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\SSF2X Recomp'
New-Item -ItemType Directory -Force -Path $startMenu | Out-Null
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut((Join-Path $startMenu 'Super Street Fighter II X.lnk'))
$shortcut.TargetPath = Join-Path $Destination 'ssf2xj-launcher.exe'
$shortcut.WorkingDirectory = $Destination
$shortcut.Description = 'Super Street Fighter II X Recomp'
$shortcut.Save()

if ($env:SSF2X_RECOMP_TEST_MODE -ne '1') {
    $registry = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\SSF2XRecomp'
    New-Item -Path $registry -Force | Out-Null
    New-ItemProperty -Path $registry -Name DisplayName -Value 'Super Street Fighter II X Recomp' -PropertyType String -Force | Out-Null
    New-ItemProperty -Path $registry -Name DisplayVersion -Value '0.1' -PropertyType String -Force | Out-Null
    New-ItemProperty -Path $registry -Name Publisher -Value 'SSF2X Recomp community project' -PropertyType String -Force | Out-Null
    New-ItemProperty -Path $registry -Name InstallLocation -Value $Destination -PropertyType String -Force | Out-Null
    $uninstall = 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "' + (Join-Path $Destination 'Uninstall.ps1') + '"'
    New-ItemProperty -Path $registry -Name UninstallString -Value $uninstall -PropertyType String -Force | Out-Null
    New-ItemProperty -Path $registry -Name NoModify -Value 1 -PropertyType DWord -Force | Out-Null
}

Write-Host "Instalado em $Destination"
if (-not $NoLaunch -and $env:SSF2X_RECOMP_TEST_MODE -ne '1') {
    Start-Process -FilePath (Join-Path $Destination 'ssf2xj-launcher.exe') -WorkingDirectory $Destination
}
