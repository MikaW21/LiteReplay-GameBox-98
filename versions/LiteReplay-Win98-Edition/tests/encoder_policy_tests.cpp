#include "encoder_policy.hpp"
#include <cassert>
#include <iostream>
int main(){using namespace lr;
 auto n=encoderChoices("auto","h264_nvenc",0x10de,false);assert(n[0].name=="h264_nvenc");for(auto& x:n)assert(!x.software);
 auto a=encoderChoices("auto","h264_nvenc",0x1002,false);assert(a[0].name=="h264_amf"&&a[1].name=="h264_mf"&&!a[1].software);
 auto i=encoderChoices("auto","h264_nvenc",0x8086,false);assert(i.size()==1&&i[0].name=="h264_mf"&&!i[0].software);
 auto c=encoderChoices("auto","h264_nvenc",0x8086,true);assert(c.size()==2&&c.back().software&&c.back().cpuFrames);
 auto h=encoderChoices("auto","hevc_nvenc",0x1002,true);assert(h.size()==1&&h[0].name=="hevc_amf");
 assert(encoderChoices("mf_sw","hevc_nvenc",0x1002,true).empty());
 assert(encoderChoices("mf_sw","h264_nvenc",0x1002,false).size()==1);
 auto v=encoderChoices("amf","av1_nvenc",0x1002,false);assert(v.size()==1&&v[0].name=="av1_amf");
 std::cout<<"PASS: AMD/Intel/NVIDIA policy; explicit software consent; no silent codec change\n";
}
