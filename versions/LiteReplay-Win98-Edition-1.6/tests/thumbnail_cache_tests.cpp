#include "thumbnail_cache.hpp"
#include <cassert>
int main(){
 lr::ThumbnailCache c;c.limit(8);
 lr::ThumbnailPixels a{1,1,{1}},b{1,1,{2}},d{1,1,{3}};
 c.put(L"a",a);c.put(L"b",b);assert(c.bytes()==8);assert(c.get(L"a")->pixels[0]==1);
 c.put(L"d",d);assert(!c.get(L"b"));assert(c.get(L"a"));assert(c.size()==2);
 c.put(L"a",d);assert(c.bytes()==8&&c.get(L"a")->pixels[0]==3);
 c.limit(4);assert(c.size()==1&&c.get(L"a"));c.erase(L"a");assert(c.bytes()==0);
 for(int i=0;i<200;++i)c.put(std::to_wstring(i),{});
 assert(c.size()==128&&c.bytes()==0);assert(!c.get(L"0"));assert(c.get(L"199"));
 c.clear();assert(c.size()==0);c.limit(0);c.put(L"x",a);assert(!c.get(L"x"));
}
