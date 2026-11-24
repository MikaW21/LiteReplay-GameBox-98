#pragma once
#include <evntrace.h>
#include <deque>
#include <cstdlib>
#include <cctype>

namespace lr {
struct FrameStats {double fps=-1,ms=-1,low=-1;std::vector<double> times;};
class FrameSampler {
    struct Frame {ULONGLONG arrival;double ms;};
    std::map<std::string,std::deque<Frame>> chains_;
    HANDLE pipe_{},process_{};
    std::wstring session_;
    std::string pending_;
    int chainColumn_=-1,msColumn_=-1;
    bool header_=false,accessDenied_=false;
    static std::vector<std::string> split(const std::string& line){
        std::vector<std::string> fields(1);
        bool quoted=false;
        for(char c:line){
            if(c=='"')quoted=!quoted;
            else if(c==','&&!quoted)fields.emplace_back();
            else if(c!='\r')fields.back()+=c;
        }
        return fields;
    }
    void row(const std::string& line,ULONGLONG now){
        if(line.find("access denied")!=std::string::npos||line.find("Performance Log Users")!=std::string::npos)
            accessDenied_=true;
        auto fields=split(line);
        if(!header_){
            for(int i=0;i<int(fields.size());++i){
                auto name=fields[i];
                for(char& c:name)c=char(std::tolower(static_cast<unsigned char>(c)));
                if(name=="msbetweenpresents")msColumn_=i;
                if(name=="swapchainaddress")chainColumn_=i;
            }
            header_=msColumn_>=0&&chainColumn_>=0;
            return;
        }
        if(int(fields.size())<=std::max(chainColumn_,msColumn_))return;
        const auto& value=fields[msColumn_];
        char* end=nullptr;
        double ms=std::strtod(value.c_str(),&end);
        if(end!=value.c_str()+value.size()||!std::isfinite(ms)||ms<=0||ms>1000)return;
        auto& frames=chains_[fields[chainColumn_]];
        frames.push_back({now,ms});
        if(frames.size()>8192)frames.pop_front();
    }
public:
    ~FrameSampler(){stop();}
    void stop(){
        if(process_){
            if(WaitForSingleObject(process_,0)==WAIT_TIMEOUT){
                TerminateProcess(process_,0);
                WaitForSingleObject(process_,300);
                struct TraceProperties {EVENT_TRACE_PROPERTIES properties{};wchar_t name[128]{};} trace;
                trace.properties.Wnode.BufferSize=sizeof trace;
                trace.properties.LoggerNameOffset=sizeof(EVENT_TRACE_PROPERTIES);
                ControlTraceW(0,session_.c_str(),&trace.properties,EVENT_TRACE_CONTROL_STOP);
            }
            CloseHandle(process_);process_=nullptr;
        }
        if(pipe_){CloseHandle(pipe_);pipe_=nullptr;}
        header_=false;accessDenied_=false;msColumn_=chainColumn_=-1;pending_.clear();chains_.clear();
    }
    bool start(const std::filesystem::path& exe,DWORD pid){
        stop();
        if(!pid||!std::filesystem::is_regular_file(exe))return false;
        SECURITY_ATTRIBUTES security{sizeof security,nullptr,TRUE};
        HANDLE writer=nullptr;
        if(!CreatePipe(&pipe_,&writer,&security,65536))return false;
        SetHandleInformation(pipe_,HANDLE_FLAG_INHERIT,0);
        HANDLE nil=CreateFileW(L"NUL",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr);
        if(nil==INVALID_HANDLE_VALUE){CloseHandle(writer);stop();return false;}
        session_=L"LiteReplay-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
        std::wstring command=L"\""+exe.wstring()+L"\" --process_id "+std::to_wstring(pid)+
            L" --output_stdout --v1_metrics --no_track_gpu --no_track_input --no_track_display --terminate_on_proc_exit --session_name "+session_;
        STARTUPINFOW startup{};startup.cb=sizeof startup;startup.dwFlags=STARTF_USESTDHANDLES;
        startup.hStdOutput=writer;startup.hStdError=writer;startup.hStdInput=nil;
        PROCESS_INFORMATION result{};
        BOOL ok=CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&result);
        CloseHandle(writer);CloseHandle(nil);
        if(!ok){stop();return false;}
        CloseHandle(result.hThread);process_=result.hProcess;
        return true;
    }
    bool running() const {return process_&&WaitForSingleObject(process_,0)==WAIT_TIMEOUT;}
    bool accessDenied() const{return accessDenied_;}
    FrameStats sample(){
        if(!pipe_)return {};
        const auto now=GetTickCount64();
        for(int i=0;i<16;++i){
            DWORD available=0,read=0;
            char bytes[8192];
            if(!PeekNamedPipe(pipe_,nullptr,0,nullptr,&available,nullptr)||!available||
                !ReadFile(pipe_,bytes,std::min(available,DWORD(sizeof bytes)),&read,nullptr)||!read)break;
            pending_.append(bytes,read);
            size_t start=0,end=0;
            while((end=pending_.find('\n',start))!=std::string::npos){row(pending_.substr(start,end-start),now);start=end+1;}
            pending_.erase(0,start);
            if(pending_.size()>65536)pending_.clear();
        }
        std::deque<Frame>* primary=nullptr;
        for(auto i=chains_.begin();i!=chains_.end();){
            auto& frames=i->second;
            while(!frames.empty()&&now-frames.front().arrival>5000)frames.pop_front();
            if(frames.empty()){i=chains_.erase(i);continue;}
            if(now-frames.back().arrival<1800&&(!primary||frames.size()>primary->size()))primary=&frames;
            ++i;
        }
        if(!primary||primary->size()<2)return {};
        std::vector<double> times;
        double total=0;
        for(auto frame:*primary){times.push_back(frame.ms);total+=frame.ms;}
        auto chronological=times; if(chronological.size()>240)chronological.erase(chronological.begin(),chronological.end()-240); const size_t count=(times.size()+99)/100;
        std::partial_sort(times.begin(),times.begin()+count,times.end(),std::greater<double>());
        auto tail=std::accumulate(times.begin(),times.begin()+count,0.0);
        return {1000.0*times.size()/total,total/times.size(),times.size()>=100?1000.0*count/tail:-1,std::move(chronological)};
    }
};
}
