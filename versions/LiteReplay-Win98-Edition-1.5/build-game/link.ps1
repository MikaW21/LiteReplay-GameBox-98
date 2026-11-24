param($object,$output)
$env:Path="C:\Yuzi_app\.retro-build\mingw64\bin;$env:Path"
$bin="C:/Yuzi_app/LiteReplay-Win98-Edition-1.5/bin/windows-x64"
g++ -s -static-libgcc -static-libstdc++ -municode -mwindows $object ../.retro-build/win98-ui-engine.o ../.retro-build/win98-ui-video.o ../.retro-build/win98-ui-audio.o build-game/resources.o "$bin/avcodec-61.dll" "$bin/avformat-61.dll" "$bin/avutil-59.dll" "$bin/swresample-5.dll" -ld3d11 -ldxgi -ldxguid -lole32 -lavrt -luuid -lksuser -lwindowscodecs -lmfuuid -lcomctl32 -lshell32 -ladvapi32 -ldwmapi -lpsapi -lpdh -lwinmm -lcomdlg32 -lwbemuuid -loleaut32 -lgdi32 -o $output
exit $LASTEXITCODE

