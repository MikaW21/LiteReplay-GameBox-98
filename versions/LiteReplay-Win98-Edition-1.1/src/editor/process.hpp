#pragma once
#include "core.hpp"
#include <windows.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include "progress.hpp"
namespace lc {
inline std::atomic<DWORD> processPriority{IDLE_PRIORITY_CLASS};
inline std::wstring wide(const std::string& s){if(s.empty())return {};int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0);if(!n)throw std::runtime_error("Invalid UTF-8");std::wstring r(n,L'\0');MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),r.data(),n);return r;}
inline std::string utf8(const std::wstring& s){if(s.empty())return {};int n=WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),nullptr,0,nullptr,nullptr);std::string r(n,'\0');WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),r.data(),n,nullptr,nullptr);return r;}
struct Handle {HANDLE h=nullptr;Handle()=default;explicit Handle(HANDLE v):h(v){}~Handle(){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);}Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;};
struct Result {DWORD code=1;std::string output;};
inline Result run(const std::filesystem::path& exe,const std::vector<std::string>& args,std::atomic<bool>& cancel,const std::filesystem::path& cwd={},DWORD timeoutMs=0,std::function<void(double)> onProgress={}){
    if(!std::filesystem::exists(exe))throw std::runtime_error("Missing tool: "+utf8(exe.wstring()));
    std::wstring cmd=quoteWindows(exe.wstring());for(auto& a:args)cmd+=L" "+quoteWindows(wide(a));
    SECURITY_ATTRIBUTES sa{sizeof sa,nullptr,TRUE};Handle rd,wr;if(!CreatePipe(&rd.h,&wr.h,&sa,0))throw std::runtime_error("CreatePipe failed");SetHandleInformation(rd.h,HANDLE_FLAG_INHERIT,0);
    Handle nul(CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr));
    Handle job(CreateJobObjectW(nullptr,nullptr));JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!job.h||!SetInformationJobObject(job.h,JobObjectExtendedLimitInformation,&limits,sizeof limits))throw std::runtime_error("Job setup failed");
    STARTUPINFOW si{sizeof si};si.dwFlags=STARTF_USESTDHANDLES;si.hStdOutput=wr.h;si.hStdError=wr.h;si.hStdInput=nul.h;PROCESS_INFORMATION pi{};
    if(!CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED|processPriority.load(),nullptr,cwd.empty()?nullptr:cwd.c_str(),&si,&pi))throw std::runtime_error("Cannot start FFmpeg/FFprobe");
    Handle process(pi.hProcess),thread(pi.hThread);if(!AssignProcessToJobObject(job.h,process.h)){TerminateProcess(process.h,1);WaitForSingleObject(process.h,5000);throw std::runtime_error("Cannot supervise child process");}
    if(ResumeThread(thread.h)==DWORD(-1)){TerminateJobObject(job.h,1);throw std::runtime_error("Cannot resume child process");}CloseHandle(wr.h);wr.h=nullptr;ProgressLines progressLines;Result result;bool ended=false;ULONGLONG started=GetTickCount64();
    while(true){if(timeoutMs&&GetTickCount64()-started>timeoutMs){TerminateJobObject(job.h,ERROR_TIMEOUT);WaitForSingleObject(process.h,5000);throw std::runtime_error("Media probe timed out");}if(cancel){TerminateJobObject(job.h,ERROR_CANCELLED);WaitForSingleObject(process.h,5000);result.code=ERROR_CANCELLED;return result;}DWORD available=0;if(PeekNamedPipe(rd.h,nullptr,0,nullptr,&available,nullptr)&&available){char buffer[4096];DWORD got=0;if(ReadFile(rd.h,buffer,std::min<DWORD>(available,sizeof buffer),&got,nullptr)&&got){if(onProgress)progressLines.feed(std::string(buffer,got),onProgress);result.output.append(buffer,got);if(result.output.size()>65536)result.output.erase(0,result.output.size()-65536);continue;}}
        if(ended)break;if(cancel){TerminateJobObject(job.h,ERROR_CANCELLED);WaitForSingleObject(process.h,5000);result.code=ERROR_CANCELLED;return result;}ended=WaitForSingleObject(process.h,30)==WAIT_OBJECT_0;
    }
    GetExitCodeProcess(process.h,&result.code);return result;
}
inline std::string readFile(const std::filesystem::path& path){if(std::filesystem::file_size(path)>10*1024*1024)throw std::runtime_error("File too large");std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Cannot open file");return {std::istreambuf_iterator<char>(f),{}};}
inline void writeFile(const std::filesystem::path& path,const std::string& data){std::ofstream f(path,std::ios::binary|std::ios::trunc);f.write(data.data(),std::streamsize(data.size()));f.close();if(!f)throw std::runtime_error("Cannot write file");}
}
