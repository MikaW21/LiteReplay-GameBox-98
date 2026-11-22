#pragma once
#include <pdh.h>
#include <pdhmsg.h>
#include <dxgi.h>
#include <wbemidl.h>
#include <numeric>
#include <cwctype>
#include <cstring>

namespace lr {
class PdhSamples {
    PDH_HQUERY query_{};
public:
    PdhSamples(){PdhOpenQueryW(nullptr,0,&query_);}
    ~PdhSamples(){if(query_)PdhCloseQuery(query_);}
    PDH_HCOUNTER add(const wchar_t* path) const {
        PDH_HCOUNTER counter{};
        if(!query_||PdhAddEnglishCounterW(query_,path,0,&counter)!=ERROR_SUCCESS)return nullptr;
        return counter;
    }
    bool collect() const {return query_&&PdhCollectQueryData(query_)==ERROR_SUCCESS;}
    static std::map<std::wstring,double> read(PDH_HCOUNTER counter){
        std::map<std::wstring,double> out;
        DWORD bytes=0,count=0;
        if(!counter||PdhGetFormattedCounterArrayW(counter,PDH_FMT_DOUBLE|PDH_FMT_NOCAP100,&bytes,&count,nullptr)!=PDH_STATUS(PDH_MORE_DATA)||
            bytes>16*1024*1024)return out;
        std::vector<uint64_t> storage((bytes+7)/8);
        auto items=reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(storage.data());
        if(PdhGetFormattedCounterArrayW(counter,PDH_FMT_DOUBLE|PDH_FMT_NOCAP100,&bytes,&count,items)!=ERROR_SUCCESS)return out;
        for(DWORD i=0;i<count;++i){
            const auto& value=items[i].FmtValue;
            if(items[i].szName&&(value.CStatus==PDH_CSTATUS_VALID_DATA||value.CStatus==PDH_CSTATUS_NEW_DATA)&&
                std::isfinite(value.doubleValue)&&value.doubleValue>=0)out[items[i].szName]=value.doubleValue;
        }
        return out;
    }
    static double sum(const std::map<std::wstring,double>& values){
        if(values.empty())return -1;
        double total=0;
        for(const auto& value:values)total+=value.second;
        return total;
    }
};
inline uint64_t dedicatedVideoBytes(){
    IDXGIFactory1* factory=nullptr;
    if(FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),reinterpret_cast<void**>(&factory))))return 0;
    uint64_t total=0;
    IDXGIAdapter1* adapter=nullptr;
    for(UINT i=0;SUCCEEDED(factory->EnumAdapters1(i,&adapter));++i){
        DXGI_ADAPTER_DESC1 desc{};
        if(SUCCEEDED(adapter->GetDesc1(&desc))&&!(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE))total+=desc.DedicatedVideoMemory;
        adapter->Release();
    }
    factory->Release();
    return total;
}
struct SensorReadings {
    double cpuTemp=-1,cpuMhz=-1,gpuTemp=-1,gpuMhz=-1;
};
class NvidiaSensors {
    using Device=struct nvmlDevice_st*;
    using Action=int(*)();
    using Count=int(*)(unsigned*);
    using Index=int(*)(unsigned,Device*);
    using Reading=int(*)(Device,unsigned,unsigned*);
    HMODULE library_{};
    Action shutdown_{};
    Count count_{};
    Index index_{};
    Reading temp_{},clock_{};
    bool tried_=false;
    template<class T>T function(const char* name)const{
        auto address=GetProcAddress(library_,name);
        T result{};
        static_assert(sizeof(result)==sizeof(address));
        std::memcpy(&result,&address,sizeof(result));
        return result;
    }
    void initialize(){
        tried_=true;
        library_=LoadLibraryExW(L"nvml.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!library_)return;
        auto init=function<Action>("nvmlInit_v2");
        if(!init||init()!=0)return;
        shutdown_=function<Action>("nvmlShutdown");
        count_=function<Count>("nvmlDeviceGetCount_v2");
        index_=function<Index>("nvmlDeviceGetHandleByIndex_v2");
        temp_=function<Reading>("nvmlDeviceGetTemperature");
        clock_=function<Reading>("nvmlDeviceGetClockInfo");
    }
public:
    ~NvidiaSensors(){if(shutdown_)shutdown_();if(library_)FreeLibrary(library_);}
    SensorReadings sample(){
        if(!tried_)initialize();
        SensorReadings result;
        unsigned count=0;
        if(!count_||!index_||count_(&count)!=0)return result;
        for(unsigned i=0;i<count;++i){
            Device device{};
            if(index_(i,&device)!=0)continue;
            unsigned value=0;
            // NVML_TEMPERATURE_GPU and NVML_CLOCK_GRAPHICS are both zero.
            if(temp_&&temp_(device,0,&value)==0)result.gpuTemp=std::max(result.gpuTemp,double(value));
            if(clock_&&clock_(device,0,&value)==0&&value)result.gpuMhz=std::max(result.gpuMhz,double(value));
        }
        return result;
    }
};
class HardwareSensors {
    HRESULT com_=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    IWbemServices* service_{};
    ULONGLONG next_{},nextConnect_{};
    SensorReadings last_;
    NvidiaSensors nvidia_;
    static std::wstring get(IWbemClassObject* object,const wchar_t* key){
        VARIANT value;VariantInit(&value);
        std::wstring result;
        if(SUCCEEDED(object->Get(key,0,&value,nullptr,nullptr))&&value.vt==VT_BSTR&&value.bstrVal)result=value.bstrVal;
        VariantClear(&value);return result;
    }
    bool connect(){
        IWbemLocator* locator=nullptr;
        if(FAILED(com_)||FAILED(CoCreateInstance(CLSID_WbemLocator,nullptr,CLSCTX_INPROC_SERVER,
            IID_IWbemLocator,reinterpret_cast<void**>(&locator))))return false;
        BSTR ns=SysAllocString(L"ROOT\\LibreHardwareMonitor");
        const auto result=locator->ConnectServer(ns,nullptr,nullptr,nullptr,WBEM_FLAG_CONNECT_USE_MAX_WAIT,nullptr,nullptr,&service_);
        SysFreeString(ns);locator->Release();
        if(FAILED(result))return false;
        CoSetProxyBlanket(service_,RPC_C_AUTHN_WINNT,RPC_C_AUTHZ_NONE,nullptr,RPC_C_AUTHN_LEVEL_CALL,
            RPC_C_IMP_LEVEL_IMPERSONATE,nullptr,EOAC_NONE);
        return true;
    }
public:
    ~HardwareSensors(){if(service_)service_->Release();if(SUCCEEDED(com_))CoUninitialize();}
    SensorReadings sample(){
        const auto now=GetTickCount64();
        if(now<next_)return last_;
        next_=now+4000;last_={};
        const auto nvidia=nvidia_.sample();
        last_.gpuTemp=nvidia.gpuTemp;last_.gpuMhz=nvidia.gpuMhz;
        if(!service_){
            if(now<nextConnect_)return last_;
            nextConnect_=now+15000;
            if(!connect())return last_;
        }
        BSTR language=SysAllocString(L"WQL");
        BSTR query=SysAllocString(L"SELECT Identifier, Name, SensorType, Value FROM Sensor WHERE SensorType='Temperature' OR SensorType='Clock'");
        IEnumWbemClassObject* rows=nullptr;
        auto status=service_->ExecQuery(language,query,WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY,nullptr,&rows);
        SysFreeString(language);SysFreeString(query);
        if(FAILED(status)){service_->Release();service_=nullptr;return last_;}
        auto deadline=GetTickCount64()+500;
        while(GetTickCount64()<deadline){
            IWbemClassObject* object=nullptr;ULONG count=0;
            if(FAILED(rows->Next(100,1,&object,&count))||!count)break;
            auto id=get(object,L"Identifier"),name=get(object,L"Name"),type=get(object,L"SensorType");
            std::transform(name.begin(),name.end(),name.begin(),[](wchar_t c){return std::towlower(c);});
            VARIANT raw,number;VariantInit(&raw);VariantInit(&number);
            if(SUCCEEDED(object->Get(L"Value",0,&raw,nullptr,nullptr))&&raw.vt!=VT_NULL&&raw.vt!=VT_EMPTY&&
                SUCCEEDED(VariantChangeType(&number,&raw,0,VT_R8))&&std::isfinite(number.dblVal)&&number.dblVal>0){
                const bool cpu=id.rfind(L"/intelcpu/",0)==0||id.rfind(L"/amdcpu/",0)==0;
                const bool gpu=id.rfind(L"/gpu-",0)==0;
                if(cpu&&type==L"Temperature")last_.cpuTemp=std::max(last_.cpuTemp,number.dblVal);
                if(gpu&&type==L"Temperature")last_.gpuTemp=std::max(last_.gpuTemp,number.dblVal);
                if(cpu&&type==L"Clock"&&name.find(L"core")!=std::wstring::npos)last_.cpuMhz=std::max(last_.cpuMhz,number.dblVal);
                if(gpu&&type==L"Clock"&&name.find(L"core")!=std::wstring::npos)last_.gpuMhz=std::max(last_.gpuMhz,number.dblVal);
            }
            VariantClear(&raw);VariantClear(&number);object->Release();
        }
        rows->Release();return last_;
    }
};
}
