# Per-user install. Run from the built distribution directory, not the source tree.
$ErrorActionPreference = 'Stop'
$source = $PSScriptRoot
if (-not (Test-Path (Join-Path $source 'LiteReplay.exe'))) { throw 'Build LiteReplay first, then run install.ps1 inside dist\LiteReplay.' }
if (Get-Process LiteReplay -ErrorAction SilentlyContinue) { throw 'Exit LiteReplay from its tray menu before installing.' }
$target = Join-Path $env:LOCALAPPDATA 'Programs\LiteReplay-2-Preview'
if ([IO.Path]::GetFullPath($source).TrimEnd('\') -ne [IO.Path]::GetFullPath($target).TrimEnd('\')) {
    New-Item -ItemType Directory -Force $target | Out-Null
    Copy-Item "$source\*" $target -Recurse -Force
}
$shortcut = Join-Path ([Environment]::GetFolderPath('Programs')) 'LiteReplay 2 Preview.lnk'
$shell = New-Object -ComObject WScript.Shell
$link = $shell.CreateShortcut($shortcut)
$link.TargetPath = Join-Path $target 'LiteReplay.exe'
$link.WorkingDirectory = $target
$link.Save()
Write-Host "Installed to $target. Launch LiteReplay from Start. Startup remains opt-in in the application."
