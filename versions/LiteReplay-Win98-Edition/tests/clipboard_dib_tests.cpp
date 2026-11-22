#include "clipboard_dib.hpp"
#include <cassert>
int main(){
 std::vector<uint8_t> bgra={1,2,3,255,4,5,6,255};auto d=lr::clipboardDib(bgra,1,2);
 assert(d.size()==48&&d[0]==40&&d[4]==1&&d[8]==2&&d[12]==1&&d[14]==24&&d[20]==8);
 assert(d[40]==4&&d[41]==5&&d[42]==6&&d[43]==0);
 assert(d[44]==1&&d[45]==2&&d[46]==3&&d[47]==0);
 bool failed=false;try{lr::clipboardDib({},1,1);}catch(...){failed=true;}assert(failed);
 std::vector<uint8_t> wide(3*4,255);d=lr::clipboardDib(wide,3,1);assert(d.size()==52&&d[49]==0&&d[50]==0&&d[51]==0);
}
