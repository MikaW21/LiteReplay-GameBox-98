#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <locale>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace lc {
struct Clip {std::string path;double duration=0,in=0,out=0;int volume=100;bool audio=false;int width=0,height=0;double fps=0,mbps=0;bool estimatedBitrate=false;};
struct Export {int width=1920,height=1080;double fps=60,mbps=20;int encoder=0;};
struct Project {Export settings;std::vector<Clip> clips;};
inline std::string decimal(double n){std::ostringstream o;o.imbue(std::locale::classic());o<<std::fixed<<std::setprecision(6)<<n;return o.str();}
inline void validate(const Clip& c){if(c.path.empty()||c.path.size()>32768||!std::isfinite(c.duration)||!std::isfinite(c.in)||!std::isfinite(c.out)||c.duration<=0||c.duration>604800||c.in<0||c.out>c.duration+.001||c.out-c.in<.04||c.volume<0||c.volume>200)throw std::runtime_error("Invalid clip range / volume");}
inline void validate(const Export& e){if(e.width<2||e.height<2||e.width>16384||e.height>16384||e.width%2||e.height%2||!std::isfinite(e.fps)||e.fps<=0||e.fps>1000||!std::isfinite(e.mbps)||e.mbps<=0||e.mbps>1000||e.encoder<0||e.encoder>2)throw std::runtime_error("Invalid export settings");}
inline bool metadataKnown(const Clip& c){return c.width>=2&&c.height>=2&&c.width<=16384&&c.height<=16384&&std::isfinite(c.fps)&&c.fps>0&&c.fps<=1000&&std::isfinite(c.mbps)&&c.mbps>0&&c.mbps<=1000;}
inline Export sourceLimit(const Project& p){
 if(p.clips.empty())throw std::runtime_error("No source videos / 尚无视频");
 Export limit;limit.width=p.clips[0].width;limit.height=p.clips[0].height;limit.fps=1000;limit.mbps=1000;
 double scale=1;
 for(const auto& c:p.clips){if(!metadataKnown(c))throw std::runtime_error("Source specifications unavailable; refresh media information / 无法确定原视频规格，请重新读取规格");scale=std::min({scale,double(c.width)/p.clips[0].width,double(c.height)/p.clips[0].height});limit.fps=std::min(limit.fps,c.fps);limit.mbps=std::min(limit.mbps,c.mbps);}
 limit.width=std::max(2,int(limit.width*scale)/2*2);limit.height=std::max(2,int(limit.height*scale)/2*2);limit.mbps=std::floor(limit.mbps*1000000.)/1000000.;return limit;
}
inline void enforceLimit(const Project& p,const Export& e){validate(e);auto cap=sourceLimit(p);if(e.width>cap.width||e.height>cap.height||e.fps>cap.fps+1e-7||e.mbps>cap.mbps+1e-7)throw std::runtime_error("Export exceeds source limits / 导出规格不能超过原视频上限");}
inline Export clampExport(const Project& p,Export e){auto cap=sourceLimit(p);double scale=std::min({1.0,double(cap.width)/e.width,double(cap.height)/e.height});e.width=std::max(2,int(e.width*scale)/2*2);e.height=std::max(2,int(e.height*scale)/2*2);e.fps=std::min(e.fps,cap.fps);e.mbps=std::min(e.mbps,cap.mbps);return e;}

