#pragma once
#include <array>
#include <cstddef>
namespace lr {
struct Resolution { int width,height;const wchar_t* label; };
inline constexpr std::array<Resolution,5> resolutions{{
    {1280,720,L"720p · 1280 × 720"}, {1600,900,L"900p · 1600 × 900"},
    {1920,1080,L"1080p · 1920 × 1080"}, {2560,1440,L"1440p · 2560 × 1440"},
    {3840,2160,L"2160p · 3840 × 2160"}
}};
inline constexpr std::array<int,8> frameRates{{24,30,50,60,75,90,120,144}};
inline size_t resolutionIndex(int width){for(size_t i=0;i<resolutions.size();++i)if(resolutions[i].width==width)return i;return 2;}
inline size_t fpsIndex(int fps){for(size_t i=0;i<frameRates.size();++i)if(frameRates[i]==fps)return i;return 3;}
}
