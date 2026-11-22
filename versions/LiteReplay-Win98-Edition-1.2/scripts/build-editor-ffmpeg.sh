#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
work="${LITEREPLAY_EDITOR_BUILD_DIR:-$root/build-deps}"
zig_exe="${ZIG:-$(command -v zig)}"
mkdir -p "$work/toolchain" "$work/ffmpeg-build" "$root/bin/windows-x64/editor-tools"
tar -xzf "$root/third_party_sources/FFmpeg-n7.1.1.tar.gz" -C "$work"
tar -xzf "$root/third_party_sources/nv-codec-headers-n12.2.72.0.tar.gz" -C "$work"
export LR_ZIG="$zig_exe" LR_NV_HEADERS="$work/nv-codec-headers-n12.2.72.0"
for tool in cc ar dlltool; do
  printf '#!/bin/sh\nexec "$LR_ZIG" %s ' "$tool" > "$work/toolchain/$tool"
  if [[ "$tool" == cc ]]; then printf '%s ' '-target x86_64-windows-gnu' >> "$work/toolchain/$tool"; fi
  printf '"$@"\n' >> "$work/toolchain/$tool"; chmod +x "$work/toolchain/$tool"
done
cp "$root/scripts/nv_pkg_config.py" "$work/toolchain/nv-pkg-config"
chmod +x "$work/toolchain/nv-pkg-config"
cd "$work/ffmpeg-build"
"$work/FFmpeg-n7.1.1/configure" --enable-cross-compile --target-os=mingw32 --arch=x86_64 \
 --cross-prefix="$work/toolchain/" --cc="$work/toolchain/cc" --ar="$work/toolchain/ar" --nm=nm --strip=strip \
 --pkg-config="$work/toolchain/nv-pkg-config" --windres=/bin/false \
 --disable-autodetect --disable-everything --disable-doc --disable-x86asm --disable-network \
 --disable-programs --enable-ffmpeg --enable-ffprobe --enable-static --disable-shared \
 --enable-ffnvcodec --enable-nvenc --enable-d3d11va --disable-cuvid --disable-nvdec \
 --enable-encoder=h264_nvenc,hevc_nvenc,av1_nvenc,aac,mpeg4,pcm_s16le \
 --enable-decoder=h264,hevc,mpeg4,mpeg2video,mjpeg,vp8,vp9,aac,mp3,mp3float,ac3,eac3,flac,vorbis,opus,pcm_s16le,pcm_s24le,pcm_s32le,pcm_f32le,pcm_f64le \
 --enable-parser=h264,hevc,mpeg4video,mpegvideo,mjpeg,vp8,vp9,aac,mpegaudio,ac3,flac,vorbis,opus \
 --enable-demuxer=mov,matroska,avi,mpegts,mpegps,wav,mp3,flac,ogg,concat \
 --enable-bsf=h264_mp4toannexb,hevc_mp4toannexb,aac_adtstoasc,extract_extradata \
 --enable-muxer=mp4,matroska,wav --enable-protocol=file,pipe \
 --enable-indev=lavfi --enable-filter=scale,pad,setsar,fps,format,aformat,aresample,volume,anullsrc,trim,atrim,setpts,asetpts,concat,null,anull,buffer,abuffer,buffersink,abuffersink \
 --enable-swscale --enable-swresample
make -j"${JOBS:-4}"
cp ffmpeg.exe ffprobe.exe "$root/bin/windows-x64/editor-tools/"
