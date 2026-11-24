#include "editor/media_probe.hpp"
#include <cassert>
#include <functional>
using namespace lc;
void fail(std::function<void()> f){bool bad=false;try{f();}catch(...){bad=true;}assert(bad);}
int main(){
 auto c=parseMediaProbe("source.mp4","[STREAM]\ncodec_type=video\nwidth=1920\nheight=1080\navg_frame_rate=30000/1001\nbit_rate=12000000\n[/STREAM]\n[STREAM]\ncodec_type=audio\nbit_rate=192000\n[/STREAM]\n[FORMAT]\nduration=10\nbit_rate=12500000\n[/FORMAT]\n");assert(c.width==1920&&c.height==1080&&c.audio&&c.mbps==12);assert(std::abs(c.fps-30000./1001)<1e-9);assert(frameRateText(c.fps)=="30000/1001");
 Project p;p.clips={c};p.settings=sourceLimit(p);enforceLimit(p,p.settings);auto e=p.settings;e.fps=30;fail([&]{enforceLimit(p,e);});e=p.settings;e.width=2560;fail([&]{enforceLimit(p,e);});e=p.settings;e.mbps=12.01;fail([&]{enforceLimit(p,e);});e=p.settings;e.width=1280;e.height=720;e.fps=24;e.mbps=6;enforceLimit(p,e);
 auto low=c;low.width=1280;low.height=720;low.fps=24;low.mbps=4;p.clips.push_back(low);auto cap=sourceLimit(p);assert(cap.width==1280&&cap.height==720&&cap.fps==24&&cap.mbps==4);auto clamped=clampExport(p,p.settings);enforceLimit(p,clamped);
 auto round=deserialize(serialize(p));assert(round.clips[0].fps==c.fps&&round.clips[1].mbps==4);
 auto old=deserialize("LITECUT 1\n1920 1080 60 20 0\n1\n\"old.mp4\" 10 0 10 100 1\n");assert(!metadataKnown(old.clips[0]));fail([&]{sourceLimit(old);});
 auto tagged=parseMediaProbe("new.mp4","[STREAM]\ncodec_type=video\nwidth=1280\nheight=720\navg_frame_rate=30/1\nbit_rate=2000000\n[/STREAM]\n[FORMAT]\nduration=2\nTAG:comment=LiteReplayRecordingV1;width=1280;height=720;fps=30;mbps=6\n[/FORMAT]\n");assert(tagged.mbps==6&&tagged.fps==30&&!tagged.estimatedBitrate);
 auto unknown=parseMediaProbe("unknown.mkv","[STREAM]\ncodec_type=video\nwidth=1280\nheight=720\navg_frame_rate=30/1\nbit_rate=N/A\n[/STREAM]\n[FORMAT]\nduration=2\n[/FORMAT]\n");assert(!metadataKnown(unknown));
 auto estimated=parseMediaProbe("estimated.mkv","[STREAM]\ncodec_type=video\nwidth=1280\nheight=720\navg_frame_rate=0/0\nr_frame_rate=30/1\n[/STREAM]\n[STREAM]\ncodec_type=audio\nbit_rate=192000\n[/STREAM]\n[FORMAT]\nduration=2\nbit_rate=6192000\n[/FORMAT]\n");assert(estimated.mbps==6&&estimated.estimatedBitrate&&estimated.fps==30);
 auto portrait=tagged;portrait.width=720;portrait.height=1280;p.clips={portrait};cap=sourceLimit(p);assert(cap.width==720&&cap.height==1280);portrait.width=719;p.clips={portrait};cap=sourceLimit(p);assert(cap.width==718);
 assert(mediaNumber("nan")==0&&mediaRate("0/0")==0);assert(frameRateText(60)=="60/1");fail([]{frameRateText(.00001);});assert(frameRateText(60000./1001)=="60000/1001");
 auto args=segmentArgs(c,Export{1280,720,24,6,1},"out.mp4");assert(std::find(args.begin(),args.end(),"-q:v")==args.end());assert(std::find(args.begin(),args.end(),"-maxrate")!=args.end());
}
