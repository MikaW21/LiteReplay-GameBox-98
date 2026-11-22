#pragma once
#include "core.hpp"
namespace lc {
inline double mediaNumber(const std::string& s){try{size_t n;double v=std::stod(s,&n);return n==s.size()&&std::isfinite(v)&&v>=0?v:0;}catch(...){return 0;}}
inline double mediaRate(const std::string& s){auto p=s.find('/');if(p==std::string::npos)return mediaNumber(s);double d=mediaNumber(s.substr(p+1));return d>0?mediaNumber(s.substr(0,p))/d:0;}
inline Clip parseMediaProbe(const std::string& path,const std::string& data){
 Clip c;c.path=path;std::istringstream input(data);std::string line,type,comment;int width=0,height=0;double avg=0,rate=0,bps=0,videoBps=0,audioBps=0,formatBps=0;bool video=false,inStream=false;
 while(std::getline(input,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line=="[STREAM]"){inStream=true;type.clear();width=height=0;avg=rate=bps=0;continue;}if(line=="[/STREAM]"){if(type=="video"&&!video){video=true;c.width=width;c.height=height;c.fps=avg>0?avg:rate;videoBps=bps;}if(type=="audio"){c.audio=true;audioBps+=bps>0?bps:192000;}inStream=false;continue;}
 auto eq=line.find('=');if(eq==std::string::npos)continue;auto key=line.substr(0,eq),value=line.substr(eq+1);
 if(inStream){if(key=="codec_type")type=value;else if(key=="width")width=int(std::min(16385.0,mediaNumber(value)));else if(key=="height")height=int(std::min(16385.0,mediaNumber(value)));else if(key=="avg_frame_rate")avg=mediaRate(value);else if(key=="r_frame_rate")rate=mediaRate(value);else if(key=="bit_rate")bps=mediaNumber(value);else if(key=="rotation"&&(value=="90"||value=="-90"||value=="270"))std::swap(width,height);}
 else {if(key=="duration")c.duration=mediaNumber(value);else if(key=="bit_rate")formatBps=mediaNumber(value);else if(key=="TAG:comment"||key=="TAG:COMMENT")comment=value;}}
 c.out=c.duration;c.mbps=videoBps/1000000.;if(c.mbps<=0&&formatBps>audioBps){c.mbps=(formatBps-audioBps)/1000000.;c.estimatedBitrate=true;}
 // Our replay writer embeds the configured bitrate and frame rate, not the user's current settings.
 if(comment.rfind("LiteReplayRecordingV1;",0)==0){std::istringstream fields(comment);std::string field;double recordedRate=0,recordedMbps=0;int recordedW=0,recordedH=0;while(std::getline(fields,field,';')){auto e=field.find('=');if(e==std::string::npos)continue;auto k=field.substr(0,e);double v=mediaNumber(field.substr(e+1));if(k=="width")recordedW=int(std::min(16385.,v));if(k=="height")recordedH=int(std::min(16385.,v));if(k=="fps")recordedRate=v;if(k=="mbps")recordedMbps=v;}if(recordedW==c.width&&recordedH==c.height&&recordedRate>0&&recordedRate<=1000&&recordedMbps>0&&recordedMbps<=100){c.fps=recordedRate;c.mbps=recordedMbps;c.estimatedBitrate=false;}}
 if(!video)throw std::runtime_error("No video stream / 未找到视频流");validate(c);return c;
}
}
