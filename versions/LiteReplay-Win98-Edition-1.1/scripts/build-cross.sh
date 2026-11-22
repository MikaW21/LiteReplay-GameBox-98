#!/usr/bin/env bash
# Reproduce the included x64 Windows binaries on Linux.
# Requirements: Zig 0.13.0, make, gcc (host generator), nm, Python 3, tar.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
work="${LITEREPLAY_CROSS_WORK:-$root/build-cross}"
zig_exe="${ZIG:-$(command -v zig)}"
[[ "$($zig_exe version)" == 0.13.0 ]] || { echo 'Use Zig 0.13.0 for this reproduction'; exit 1; }
mkdir -p "$work/toolchain" "$work/ffmpeg-build" "$work/objects" "$root/bin/windows-x64"
tar -xzf "$root/third_party_sources/FFmpeg-n7.1.1.tar.gz" -C "$work"
tar -xzf "$root/third_party_sources/nv-codec-headers-n12.2.72.0.tar.gz" -C "$work"
export LR_ZIG="$zig_exe" LR_NV_HEADERS="$work/nv-codec-headers-n12.2.72.0"
for tool in cc ar dlltool; do
  printf '#!/bin/sh\nexec "$LR_ZIG" %s ' "$tool" > "$work/toolchain/$tool"
  if [[ "$tool" == cc ]]; then printf '%s ' '-target x86_64-windows-gnu' >> "$work/toolchain/$tool"; fi
  printf '"$@"\n' >> "$work/toolchain/$tool"
  chmod +x "$work/toolchain/$tool"
done
cp "$root/scripts/nv_pkg_config.py" "$work/toolchain/nv-pkg-config"
chmod +x "$work/toolchain/nv-pkg-config"
cd "$work/ffmpeg-build"
"$work/FFmpeg-n7.1.1/configure" \
 --enable-cross-compile --target-os=mingw32 --arch=x86_64 \
 --cross-prefix="$work/toolchain/" --cc="$work/toolchain/cc" --ar="$work/toolchain/ar" --nm=nm \
 --pkg-config="$work/toolchain/nv-pkg-config" --windres=/bin/false \
 --disable-autodetect --disable-everything --disable-programs --disable-doc --disable-x86asm \
 --disable-network --disable-avdevice --disable-avfilter --disable-swscale --disable-postproc \
 --enable-shared --disable-static --enable-d3d11va --enable-ffnvcodec --enable-nvenc \
 --disable-cuvid --disable-nvdec --enable-encoder=h264_nvenc,hevc_nvenc,av1_nvenc,aac \
 --enable-muxer=mp4,matroska --enable-protocol=file --enable-swresample \
 --prefix="$work/sdk"
# LLD uses a fixed image base from the remaining flags; GNU-only switch is unsupported.
python3 - <<'PY'
from pathlib import Path
p=Path('ffbuild/config.mak')
s=p.read_text().replace(' -Wl,--disable-auto-image-base','')
p.write_text(s)
PY
make -j"${JOBS:-4}"
cd "$root"
common=(-target x86_64-windows-gnu -std=c++17 -O2 -DUNICODE -D_UNICODE -DNOMINMAX
  -Wno-macro-redefined -D_WIN32_WINNT=0x0A00 -I "$work/ffmpeg-build" -I "$work/FFmpeg-n7.1.1" -I "$root/src")
for unit in app video audio engine; do
  "$zig_exe" c++ "${common[@]}" -c "$root/src/$unit.cpp" -o "$work/objects/$unit.o"
done
"$zig_exe" c++ "${common[@]}" -c "$root/tests/integration_win.cpp" -o "$work/objects/probe.o"
libs=(-L "$work/ffmpeg-build/libavcodec" -L "$work/ffmpeg-build/libavformat"
  -L "$work/ffmpeg-build/libavutil" -L "$work/ffmpeg-build/libswresample"
  -lavcodec -lavformat -lavutil -lswresample -ld3d11 -ldxgi -ldxguid -lole32 -lavrt -luuid)
objects=("$work/objects/video.o" "$work/objects/audio.o" "$work/objects/engine.o")
"$zig_exe" c++ -target x86_64-windows-gnu "$work/objects/app.o" "${objects[@]}" "${libs[@]}" \
 -lmfuuid -lcomctl32 -lshell32 -ladvapi32 -ldwmapi -lpsapi -lpdh -lwinmm -lcomdlg32 -lwbemuuid -loleaut32 -lgdi32 -luser32 \
 -Wl,--subsystem,windows -municode -s -o "$root/bin/windows-x64/LiteReplay.exe"
"$zig_exe" c++ -target x86_64-windows-gnu "$work/objects/probe.o" "${objects[@]}" "${libs[@]}" \
 -s -o "$root/bin/windows-x64/LiteReplayProbe.exe"
cp "$work/ffmpeg-build/libavcodec/avcodec-61.dll" "$work/ffmpeg-build/libavformat/avformat-61.dll" \
 "$work/ffmpeg-build/libavutil/avutil-59.dll" "$work/ffmpeg-build/libswresample/swresample-5.dll" "$root/bin/windows-x64/"
rm -f "$root/bin/windows-x64/app.lib" "$root/bin/windows-x64/probe.lib"
python3 "$root/scripts/make_sound.py"
cp -r "$root/languages" "$root/bin/windows-x64/"
echo 'Cross-build complete. Still requires Windows/NVIDIA hardware validation.'
