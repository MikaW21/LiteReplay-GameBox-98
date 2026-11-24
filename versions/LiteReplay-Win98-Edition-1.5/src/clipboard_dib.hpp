#pragma once
#include "screenshot_geometry.hpp"
#include <cstdint>
#include <vector>
namespace lr {
// CF_DIB: 40-byte BITMAPINFOHEADER + bottom-up, DWORD-aligned BGR24 rows.
inline std::vector<uint8_t> clipboardDib(const std::vector<uint8_t>& bgra,int w,int h){
 if(bgra.size()!=shotBytes(w,h))throw std::runtime_error("Invalid screenshot pixels");
 const size_t stride=(size_t(w)*3+3)&~size_t(3),length=stride*size_t(h);
 std::vector<uint8_t> out(40+length,0);
 auto u32=[&](size_t p,uint32_t n){for(unsigned i=0;i<4;++i)out[p+i]=uint8_t(n>>(8*i));};
 u32(0,40);u32(4,uint32_t(w));u32(8,uint32_t(h));out[12]=1;out[14]=24;u32(20,uint32_t(length));
 for(int y=0;y<h;++y)for(int x=0;x<w;++x){size_t src=(size_t(y)*w+x)*4,dst=40+size_t(h-1-y)*stride+size_t(x)*3;for(int c=0;c<3;++c)out[dst+c]=bgra[src+c];}
 return out;
}
}
