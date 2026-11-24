#pragma once
#include <tlhelp32.h>

namespace lr {
class ProcessTarget {
    struct RestorePoint {DWORD priority=0;DWORD_PTR mask=0;};
    std::map<std::pair<DWORD,uint64_t>,RestorePoint> originals_;
    HANDLE handle_{};
    uint64_t started_{};
public:
    struct Entry {DWORD pid;std::wstring name;};
    struct Scheduling {DWORD priority=0;DWORD_PTR mask=0,allowed=0;};
    ~ProcessTarget(){clear();}
    static std::vector<Entry> list(){
        std::vector<Entry> results;
        HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
        if(snapshot==INVALID_HANDLE_VALUE)return results;
        PROCESSENTRY32W entry{};entry.dwSize=sizeof entry;
        for(BOOL ok=Process32FirstW(snapshot,&entry);ok;ok=Process32NextW(snapshot,&entry)){
            if(entry.th32ProcessID)results.push_back({entry.th32ProcessID,entry.szExeFile});
        }
        CloseHandle(snapshot);
        std::sort(results.begin(),results.end(),[](const auto& a,const auto& b){
            int order=lstrcmpiW(a.name.c_str(),b.name.c_str());
            return order<0||(order==0&&a.pid<b.pid);
        });
        return results;
    }
    void clear(){if(handle_)CloseHandle(handle_);handle_=nullptr;started_=0;}
    DWORD pid() const {
        return handle_&&WaitForSingleObject(handle_,0)==WAIT_TIMEOUT?GetProcessId(handle_):0;
    }
    bool select(DWORD id){
        clear();
        handle_=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,id);
        FILETIME created{},exit{},kernel{},user{};
        if(!handle_||!GetProcessTimes(handle_,&created,&exit,&kernel,&user)){clear();return false;}
        started_=(uint64_t(created.dwHighDateTime)<<32)|created.dwLowDateTime;
        return pid()!=0;
    }
    Scheduling read() const {
        Scheduling result;
        if(!pid())return result;
        result.priority=GetPriorityClass(handle_);
        // The legacy affinity mask represents one processor group only.
        if(GetActiveProcessorGroupCount()==1)GetProcessAffinityMask(handle_,&result.mask,&result.allowed);
        return result;
    }
    bool canRestore() const {return originals_.count({pid(),started_})!=0;}
    DWORD apply(DWORD priority,DWORD_PTR mask,bool restore){
        const auto id=pid();
        if(!id)return ERROR_NOT_FOUND;
        if(id==GetCurrentProcessId())return ERROR_ACCESS_DENIED;
        auto old=read();
        auto key=std::make_pair(id,started_);
        auto original=originals_.find(key);
        if(restore){
            if(original==originals_.end())return ERROR_NOT_FOUND;
            priority=original->second.priority;mask=original->second.mask;
        }
        if(!old.priority||(mask&&(!old.allowed||(mask&~old.allowed))))return ERROR_INVALID_PARAMETER;
        HANDLE change=OpenProcess(PROCESS_SET_INFORMATION|PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,id);
        if(!change)return GetLastError();
        FILETIME created{},exit{},kernel{},user{};
        DWORD error=ERROR_SUCCESS;
        if(!GetProcessTimes(change,&created,&exit,&kernel,&user)||
            ((uint64_t(created.dwHighDateTime)<<32)|created.dwLowDateTime)!=started_||
            WaitForSingleObject(change,0)!=WAIT_TIMEOUT)error=ERROR_NOT_FOUND;
        else if(priority!=old.priority&&!SetPriorityClass(change,priority))error=GetLastError();
        else if(mask&&mask!=old.mask&&!SetProcessAffinityMask(change,mask)){
            error=GetLastError();
            if(priority!=old.priority&&!SetPriorityClass(change,old.priority))
                originals_.try_emplace(key,RestorePoint{old.priority,old.mask});
        }else if(restore)originals_.erase(key);
        else if(priority!=old.priority||(mask&&mask!=old.mask))
            originals_.try_emplace(key,RestorePoint{old.priority,old.mask});
        CloseHandle(change);
        return error;
    }
};
}
