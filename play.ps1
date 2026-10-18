param(
    [string]$RomPath = (Join-Path $PSScriptRoot 'roms\ssf2xj.zip'),
    [string]$MsysRoot = 'C:\msys64'
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$game = Join-Path $root '_build\cps2recomp\build\ssf2xj\ssf2xj.exe'
if (-not (Test-Path -LiteralPath $game)) { throw 'Compile primeiro com build.ps1.' }
$rom = (Resolve-Path -LiteralPath $RomPath).Path
if (-not (Test-Path -LiteralPath (Join-Path (Split-Path -Parent $rom) 'ssf2t.zip'))) {
    throw 'ssf2t.zip precisa estar ao lado de ssf2xj.zip.'
}
$runtime = Join-Path $MsysRoot 'ucrt64\bin'
if (-not (Test-Path -LiteralPath (Join-Path $runtime 'SDL3.dll'))) { throw 'SDL3.dll não foi encontrada no MSYS2 UCRT64.' }
$userdata = Join-Path $root 'userdata'
New-Item -ItemType Directory -Path $userdata -Force | Out-Null
$settings = Join-Path $userdata 'settings.ini'
if (-not (Test-Path -LiteralPath $settings)) { Set-Content -LiteralPath $settings -Value "[features]`narcade_timing=1`n" }
$env:PATH = "$runtime;$env:PATH"
$args = @('--rom', ('"' + $rom + '"'), '--settings', ('"' + $settings + '"'),
          '--eeprom', ('"' + (Join-Path $userdata 'eeprom.bin') + '"'))
$process = Start-Process -FilePath $game -WorkingDirectory (Split-Path -Parent $game) -ArgumentList $args -Wait -PassThru
exit $process.ExitCode
