#pragma once
#include "common.hpp"
#include "monitor_math.hpp"
#include "monitor_sources.hpp"
#include <psapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <condition_variable>
namespace lr {
struct PerformanceSnapshot {
    double systemCpu=-1, processCpu=-1, systemGpu=-1, processGpu=-1, encodeGpu=-1;
    uint64_t usedRam=0,totalRam=0,privateRam=0,workingRam=0;
    bool ramValid=false,processRamValid=false;
    unsigned long long sampledAt=0;
    std::map<std::wstring,double> cores;
    SensorReadings sensors;
    double vram=-1,diskRead=-1,diskWrite=-1,netIn=-1,netOut=-1,targetCpu=-1,targetGpu=-1;
    uint64_t vramTotal=0,targetRam=0;
    DWORD target=0;
    bool targetRamValid=false;
};
class Performance {
    std::atomic<bool> stop_{false};
    std::mutex mutex_;
    PerformanceSnapshot snapshot_;
    std::thread worker_;
    std::mutex waitMutex_;
    std::condition_variable wake_;
    void nap(int ms){std::unique_lock<std::mutex> lock(waitMutex_);wake_.wait_for(lock,std::chrono::milliseconds(ms),[this]{return stop_.load();});}
    static uint64_t time(FILETIME t){return uint64_t(t.dwLowDateTime)|(uint64_t(t.dwHighDateTime)<<32);}
    void run(){
        PdhSamples base,detail;
        auto gpu=base.add(L"\\GPU Engine(*)\\Utilization Percentage");
        auto cores=detail.add(L"\\Processor Information(*)\\% Processor Time");
        auto mhz=detail.add(L"\\Processor Information(*)\\Processor Frequency");
        auto vram=detail.add(L"\\GPU Adapter Memory(*)\\Dedicated Usage");
        auto diskRead=detail.add(L"\\PhysicalDisk(*)\\Disk Read Bytes/sec");
        auto diskWrite=detail.add(L"\\PhysicalDisk(*)\\Disk Write Bytes/sec");
        auto netIn=detail.add(L"\\Network Interface(*)\\Bytes Received/sec");
        auto netOut=detail.add(L"\\Network Interface(*)\\Bytes Sent/sec");
        HardwareSensors sensors;
        uint64_t vramTotal=0;
        bool detailWarm=false,hasVramTotal=false;
        DWORD previousTarget=0;
        uint64_t targetTime=0,targetClock=0,targetCreated=0;
        bool gpuWarm=false;
        uint64_t oldTotal=0,oldIdle=0,oldProcess=0,oldClock=0;
        bool hadSystem=false,hadProcess=false;
        SYSTEM_INFO sys{};GetSystemInfo(&sys);
        while(!stop_){
            if(!enabled){hadSystem=hadProcess=false;gpuWarm=false;nap(200);continue;}
            PerformanceSnapshot out;
            FILETIME idle{},kernel{},user{},created{},exit{};
            if(GetSystemTimes(&idle,&kernel,&user)){
                auto total=time(kernel)+time(user),free=time(idle);
                if(hadSystem&&total>=oldTotal&&free>=oldIdle)out.systemCpu=cpuBusy(total-oldTotal,free-oldIdle);
                oldTotal=total;oldIdle=free;hadSystem=true;
            }else hadSystem=false;
            const uint64_t now=uint64_t(clock100ns());
            if(GetProcessTimes(GetCurrentProcess(),&created,&exit,&kernel,&user)){
                const auto cpu=time(kernel)+time(user);
                if(hadProcess&&now>oldClock&&cpu>=oldProcess)
                    out.processCpu=std::clamp(100.0*double(cpu-oldProcess)/double(now-oldClock)/sys.dwNumberOfProcessors,0.0,100.0);
                oldProcess=cpu;oldClock=now;hadProcess=true;
            }else hadProcess=false;
            MEMORYSTATUSEX memory{};memory.dwLength=sizeof memory;
            if(GlobalMemoryStatusEx(&memory)&&memory.ullTotalPhys>0){out.ramValid=true;out.totalRam=memory.ullTotalPhys;out.usedRam=memory.ullTotalPhys-memory.ullAvailPhys;}
            PROCESS_MEMORY_COUNTERS_EX pm{};pm.cb=sizeof pm;
            if(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pm),sizeof pm)){
                out.processRamValid=true;out.privateRam=pm.PrivateUsage;out.workingRam=pm.WorkingSetSize;
            }
            out.target=targetPid.load();
            if(base.collect()&&gpuWarm){
                GpuUsage usage;
                std::map<std::wstring,double> selected;
                const auto prefix=L"pid_"+std::to_wstring(out.target)+L"_";
                for(const auto& [name,value]:PdhSamples::read(gpu)){
                    usage.add(name,value,GetCurrentProcessId());
                    if(out.target&&name.rfind(prefix,0)==0)selected[name]=value;
                }
                out.systemGpu=GpuUsage::peak(usage.system);out.processGpu=GpuUsage::peak(usage.process);
                out.encodeGpu=GpuUsage::peak(usage.encode);
                if(out.target&&out.systemGpu>=0)out.targetGpu=selected.empty()?0:GpuUsage::peak(selected);
            }
            gpuWarm=true;
            if(out.target){
                HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,out.target);
                if(process){
                    if(GetProcessTimes(process,&created,&exit,&kernel,&user)){
                        auto ticks=time(kernel)+time(user);
                        if(previousTarget==out.target&&time(created)==targetCreated&&now>targetClock&&ticks>=targetTime){
                            auto logical=std::max(DWORD(1),GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
                            out.targetCpu=std::clamp(100.0*double(ticks-targetTime)/double(now-targetClock)/logical,0.0,100.0);
                        }
                        previousTarget=out.target;targetCreated=time(created);targetClock=now;targetTime=ticks;
                    }
                    if(GetProcessMemoryInfo(process,reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pm),sizeof pm)){
                        out.targetRam=pm.WorkingSetSize;out.targetRamValid=true;
                    }
                    if(!GetExitCodeProcess(process,&exit.dwLowDateTime)||exit.dwLowDateTime!=STILL_ACTIVE)out.target=0;
                    CloseHandle(process);
                }else out.target=0;
            }else previousTarget=0;
            if(extended){
                out.sensors=sensors.sample();
                if(!hasVramTotal){vramTotal=dedicatedVideoBytes();hasVramTotal=true;}
                out.vramTotal=vramTotal;
                if(detail.collect()){
                    if(detailWarm){
                        out.cores=PdhSamples::read(cores);
                        for(auto i=out.cores.begin();i!=out.cores.end();){
                            if(i->first.find(L"_Total")!=std::wstring::npos)i=out.cores.erase(i);else ++i;
                        }
                        auto diskTotal=[](PDH_HCOUNTER c){
                            auto values=PdhSamples::read(c);
                            auto total=values.find(L"_Total");
                            return total==values.end()?PdhSamples::sum(values):total->second;
                        };
                        out.diskRead=diskTotal(diskRead);out.diskWrite=diskTotal(diskWrite);
                        out.netIn=PdhSamples::sum(PdhSamples::read(netIn));out.netOut=PdhSamples::sum(PdhSamples::read(netOut));
                    }
                    out.vram=PdhSamples::sum(PdhSamples::read(vram));
                    if(out.sensors.cpuMhz<0)
                        for(const auto& [name,value]:PdhSamples::read(mhz))
                            if(name.find(L"_Total")==std::wstring::npos&&value>0)out.sensors.cpuMhz=std::max(out.sensors.cpuMhz,value);
                    detailWarm=true;
                }
            }else detailWarm=false;
            out.sampledAt=GetTickCount64();
            {std::lock_guard<std::mutex> lock(mutex_);snapshot_=out;}
            nap(intervalMs.load());
        }

    }
