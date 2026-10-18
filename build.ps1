param(
    [Parameter(Mandatory = $true)][string]$RomPath,
    [string]$ToolkitSource = 'https://github.com/tmcleroy/cps2recomp.git',
    [string]$MsysRoot = 'C:\msys64'
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$toolkit = Join-Path $root '_build\cps2recomp'
$rom = (Resolve-Path -LiteralPath $RomPath).Path
$parentRom = Join-Path (Split-Path -Parent $rom) 'ssf2t.zip'
if (-not (Test-Path -LiteralPath $parentRom)) { throw 'ssf2t.zip precisa estar ao lado de ssf2xj.zip.' }
if (-not (Test-Path -LiteralPath (Join-Path $MsysRoot 'usr\bin\bash.exe'))) { throw 'Instale MSYS2 UCRT64 antes de compilar.' }
if (-not (Test-Path -LiteralPath $toolkit)) {
    New-Item -ItemType Directory -Path (Split-Path -Parent $toolkit) -Force | Out-Null
    # The toolkit verifies raw hashes of recomp-common; CRLF conversion breaks that check.
    & git -c core.autocrlf=false clone $ToolkitSource $toolkit
    if ($LASTEXITCODE -ne 0) { throw 'Não foi possível obter o cps2recomp.' }
    & git -C $toolkit checkout --detach fdd55ecbd20c9197c85ef526ecebd44879ced82e
    if ($LASTEXITCODE -ne 0) { throw 'Não foi possível selecionar a versão do toolkit usada neste projeto.' }
}
$commit = (& git -C $toolkit rev-parse HEAD).Trim()
if ($commit -ne 'fdd55ecbd20c9197c85ef526ecebd44879ced82e') {
    throw "O toolkit local usa outro commit: $commit"
}
$patches = @(
    @{ Name = 'display-wide-crt.patch'; Marker = 'machine/machine.cpp' },
    @{ Name = 'fix-windows-rom-path.patch'; Marker = 'machine/romset.cpp' },
    @{ Name = 'interface-wall-wide.patch'; Marker = 'recomp-common/text.h' }
)
foreach ($item in $patches) {
    $name = $item.Name
    $patch = Join-Path $root $name
    $marker = "--include=$($item.Marker)"
    & git -C $toolkit apply --ignore-whitespace --reverse --check $marker $patch 2>$null
    if ($LASTEXITCODE -eq 0) { continue }
    & git -C $toolkit apply --ignore-whitespace --check $patch
    if ($LASTEXITCODE -ne 0) { throw "O patch $name não se aplica à versão do toolkit." }
    & git -C $toolkit apply --ignore-whitespace $patch
    if ($LASTEXITCODE -ne 0) { throw "Falha ao aplicar $name." }
}
$game = Join-Path $toolkit 'games\ssf2xj'
New-Item -ItemType Directory -Path $game -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'ssf2xj\game.cfg') -Destination $game -Force
Copy-Item -LiteralPath (Join-Path $root 'ssf2xj\src') -Destination $game -Recurse -Force
Copy-Item -LiteralPath (Join-Path $root 'ssf2xj\movies') -Destination $game -Recurse -Force
$work = Join-Path $toolkit 'work\ssf2xj'
New-Item -ItemType Directory -Path $work -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'entries\entries.txt') -Destination $work -Force
Copy-Item -LiteralPath (Join-Path $root 'entries\entries.txt.z80') -Destination $work -Force
$env:MSYSTEM = 'UCRT64'
$env:CHERE_INVOKING = '1'
$env:RECOMP_ROM_PATH = $rom.Replace('\', '/')
$tempFolder = Join-Path $toolkit 'work-tmp'
New-Item -ItemType Directory -Path $tempFolder -Force | Out-Null
$env:TMPDIR = $tempFolder.Replace('\', '/')
$env:TMP = $tempFolder
$env:TEMP = $tempFolder
Push-Location $toolkit
try {
    & (Join-Path $MsysRoot 'usr\bin\bash.exe') -lc 'export TMPDIR="$PWD/work-tmp" TMP="$PWD/work-tmp" TEMP="$PWD/work-tmp"; make GAME=ssf2xj ROM="$RECOMP_ROM_PATH" port'
    if ($LASTEXITCODE -ne 0) { throw 'A compilação falhou.' }
} finally { Pop-Location }
Write-Host "Pronto: $(Join-Path $toolkit 'build\ssf2xj\ssf2xj.exe')"
