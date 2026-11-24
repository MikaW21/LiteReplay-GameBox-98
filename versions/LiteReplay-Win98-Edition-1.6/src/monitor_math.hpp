#pragma once
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
namespace lr {
// Aggregate each physical engine across processes, then select the busiest.
// Different engines must not be added together into a misleading >100% GPU.
struct GpuUsage {
    std::map<std::wstring,double> system,process,encode;
    void add(const std::wstring& name,double value,unsigned long pid) {
        if(!std::isfinite(value)||value<0)return;
        auto marker=name.find(L"_luid_");
        if(marker==std::wstring::npos||name.find(L"_eng_")==std::wstring::npos)return;
        auto key=name.substr(marker);
        // PDH may append an instance duplicate suffix; it is not an engine ID.
        auto duplicate=key.find(L'#');if(duplicate!=std::wstring::npos)key.resize(duplicate);
        system[key]+=value;
        if(name.rfind(L"pid_"+std::to_wstring(pid)+L"_",0)==0){
            process[key]+=value;
            if(key.find(L"engtype_VideoEncode")!=std::wstring::npos)encode[key]+=value;
        }
    }
    static double peak(const std::map<std::wstring,double>& values) {
        if(values.empty())return -1;
        double out=0;for(const auto& item:values)out=std::max(out,item.second);
        return std::clamp(out,0.0,100.0);
    }
};
inline double cpuBusy(unsigned long long total,unsigned long long idle){
    if(!total||idle>total)return -1;
    return 100.0*double(total-idle)/double(total);
}
}
