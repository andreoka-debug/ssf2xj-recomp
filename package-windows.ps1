param(
    [string]$MsysRoot = 'C:\msys64',
    [string]$OutputPath
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
if (-not $OutputPath) { $OutputPath = Join-Path $root 'dist\SSF2X-Recomp-Setup.exe' }
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
$build = Join-Path $root '_build\cps2recomp\build\ssf2xj'
$runtime = Join-Path $MsysRoot 'ucrt64\bin'
$stage = Join-Path $root '_build\windows-package\files'
New-Item -ItemType Directory -Force -Path $stage,(Split-Path -Parent $OutputPath) | Out-Null

$sources = [ordered]@{
    'ssf2xj.exe' = Join-Path $build 'ssf2xj.exe'
    'ssf2xj-launcher.exe' = Join-Path $build 'ssf2xj-launcher.exe'
    'SDL3.dll' = Join-Path $runtime 'SDL3.dll'
    'libgcc_s_seh-1.dll' = Join-Path $runtime 'libgcc_s_seh-1.dll'
    'libstdc++-6.dll' = Join-Path $runtime 'libstdc++-6.dll'
    'libwinpthread-1.dll' = Join-Path $runtime 'libwinpthread-1.dll'
    'zlib1.dll' = Join-Path $runtime 'zlib1.dll'
    'libiconv-2.dll' = Join-Path $runtime 'libiconv-2.dll'
    'LICENSE' = Join-Path $root 'LICENSE'
    'README.md' = Join-Path $root 'README.md'
    'install.cmd' = Join-Path $root 'installer\install.cmd'
    'setup.ps1' = Join-Path $root 'installer\setup.ps1'
    'Uninstall.ps1' = Join-Path $root 'installer\Uninstall.ps1'
}
foreach ($entry in $sources.GetEnumerator()) {
    if (-not (Test-Path -LiteralPath $entry.Value)) { throw "Arquivo ausente: $($entry.Value). Compile o jogo antes de empacotar." }
    Copy-Item -LiteralPath $entry.Value -Destination (Join-Path $stage $entry.Key) -Force
}

$sed = Join-Path $root '_build\windows-package\package.sed'
$lines = @(
    '[Version]', 'Class=IEXPRESS', 'SEDVersion=3', '',
    '[Options]', 'PackagePurpose=InstallApp', 'ShowInstallProgramWindow=1',
    'HideExtractAnimation=1', 'UseLongFileName=1', 'InsideCompressed=1',
    'CAB_FixedSize=0', 'CAB_ResvCodeSigning=0', 'RebootMode=N',
    'InstallPrompt=%InstallPrompt%', 'DisplayLicense=%DisplayLicense%',
    'FinishMessage=%FinishMessage%', 'TargetName=%TargetName%',
    'FriendlyName=%FriendlyName%', 'AppLaunched=%AppLaunched%',
    'PostInstallCmd=%PostInstallCmd%', 'AdminQuietInstCmd=%AdminQuietInstCmd%',
    'UserQuietInstCmd=%UserQuietInstCmd%', 'SourceFiles=SourceFiles', '',
    '[Strings]', 'InstallPrompt=', 'DisplayLicense=', 'FinishMessage=',
    "TargetName=$OutputPath", 'FriendlyName=SSF2X Recomp',
    'AppLaunched=cmd.exe /d /c install.cmd', 'PostInstallCmd=<None>',
    'AdminQuietInstCmd=', 'UserQuietInstCmd=', ''
)
for ($i = 0; $i -lt $sources.Count; $i++) { $lines += "FILE$i=`"$($sources.Keys[$i])`"" }
$lines += '', '[SourceFiles]', "SourceFiles0=$stage\", '', '[SourceFiles0]'
for ($i = 0; $i -lt $sources.Count; $i++) { $lines += "%FILE$i%=" }
[IO.File]::WriteAllText($sed, ($lines -join "`r`n") + "`r`n", [Text.Encoding]::ASCII)

$iexpress = Join-Path $env:WINDIR 'System32\iexpress.exe'
if (-not (Test-Path -LiteralPath $iexpress)) { throw 'IExpress nao esta disponivel nesta instalacao do Windows.' }
if (Test-Path -LiteralPath $OutputPath) { Remove-Item -LiteralPath $OutputPath -Force }
& $iexpress /N /Q /M $sed
$deadline = [DateTime]::UtcNow.AddSeconds(45)
$lastSize = -1
$stable = 0
while ([DateTime]::UtcNow -lt $deadline) {
    if (Test-Path -LiteralPath $OutputPath) {
        $size = (Get-Item -LiteralPath $OutputPath).Length
        $stable = if ($size -gt 1024 -and $size -eq $lastSize) { $stable + 1 } else { 0 }
        if ($stable -ge 2) { break }
        $lastSize = $size
    }
    Start-Sleep -Seconds 1
}
if (-not (Test-Path -LiteralPath $OutputPath) -or $stable -lt 2) {
    throw 'Falha ao criar o instalador com IExpress.'
}
Write-Host "Instalador pronto: $OutputPath"
