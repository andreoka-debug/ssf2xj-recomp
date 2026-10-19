param(
    [Parameter(Mandatory = $true)][ValidateSet('Host', 'Join')][string]$Mode,
    [string]$Address = '127.0.0.1',
    [ValidateRange(1, 65535)][int]$Port = 27888,
    [string]$RomPath,
    [string]$MsysRoot = 'C:\msys64'
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$game = Join-Path $root '_build\cps2recomp\build\ssf2xj\ssf2xj.exe'
if (-not (Test-Path -LiteralPath $game)) { throw 'Compile primeiro com build.ps1.' }
if (-not $RomPath) { $RomPath = Join-Path $root 'roms\ssf2xj.zip' }
$rom = (Resolve-Path -LiteralPath $RomPath).Path
if (-not (Test-Path -LiteralPath (Join-Path (Split-Path -Parent $rom) 'ssf2t.zip'))) {
    throw 'ssf2t.zip precisa estar ao lado de ssf2xj.zip.'
}
$parsedAddress = $null
if (-not [Net.IPAddress]::TryParse($Address, [ref]$parsedAddress) -or
    $parsedAddress.AddressFamily -ne [Net.Sockets.AddressFamily]::InterNetwork) {
    throw 'Informe um endereco IPv4 numerico.'
}
$runtime = Join-Path $MsysRoot 'ucrt64\bin'
$env:PATH = "$runtime;$env:PATH"
$userdata = Join-Path $root 'userdata'
$logs = Join-Path $userdata 'logs'
New-Item -ItemType Directory -Force -Path $logs | Out-Null
$settings = Join-Path $userdata 'settings.ini'
$log = Join-Path $logs ("netplay-{0}-{1}.log" -f $Mode.ToLowerInvariant(), (Get-Date -Format 'yyyyMMdd-HHmmss'))
$option = if ($Mode -eq 'Host') { '--net-host' } else { '--net-join' }
$player = if ($Mode -eq 'Host') { 1 } else { 2 }
Write-Host "Sincronizacao experimental - P$player - ${Address}:$Port"
Write-Host 'Use o mapeamento local do Jogador 1. Esc encerra a sessao.'
# Deliberately omit EEPROM: both PCs start with the same fresh board settings,
# and the session never overwrites the offline saves or preferences.
$arguments = @('--rom', ('"' + $rom + '"'), '--settings', ('"' + $settings + '"'),
    $option, "${Address}:$Port", '--net-log', ('"' + $log + '"'))
$process = Start-Process -FilePath $game -WorkingDirectory (Split-Path -Parent $game) -ArgumentList $arguments -Wait -PassThru
if ($process.ExitCode -ne 0) { Write-Host "Sessao encerrada com erro. Diagnostico: $log" }
exit $process.ExitCode
