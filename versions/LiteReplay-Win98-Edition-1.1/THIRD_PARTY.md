# Third-party components

LiteReplay source code is MIT licensed; see LICENSE. This project includes four minimal FFmpeg 7.1.1 DLLs built for Windows x64,
plus matching upstream source archives under third_party_sources. It does not
include the NVIDIA driver or proprietary Windows SDK.

Runtime/build dependencies:

- Intel PresentMon Console 2.3.1 (MIT): https://github.com/GameTechDev/PresentMon/releases/tag/v2.3.1 .
  The release executable and its license are in bin/windows-x64/performance-tools.
- NVIDIA NVML: optional sensor readings via the installed driver.
- LibreHardwareMonitor: optional external WMI sensor provider, distributed by its own project.

- FFmpeg libavcodec / libavformat / libavutil / libswresample:
  https://ffmpeg.org/ ; license depends on the exact build configuration.
  The vcpkg manifest does not enable GPL/nonfree features. Shared linking is used.
- ffnvcodec headers: https://github.com/FFmpeg/nv-codec-headers ; their own license.
- NVIDIA NVENC driver runtime: supplied by the installed NVIDIA display driver,
  subject to NVIDIA's terms. Do not copy the driver DLL into the application.
- Microsoft Windows SDK / system DLLs and MSVC runtime: Microsoft license terms.
- vcpkg: https://github.com/microsoft/vcpkg ; dependency manager, not a runtime service.

The build script copies installed dependency copyright notices into
third-party-licenses and records versions/build configuration. For any binary
redistribution, preserve those notices and satisfy the exact dependencies'
source/relinking and other distribution requirements. A manual FFmpeg SDK needs
its own matching notices and source provenance. MIT for LiteReplay does not
relicense its dependencies. The Windows VC++ x64 redistributable may be needed
on a clean machine; install it from Microsoft rather than copying arbitrary DLLs.

## 1.3 提示音

bin/windows-x64/save.wav 由 scripts/make_sound.py 原创合成，按本项目 LICENSE 提供。不包含 Discord 音频。

## 1.4 Equalizer APO

可选外部依赖，未捆绑或复制其二进制/源码。用户从官方项目安装，LiteReplay 生成其公开格式的配置文件。官方项目 https://sourceforge.net/projects/equalizerapo/ 。

## Compatibility preview build provenance (2026-11-09)

Application, probe and all four FFmpeg 7.1.1 DLLs use MinGW GCC 13 POSIX. Application runtime libraries are statically linked; preserve the GCC runtime exception and MinGW notices. The shared FFmpeg libraries are replaceable. FFmpeg is unmodified and built without GPL/nonfree components, decoders or external assembly, with NVENC, AMD AMF, h264_mf and AAC encoders. Exact configuration and encoder list are in validation/. Source archives are in third_party_sources/. Reproduce with scripts/build-compat-ffmpeg.sh, then scripts/build-mingw.sh. The older Zig build script documents historical releases only.

AMF headers 1.4.33 and their notices are included. AMD and NVIDIA driver runtimes are not bundled. Windows supplies Media Foundation. No libx264 encoder was added. The original FFmpeg build log records an ignored missing dlltool error for optional import libraries; these were subsequently generated successfully (validation/import-library-repair.log). The recipe now sets the absolute cross-prefix to prevent that error.

The matching MinGW libwinpthread-1.dll is bundled because libavutil imports it. Preserve its MinGW/winpthreads notices.

## Phase3a editor tools

The separately executed ffmpeg.exe and ffprobe.exe in bin/windows-x64/editor-tools are reused byte-for-byte from the audited LiteCut 0.1 bundle (FFmpeg 7.1.1, Zig 0.13.0). They are different from the recorder's shared DLL build. Build with scripts/build-editor-ffmpeg.sh; exact source archives already reside in third_party_sources. Their recorded provenance is in editor-tools/BUILD_INFO.json and THIRD_PARTY.md. No LiteCut.exe or independent editor application is shipped. The editor UI is compiled into LiteReplay.exe.
