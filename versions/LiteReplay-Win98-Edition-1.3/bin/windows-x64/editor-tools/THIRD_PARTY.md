# Third-party components

## FFmpeg 7.1.1

Project: https://ffmpeg.org/  Source: https://github.com/FFmpeg/FFmpeg/tree/n7.1.1
`ffmpeg.exe` and `ffprobe.exe` were built from the included unmodified `third_party_sources/FFmpeg-n7.1.1.tar.gz` with `scripts/build-editor-ffmpeg.sh`.
This is a minimal LGPL build with no --enable-gpl or --enable-nonfree. The LGPL 2.1 license is included. Full configure flags are in the build script. FFmpeg is executed as a separate program, not linked into LiteReplay.
The exact bundled sources and build script allow rebuilding/replacing these tools. Do not use LiteReplay's encoder-only DLLs as substitutes.

## nv-codec-headers 12.2.72.0

Project: https://github.com/FFmpeg/nv-codec-headers
Unmodified source archive and upstream license comments accompany the build. Headers enable optional NVIDIA hardware encoding. The NVIDIA display driver is not bundled.

## Windows APIs / toolchain

MFPlay, Win32 and common controls are supplied by Windows. Built with Zig 0.13.0; an MSVC build is supported through CMake. FFmpeg upstream archives include licenses of all their components.
