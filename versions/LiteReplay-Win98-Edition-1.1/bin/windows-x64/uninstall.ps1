$ErrorActionPreference = 'Stop'
if (Get-Process LiteReplay -ErrorAction SilentlyContinue) { throw 'Exit LiteReplay from its tray menu first.' }
$target = Join-Path $env:LOCALAPPDATA 'Programs\LiteReplay-2-Preview'
$run = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$value = Get-ItemPropertyValue $run -Name LiteReplay -ErrorAction SilentlyContinue
if ($value -and $value.StartsWith('"' + (Join-Path $target 'LiteReplay.exe') + '"')) {
    Remove-ItemProperty $run -Name LiteReplay
}
$shortcut = Join-Path ([Environment]::GetFolderPath('Programs')) 'LiteReplay 2 Preview.lnk'
if (Test-Path $shortcut) { Remove-Item $shortcut }
if (Test-Path $target) { Remove-Item $target -Recurse -Force }
Write-Host 'Uninstalled. Saved videos, logs and settings were kept.'
