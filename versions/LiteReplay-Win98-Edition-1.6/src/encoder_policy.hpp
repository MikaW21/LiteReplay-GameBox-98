#pragma once
#include <string>
#include <vector>
namespace lr {
struct EncoderChoice {std::string name;bool cpuFrames=false,software=false;};
inline std::string codecFamily(const std::string& codec){return codec.rfind("hevc",0)==0?"hevc":codec.rfind("av1",0)==0?"av1":"h264";}
inline std::vector<EncoderChoice> encoderChoices(const std::string& backend,const std::string& codec,unsigned vendor,bool allowSoftware){
    auto family=codecFamily(codec);std::vector<EncoderChoice> choices;
    if(backend=="nvenc")choices.push_back({family+"_nvenc",false,false});
    else if(backend=="amf")choices.push_back({family+"_amf",false,false});
    else if(backend=="mf_hw"){if(family=="h264")choices.push_back({"h264_mf",true,false});}
    else if(backend=="mf_sw"){if(family=="h264")choices.push_back({"h264_mf",true,true});}
    else {
        if(vendor==0x10de)choices.push_back({family+"_nvenc",false,false});
        if(vendor==0x1002)choices.push_back({family+"_amf",false,false});
        if(family=="h264")choices.push_back({"h264_mf",true,false});
    }
    if(allowSoftware&&family=="h264"&&backend!="mf_sw")choices.push_back({"h264_mf",true,true});
    return choices;
}
}