inline int recommendedMbps(int width,int fps){return std::clamp(int(std::ceil((width==1280?8.0:width==2560?24.0:14.0)*std::max(1.0,fps/30.0))),2,100);}
inline double total(const Project& p){double value=0;for(auto& c:p.clips)value+=c.out-c.in;return value;}
inline std::string serialize(const Project& p){validate(p.settings);if(p.clips.size()>256)throw std::runtime_error("Too many clips");std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(17)<<"LITECUT 2\n"<<p.settings.width<<' '<<p.settings.height<<' '<<p.settings.fps<<' '<<p.settings.mbps<<' '<<p.settings.encoder<<'\n'<<p.clips.size()<<'\n';o<<std::setprecision(17);for(const auto& c:p.clips){validate(c);o<<std::quoted(c.path)<<' '<<c.duration<<' '<<c.in<<' '<<c.out<<' '<<c.volume<<' '<<c.audio<<' '<<c.width<<' '<<c.height<<' '<<c.fps<<' '<<c.mbps<<' '<<c.estimatedBitrate<<'\n';}return o.str();}
inline Project deserialize(const std::string& data){if(data.size()>10*1024*1024)throw std::runtime_error("Project too large");std::istringstream s(data);s.imbue(std::locale::classic());std::string tag;int version=0;Project p;size_t count=0;if(!(s>>tag>>version)||tag!="LITECUT"||(version!=1&&version!=2))throw std::runtime_error("Unsupported project");auto& e=p.settings;if(!(s>>e.width>>e.height>>e.fps>>e.mbps>>e.encoder>>count)||count>256)throw std::runtime_error("Invalid project header");validate(e);for(size_t i=0;i<count;++i){Clip c;int audio;if(!(s>>std::quoted(c.path)>>c.duration>>c.in>>c.out>>c.volume>>audio)||audio<0||audio>1)throw std::runtime_error("Invalid project clip");c.audio=audio!=0;if(version==2){int estimate=0;if(!(s>>c.width>>c.height>>c.fps>>c.mbps>>estimate)||estimate<0||estimate>1)throw std::runtime_error("Invalid source specifications");c.estimatedBitrate=estimate!=0;}validate(c);p.clips.push_back(c);}s>>std::ws;if(!s.eof())throw std::runtime_error("Unexpected project content");return p;}
inline std::string frameRateText(double fps){int bestN=0,bestD=1;double error=1e9;for(int d=1;d<=1001;++d){int n=int(std::floor(fps*d+1e-8));if(n<1||n>65535)continue;double gap=fps-double(n)/d;if(gap>=-1e-9&&gap<error){error=gap;bestN=n;bestD=d;}}if(!bestN)throw std::runtime_error("Unsupported source frame rate / 原片帧率过低，无法重编码");int g=std::gcd(bestN,bestD);return std::to_string(bestN/g)+"/"+std::to_string(bestD/g);}
inline std::vector<std::string> segmentArgs(const Clip& c,const Export& e,const std::string& output){validate(c);validate(e);if(metadataKnown(c)){Project single;single.clips={c};enforceLimit(single,e);}std::vector<std::string> a={"-hide_banner","-nostdin","-n","-threads","2","-ss",decimal(c.in),"-i",c.path};if(!c.audio){a.insert(a.end(),{"-f","lavfi","-i","anullsrc=r=48000:cl=stereo"});}a.insert(a.end(),{"-t",decimal(c.out-c.in),"-map","0:v:0","-map",c.audio?"0:a:0":"1:a:0","-vf","scale="+std::to_string(e.width)+":"+std::to_string(e.height)+":flags=lanczos:force_original_aspect_ratio=decrease,pad="+std::to_string(e.width)+":"+std::to_string(e.height)+":(ow-iw)/2:(oh-ih)/2,setsar=1,fps="+frameRateText(e.fps)+",format=yuv420p","-filter_threads","1","-af","aresample=48000:async=1:first_pts=0,volume="+decimal(c.volume/100.0),"-c:v",e.encoder==1?"mpeg4":e.encoder==2?"hevc_nvenc":"h264_nvenc"});
if(e.encoder!=1)a.insert(a.end(),{"-preset","p4","-b:v",decimal(e.mbps)+"M"});else a.insert(a.end(),{"-b:v",decimal(e.mbps)+"M","-threads","2"});
a.insert(a.end(),{"-maxrate",decimal(e.mbps)+"M","-bufsize",decimal(e.mbps*2)+"M"});
a.insert(a.end(),{"-pix_fmt","yuv420p","-bf","0","-video_track_timescale","90000","-c:a","aac","-b:a","192k","-ar","48000","-ac","2","-map_metadata","-1","-movflags","+faststart",output});return a;}
// Microsoft command-line quoting. No cmd.exe or shell is involved.
inline std::wstring quoteWindows(const std::wstring& s){std::wstring out=L"\"";size_t slashes=0;for(wchar_t ch:s){if(ch==L'\\'){++slashes;continue;}if(ch==L'\"'){out.append(slashes*2+1,L'\\');out+=ch;}else{out.append(slashes,L'\\');out+=ch;}slashes=0;}out.append(slashes*2,L'\\');out+=L'\"';return out;}
}
