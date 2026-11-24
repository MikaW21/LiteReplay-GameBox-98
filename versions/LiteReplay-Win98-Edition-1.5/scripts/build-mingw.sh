#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
: "${FFMPEG_HEADERS:?Set FFMPEG_HEADERS to matching FFmpeg 7.1.1 source or SDK headers with libavutil/avconfig.h}"
cxx="${CXX:-x86_64-w64-mingw32-g++}"
windres="${WINDRES:-$("$cxx" -print-prog-name=windres)}"
work="${LR_BUILD_DIR:-$root/build-mingw}"
mkdir -p "$work" "$root/bin/windows-x64/languages"
common=(-std=c++17 -O2 -DUNICODE -D_UNICODE -DNOMINMAX -D_WIN32_WINNT=0x0A00 -I "$FFMPEG_HEADERS" -I "$root/src")
for unit in app video audio engine; do
 "$cxx" "${common[@]}" -c "$root/src/$unit.cpp" -o "$work/$unit.o"
done
"$windres" -I "$root/src" -i "$root/src/app_resources.rc" -O coff -o "$work/app_resources.o"
"$cxx" "${common[@]}" -c "$root/tests/integration_win.cpp" -o "$work/probe.o"
libs=("$root/bin/windows-x64/avcodec-61.dll" "$root/bin/windows-x64/avformat-61.dll" "$root/bin/windows-x64/avutil-59.dll" "$root/bin/windows-x64/swresample-5.dll" -ld3d11 -ldxgi -ldxguid -lole32 -lavrt -luuid -lksuser)
objects=("$work/video.o" "$work/audio.o" "$work/engine.o")
"$cxx" "$work/app.o" "$work/app_resources.o" "${objects[@]}" "${libs[@]}" -lwindowscodecs -lmfuuid -lcomctl32 -lshell32 -ladvapi32 -ldwmapi -lpsapi -lpdh -lwinmm -lcomdlg32 -lwbemuuid -loleaut32 -lgdi32 -luser32 -static -static-libgcc -static-libstdc++ -municode -mwindows -s -o "$root/bin/windows-x64/LiteReplay.exe"
"$cxx" "$work/probe.o" "${objects[@]}" "${libs[@]}" -static -static-libgcc -static-libstdc++ -s -o "$root/bin/windows-x64/LiteReplayProbe.exe"
cp "$root/languages/"*.lang "$root/bin/windows-x64/languages/"
echo 'Built Windows x64 binaries. Hardware capture and Windows UI runtime remain unverified.'
