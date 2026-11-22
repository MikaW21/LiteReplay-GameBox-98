#pragma once
#include <algorithm>
#include <cstddef>
#include <stdexcept>
namespace lr {
struct ShotRect {int left,top,right,bottom;};
inline ShotRect shotRect(int ax,int ay,int bx,int by,int width,int height){
 return {std::clamp(std::min(ax,bx),0,width),std::clamp(std::min(ay,by),0,height),std::clamp(std::max(ax,bx),0,width),std::clamp(std::max(ay,by),0,height)};
}
inline size_t shotBytes(int width,int height){
 if(width<=0||height<=0||size_t(width)>64u*1024*1024/4/size_t(height))throw std::runtime_error("Screenshot exceeds 64 MiB pixel limit / 截图尺寸超出限制");
 return size_t(width)*size_t(height)*4;
}
}
