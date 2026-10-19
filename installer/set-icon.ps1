param(
    [Parameter(Mandatory = $true)][string]$Executable,
    [Parameter(Mandatory = $true)][string]$Icon
)
$ErrorActionPreference = 'Stop'

Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class IconResources {
    [DllImport("kernel32.dll", EntryPoint="BeginUpdateResourceW", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern IntPtr Begin(string file, bool deleteExisting);
    [DllImport("kernel32.dll", EntryPoint="UpdateResourceW", SetLastError=true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool Update(IntPtr handle, IntPtr type, IntPtr name, ushort language, byte[] data, uint size);
    [DllImport("kernel32.dll", EntryPoint="EndUpdateResourceW", SetLastError=true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool Finish(IntPtr handle, bool discard);
}
'@

$data = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Icon).Path)
$input = New-Object IO.MemoryStream(,$data)
$reader = New-Object IO.BinaryReader($input)
if ($reader.ReadUInt16() -ne 0 -or $reader.ReadUInt16() -ne 1) { throw 'Arquivo ICO invalido.' }
$count = $reader.ReadUInt16()
if ($count -lt 1) { throw 'O arquivo ICO nao tem imagens.' }

$groupStream = New-Object IO.MemoryStream
$group = New-Object IO.BinaryWriter($groupStream)
$group.Write([uint16]0)
$group.Write([uint16]1)
$group.Write([uint16]$count)
$images = @()
for ($i = 0; $i -lt $count; $i++) {
    $width = $reader.ReadByte(); $height = $reader.ReadByte()
    $colors = $reader.ReadByte(); $reserved = $reader.ReadByte()
    $planes = $reader.ReadUInt16(); $bits = $reader.ReadUInt16()
    $length = $reader.ReadUInt32(); $offset = $reader.ReadUInt32()
    if ([uint64]$offset + $length -gt $data.Length) { throw 'Dados do ICO incompletos.' }
    $image = New-Object byte[] $length
    [Array]::Copy($data, [int]$offset, $image, 0, [int]$length)
    $images += ,$image
    $group.Write([byte]$width); $group.Write([byte]$height)
    $group.Write([byte]$colors); $group.Write([byte]$reserved)
    $group.Write([uint16]$planes); $group.Write([uint16]$bits)
    $group.Write([uint32]$length); $group.Write([uint16]($i + 1))
}
$group.Flush()
$handle = [IconResources]::Begin((Resolve-Path -LiteralPath $Executable).Path, $false)
if ($handle -eq [IntPtr]::Zero) { throw "Nao foi possivel abrir o instalador: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())" }
$success = $false
try {
    for ($i = 0; $i -lt $images.Count; $i++) {
        $image = [byte[]]$images[$i]
        if (-not [IconResources]::Update($handle, [IntPtr]3, [IntPtr]($i + 1), 0, $image, [uint32]$image.Length)) {
            throw "Falha ao gravar imagem do icone: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
        }
    }
    $groupBytes = $groupStream.ToArray()
    if (-not [IconResources]::Update($handle, [IntPtr]14, [IntPtr]1, 0, $groupBytes, [uint32]$groupBytes.Length)) {
        throw "Falha ao gravar grupo do icone: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }
    $success = $true
} finally {
    if (-not [IconResources]::Finish($handle, -not $success)) {
        throw "Falha ao fechar o instalador: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }
}
