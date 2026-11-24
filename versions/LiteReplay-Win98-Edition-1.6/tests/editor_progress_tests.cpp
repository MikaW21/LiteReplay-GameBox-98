#include "editor/progress.hpp"
#include <cassert>
#include <vector>
int main(){lc::ProgressLines p;std::vector<double> times;auto cb=[&](double t){times.push_back(t);};p.feed("frame=2\nout_t",cb);assert(times.empty());p.feed("ime_us=1250000\r\nprogress=continue\n",cb);assert(times.size()==1&&times[0]==1.25);p.feed("out_time_us=N/A\nout_time_us=-1\nout_time_us=nan\nout_time_us=100junk\n",cb);assert(times.size()==1);p.feed("out_time_us=2500000\nout_time_us=300",cb);p.feed("0000\nprogress=end\n",cb);assert(times.size()==3&&times[1]==2.5&&times[2]==3);p.feed(std::string(10000,'a'),cb);p.feed("out_time_us=4000000\n",cb);assert(times.back()==4);}
