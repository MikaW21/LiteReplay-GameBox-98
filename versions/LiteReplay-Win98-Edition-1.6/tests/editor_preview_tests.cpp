#include "editor/preview_range.hpp"
#include <cassert>
int main(){
    auto r=lc::previewRange(0,10.05,100000000);assert(r.start==0&&r.end==100000000);
    r=lc::previewRange(20,40,600000000);assert(r.seek(25)==250000000);
    assert(r.seek(0)==200000000&&r.seek(60)==399999999&&r.seek(40)==399999999);
    assert(r.sourceTime(250000000)==25&&r.sourceTime(-1)==20&&r.sourceTime(900000000)==40);
    r=lc::previewRange(0,1,1);assert(r.seek(1)==0&&r.end==1);
    for(auto pair:{std::pair<double,double>{10,20},{0,0},{-1,2},{0,std::numeric_limits<double>::infinity()},{std::numeric_limits<double>::quiet_NaN(),2}}){
        bool rejected=false;try{lc::previewRange(pair.first,pair.second,100000000);}catch(...){rejected=true;}assert(rejected);
    }
    r=lc::previewRange(0,1e200,100000000);assert(r.end==100000000);
}