public:
    std::atomic<bool> enabled{true};
    std::atomic<int> intervalMs{1000};
    std::atomic<bool> extended{false};
    std::atomic<DWORD> targetPid{0};
    Performance(){worker_=std::thread([this]{try{run();}catch(...){std::lock_guard<std::mutex> lock(mutex_);snapshot_={};}});}
    ~Performance(){stop_=true;wake_.notify_one();if(worker_.joinable())worker_.join();}
    PerformanceSnapshot snapshot(){std::lock_guard<std::mutex> lock(mutex_);return snapshot_;}
    static std::wstring percent(double n){if(n<0)return L"暂不可用";std::wostringstream s;s<<std::fixed<<std::setprecision(1)<<n<<L"%";return s.str();}
    std::wstring read(bool detailed=false){
        const auto s=snapshot();
        if(!s.sampledAt)return L"正在读取性能数据…";
        std::wostringstream out;out<<std::fixed<<std::setprecision(1);
        if(detailed){
            out<<L"全机 CPU："<<percent(s.systemCpu)<<L"     LiteReplay CPU："<<percent(s.processCpu)
               <<L"\r\n\r\n全机 GPU 最忙引擎："<<percent(s.systemGpu)
               <<L"\r\nLiteReplay GPU 最忙引擎："<<percent(s.processGpu)
               <<L"\r\nLiteReplay 视频编码引擎："<<percent(s.encodeGpu)<<L"\r\n\r\n";
            if(s.ramValid)out<<L"全机物理内存："<<s.usedRam/1073741824.0<<L" / "<<s.totalRam/1073741824.0<<L" GiB（"<<percent(100.0*s.usedRam/s.totalRam)<<L"）";
            else out<<L"全机物理内存：暂不可用";
            out<<L"\r\n";
            if(s.processRamValid)out<<L"LiteReplay 私有提交内存："<<s.privateRam/1048576.0<<L" MiB\r\nLiteReplay 驻留工作集："<<s.workingRam/1048576.0<<L" MiB";
            else out<<L"LiteReplay 内存：暂不可用";
            out<<L"\r\n\r\n每秒采样；窗口都隐藏/最小化时暂停采样。\r\nGPU 按最忙引擎统计，不将不同引擎直接相加。\r\n暂不可用表示未取得有效计数，不等于 0%。";
        }else{
            out<<L"全机 CPU "<<percent(s.systemCpu)<<L"  |  GPU "<<percent(s.systemGpu)<<L"  |  内存 ";
            if(s.ramValid)out<<percent(100.0*s.usedRam/s.totalRam);else out<<L"暂不可用";
            out<<L"\n本程序 CPU "<<percent(s.processCpu)<<L"  |  私有内存 ";
            if(s.processRamValid)out<<s.privateRam/1048576.0<<L" MiB";else out<<L"暂不可用";
        }
        return out.str();
    }
};
}
