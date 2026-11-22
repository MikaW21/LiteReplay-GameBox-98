#pragma once
#include <array>
#include <algorithm>
#include <string>
#include <sstream>
#include <stdexcept>
#include <locale>
namespace lr {
inline constexpr std::array<int,10> eqFrequencies{31,62,125,250,500,1000,2000,4000,8000,16000};
inline constexpr const char* eqMarker="# LiteReplay EQ v1\n";
inline constexpr const char* eqBegin="# BEGIN LiteReplay managed EQ";
inline constexpr const char* eqEnd="# END LiteReplay managed EQ";
inline std::string endpointGuid(const std::string& endpoint){
    const auto start=endpoint.rfind('{');
    if(start==std::string::npos||endpoint.size()-start!=38||endpoint.back()!='}')throw std::runtime_error("Cannot identify endpoint GUID");
    auto guid=endpoint.substr(start);
    for(size_t i=1;i<37;++i){bool dash=i==9||i==14||i==19||i==24;char c=guid[i];if(dash?c!='-':!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))throw std::runtime_error("Invalid endpoint GUID");}
    return guid;
}
inline int eqHeadroom(const std::array<int,10>& gains){
    for(int g:gains)if(g < -12 || g > 12)throw std::runtime_error("EQ gain outside -12..12 dB");
    int peak=*std::max_element(gains.begin(),gains.end());return peak>0?-(peak+1):0;
}
inline std::string eqText(const std::string& endpoint,const std::array<int,10>& gains,bool enabled){
    const auto guid=endpointGuid(endpoint);const int preamp=eqHeadroom(gains);
    std::ostringstream out;out.imbue(std::locale::classic());out<<eqMarker;
    if(!enabled)return out.str()+"# Bypassed: no filters\n";
    out<<"Device: "<<guid<<"\nStage: post-mix\nChannel: all\nPreamp: "<<preamp<<" dB\nGraphicEQ: ";
    for(size_t i=0;i<gains.size();++i){if(i)out<<"; ";out<<eqFrequencies[i]<<' '<<gains[i];}out<<'\n';return out.str();
}
// Only remove the exact block owned by LiteReplay; never replace other filters.
inline std::string removeEqBlock(const std::string& source){
    if(source.find('\0')!=std::string::npos)throw std::runtime_error("UTF-16/binary APO config is unsupported; use UTF-8");
    auto begin=source.find(eqBegin),end=source.find(eqEnd);
    if(begin==std::string::npos&&end==std::string::npos)return source;
    if(begin==std::string::npos||end==std::string::npos||end<begin||source.find(eqBegin,begin+1)!=std::string::npos||source.find(eqEnd,end+1)!=std::string::npos)
        throw std::runtime_error("Ambiguous LiteReplay block; refusing to modify config");
    if(begin&&source[begin-1]!='\n')throw std::runtime_error("Invalid LiteReplay block boundary");
    const std::string canonical=std::string(eqBegin)+"\nDevice: all\nStage: post-mix\nChannel: all\nInclude: LiteReplay-EQ.txt\n"+eqEnd;
    auto block=source.substr(begin,end+std::char_traits<char>::length(eqEnd)-begin);
    block.erase(std::remove(block.begin(),block.end(),'\r'),block.end());
    if(block!=canonical)throw std::runtime_error("LiteReplay block was edited externally; refusing to remove it");
    end+=std::char_traits<char>::length(eqEnd);if(end<source.size()&&source[end]=='\r')++end;if(end<source.size()&&source[end]=='\n')++end;
    return source.substr(0,begin)+source.substr(end);
}
inline std::string attachEqBlock(const std::string& source){
    auto clean=removeEqBlock(source);
    if(clean.find("LiteReplay-EQ.txt")!=std::string::npos)throw std::runtime_error("An unmanaged LiteReplay-EQ.txt include already exists");
    if(!clean.empty()&&clean.back()!='\n')clean+='\n';
    return clean+eqBegin+"\nDevice: all\nStage: post-mix\nChannel: all\nInclude: LiteReplay-EQ.txt\n"+eqEnd+"\n";
}
}
