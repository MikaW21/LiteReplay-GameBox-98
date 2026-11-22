param(
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [string]$FFmpegRoot = '',
    [ValidateSet('Release','Debug')][string]$Configuration = 'Release'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
function Run([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw 'Install Visual Studio 2022 Desktop development with C++, CMake tools and Windows SDK 10.0.19041 or newer.'
}
$build = Join-Path $root 'build-windows'
$configure = @('-S', $root, '-B', $build, '-G', 'Visual Studio 17 2022', '-A', 'x64')
if ($FFmpegRoot) {
    $FFmpegRoot = (Resolve-Path $FFmpegRoot).Path
    $configure += "-DFFMPEG_ROOT=$FFmpegRoot"
} else {
    if (-not $VcpkgRoot) { throw 'Pass -VcpkgRoot C:\src\vcpkg or set VCPKG_ROOT. See README for dependency setup.' }
    $VcpkgRoot = (Resolve-Path $VcpkgRoot).Path
    if (-not (Test-Path "$VcpkgRoot\vcpkg.exe")) { Run "$VcpkgRoot\bootstrap-vcpkg.bat" @('-disableMetrics') }
    # Explicitly disable vcpkg's own optional usage metrics too.
    $env:VCPKG_DISABLE_METRICS = '1'
    $configure += @("-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot/scripts/buildsystems/vcpkg.cmake", '-DVCPKG_TARGET_TRIPLET=x64-windows')
}
Run 'cmake' $configure
Run 'cmake' @('--build', $build, '--config', $Configuration, '--parallel', '4')
Run 'ctest' @('--test-dir', $build, '-C', $Configuration, '--output-on-failure')
$bin = Join-Path $build $Configuration
# This only checks linked codec availability; capture requires explicit --capture.
Run (Join-Path $bin 'LiteReplayProbe.exe') @()
$dist = Join-Path $root 'dist\LiteReplay'
if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item -ItemType Directory -Force $dist | Out-Null
Copy-Item "$bin\LiteReplay.exe", "$bin\LiteReplayProbe.exe" $dist
$dlls = @(Get-ChildItem $bin -Filter '*.dll')
if ($dlls.Count -eq 0) { throw 'No FFmpeg runtime DLLs were deployed. Check shared x64-windows dependencies.' }
$dlls | Copy-Item -Destination $dist
Copy-Item "$root\README.md", "$root\LICENSE", "$root\THIRD_PARTY.md" $dist
Copy-Item "$root\docs" $dist -Recurse
Copy-Item "$root\languages" $dist -Recurse
Copy-Item "$bin\editor-tools", "$bin\performance-tools", "$bin\assets" $dist -Recurse
Copy-Item "$root\bin\windows-x64\save.wav" $dist
Copy-Item "$root\scripts\install.ps1", "$root\scripts\uninstall.ps1", "$root\scripts\verify_replay.py", "$root\scripts\measure.ps1" $dist
$record = Join-Path $dist 'BUILD_INFO.txt'
"Build time: $(Get-Date -Format o)`nConfiguration: $Configuration`n" | Set-Content $record
& cmake --version | Add-Content $record
if ($VcpkgRoot -and -not $FFmpegRoot) {
    & git -C $VcpkgRoot rev-parse HEAD | Add-Content $record
    if ($LASTEXITCODE -ne 0) { throw 'Cannot record vcpkg revision' }
    $installed = Join-Path $build 'vcpkg_installed\x64-windows'
    $notices = Join-Path $dist 'third-party-licenses'
    New-Item -ItemType Directory -Force $notices | Out-Null
    Get-ChildItem "$installed\share" -Filter copyright -Recurse | ForEach-Object {
        Copy-Item $_.FullName (Join-Path $notices ($_.Directory.Name + '.txt'))
    }
    Copy-Item "$build\vcpkg_installed\vcpkg\status" (Join-Path $dist 'DEPENDENCIES.txt')
} else {
    "Manual FFmpeg SDK: $FFmpegRoot`nSupply that SDK's license files before redistribution." | Add-Content $record
}
& "$bin\LiteReplayProbe.exe" | Add-Content $record
if ($LASTEXITCODE -ne 0) { throw 'Codec probe failed' }
$zip = Join-Path $root 'dist\LiteReplay-windows-x64.zip'
Compress-Archive -Path "$dist\*" -DestinationPath $zip -Force
Write-Host "Created $zip"
Write-Host 'Windows capture, synchronization and performance are NOT validated by this build. Run the manual tests.'
