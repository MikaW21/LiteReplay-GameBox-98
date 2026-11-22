#include "editor/history.hpp"
#include <cassert>
int main(){lc::Project p;lc::History h;h.reset(p);p.clips.push_back({"clip.mp4",10,0,10,100,true});h.record(p);p.clips[0].out=5;h.record(p);assert(h.undo(p)&&p.clips[0].out==10);assert(h.undo(p)&&p.clips.empty());assert(!h.undo(p));assert(h.redo(p)&&p.clips.size()==1);p.clips[0].in=1;h.record(p);assert(!h.redo(p));
for(int i=0;i<100;++i){p.clips[0].volume=i;h.record(p);}int count=0;while(h.undo(p))++count;assert(count==63);assert(h.bytes()<=16*1024*1024);
p.clips[0].volume=100;auto args=lc::copyArgs(p.clips[0],"out.mp4");assert(std::find(args.begin(),args.end(),"copy")!=args.end());assert(std::find(args.begin(),args.end(),"-n")!=args.end());assert(std::find(args.begin(),args.end(),"-vf")==args.end());p.clips[0].volume=50;bool failed=false;try{lc::copyArgs(p.clips[0],"out.mp4");}catch(...){failed=true;}assert(failed);
p.settings.encoder=2;args=lc::segmentArgs(p.clips[0],p.settings,"out.mp4");assert(std::find(args.begin(),args.end(),"hevc_nvenc")!=args.end());}
