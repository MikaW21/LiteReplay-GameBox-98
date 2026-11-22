#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
: "${FF_SOURCE:?FFmpeg 7.1.1 source directory}"
: "${AMF_INCLUDE:?AMF headers directory containing AMF/}"
: "${LR_NV_HEADERS:?nv-codec-headers source directory}"
: "${MINGW_BIN:?Directory containing x86_64-w64-mingw32-gcc-posix}"
work="${FF_BUILD:-$root/build-compat-ffmpeg}"
mkdir -p "$work"
export LR_NV_HEADERS MINGW_BIN
cd "$work"
"$FF_SOURCE/configure" --enable-cross-compile --target-os=mingw32 --arch=x86_64 --cross-prefix="$MINGW_BIN/x86_64-w64-mingw32-" \
 --cc="$MINGW_BIN/x86_64-w64-mingw32-gcc-posix" --ar="$MINGW_BIN/x86_64-w64-mingw32-ar" --nm="$MINGW_BIN/x86_64-w64-mingw32-nm" --ranlib="$MINGW_BIN/x86_64-w64-mingw32-ranlib" --windres="$root/scripts/mingw-windres.sh" \
 --pkg-config="$root/scripts/nv_pkg_config.py" --extra-cflags="-I$AMF_INCLUDE" --extra-ldflags="-static-libgcc" \
 --disable-autodetect --disable-everything --disable-programs --disable-doc --disable-x86asm --disable-network --disable-avdevice --disable-avfilter --disable-swscale --disable-postproc \
 --enable-shared --disable-static --enable-d3d11va --enable-ffnvcodec --enable-nvenc --disable-cuvid --disable-nvdec --enable-amf --enable-mediafoundation \
 --enable-encoder=h264_nvenc,hevc_nvenc,av1_nvenc,h264_amf,hevc_amf,av1_amf,h264_mf,aac --enable-muxer=mp4,matroska --enable-protocol=file --enable-swresample
make -j"${JOBS:-4}"
for pair in libavcodec/avcodec-61.dll libavformat/avformat-61.dll libavutil/avutil-59.dll libswresample/swresample-5.dll; do cp "$pair" "$root/bin/windows-x64/"; done

# GCC POSIX builds may dynamically link winpthreads from libavutil.
pthread_dll="$("$MINGW_BIN/x86_64-w64-mingw32-gcc-posix" -print-file-name=libwinpthread-1.dll)"
if [[ -f "$pthread_dll" ]]; then cp "$pthread_dll" "$root/bin/windows-x64/"; else
 echo 'Locate and bundle the matching libwinpthread-1.dll if listed by PE import inspection.' >&2
fi
