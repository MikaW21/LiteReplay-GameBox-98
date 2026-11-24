#include "screenshot_geometry.hpp"
#include <cassert>
int main(){
 auto a=lr::shotRect(90,80,10,20,100,100);assert(a.left==10&&a.top==20&&a.right==90&&a.bottom==80);
 a=lr::shotRect(-20,-30,120,200,100,80);assert(a.left==0&&a.top==0&&a.right==100&&a.bottom==80);
 a=lr::shotRect(20,20,20,20,100,100);assert(a.left==a.right&&a.top==a.bottom);
 assert(lr::shotBytes(2560,1440)==14745600);
 for(auto pair:{std::pair<int,int>{0,1},{1,-1},{100000,100000}}){bool failed=false;try{lr::shotBytes(pair.first,pair.second);}catch(...){failed=true;}assert(failed);}
}
