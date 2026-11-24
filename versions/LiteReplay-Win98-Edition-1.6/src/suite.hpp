#pragma once
#include <algorithm>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
namespace lr {
enum class RunMode { Balanced, Low, Performance };
struct ResourcePolicy {
    int monitorMs, uiMs, previewHeight, previewFps, exportThreads;
    size_t thumbnailMiB;
};
inline RunMode parseMode(int n){return n==1?RunMode::Low:n==2?RunMode::Performance:RunMode::Balanced;}
inline ResourcePolicy policy(RunMode m){
    if(m==RunMode::Low)return {2000,2000,360,15,1,8};
    if(m==RunMode::Performance)return {250,250,1440,60,4,64};
    return {1000,1000,720,30,2,32};
}
// Page changes and policy changes contain no recording configuration or restart action.
struct SuiteState {
    RunMode mode=RunMode::Balanced;
    bool english=false;
    int page=0;
    bool needsMetrics(bool mainVisible,bool hudVisible)const{
        return hudVisible || (mainVisible && (page==0||page==5));
    }
};
class Catalog {
    std::map<std::string,std::string> values_;
public:
    void read(std::istream& in){
        std::map<std::string,std::string> next;std::string line;
        while(std::getline(in,line)){
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            if(line.empty()||line[0]=='#')continue;
            auto p=line.find('=');
            if(p==std::string::npos||p==0)throw std::runtime_error("Invalid language entry");
            auto key=line.substr(0,p),value=line.substr(p+1);
            if(!next.emplace(key,value).second)throw std::runtime_error("Duplicate language key");
        }
        if(in.bad())throw std::runtime_error("Cannot read language file");
        values_=std::move(next);
    }
    const std::string& get(const std::string& key)const{
        auto it=values_.find(key);if(it==values_.end())throw std::runtime_error("Missing language key: "+key);
        return it->second;
    }
    const auto& values()const{return values_;}
};
}
