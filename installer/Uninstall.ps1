$ErrorActionPreference = 'Stop'
$destination = $PSScriptRoot
$startMenu = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\SSF2X Recomp'
$names = @(
    'ssf2xj.exe', 'ssf2xj-launcher.exe',
    'SDL3.dll', 'libgcc_s_seh-1.dll', 'libstdc++-6.dll',
    'libwinpthread-1.dll', 'zlib1.dll', 'libiconv-2.dll',
    'LICENSE', 'README.md', 'cps2x.ico', 'Uninstall.ps1'
)
foreach ($name in $names) {
    $path = Join-Path $destination $name
    if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Force }
}
$shortcut = Join-Path $startMenu 'Super Street Fighter II X.lnk'
if (Test-Path -LiteralPath $shortcut) { Remove-Item -LiteralPath $shortcut -Force }
if (Test-Path -LiteralPath $startMenu) { Remove-Item -LiteralPath $startMenu -ErrorAction SilentlyContinue }
$registry = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\SSF2XRecomp'
$registered = Get-ItemProperty -LiteralPath $registry -ErrorAction SilentlyContinue
if ($registered -and $registered.InstallLocation -eq $destination) {
    Remove-Item -LiteralPath $registry -ErrorAction SilentlyContinue
}
if (Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination -ErrorAction SilentlyContinue }
Write-Host 'Programa removido. Suas configuracoes e ROMs foram preservadas.'
