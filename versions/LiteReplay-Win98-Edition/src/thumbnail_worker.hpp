#pragma once
#include "common.hpp"
#include "thumbnail_cache.hpp"
#include <shlobj.h>
#include <shellapi.h>
namespace lr {
// One bounded pixel buffer. Extraction runs in a kill-on-close Job, never the UI.
struct ThumbnailWire {LONG width,height;uint32_t pixels[192*108];};
inline int thumbnailWorker(const wchar_t* mapping,const wchar_t* source){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    HRESULT co=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(FAILED(co))return 2;
    HANDLE m=OpenFileMappingW(FILE_MAP_WRITE,FALSE,mapping);int result=3;
    if(m){auto out=static_cast<ThumbnailWire*>(MapViewOfFile(m,FILE_MAP_WRITE,0,0,sizeof(ThumbnailWire)));
        if(out){out->width=out->height=0;ComPtr<IShellItemImageFactory> f;HBITMAP image=nullptr;
            if(SUCCEEDED(SHCreateItemFromParsingName(source,nullptr,IID_PPV_ARGS(&f)))&&SUCCEEDED(f->GetImage({192,108},SIIGBF_THUMBNAILONLY,&image))){
                BITMAP b{};if(GetObjectW(image,sizeof(b),&b)&&b.bmWidth>0&&b.bmWidth<=192&&b.bmHeight>0&&b.bmHeight<=108){
                    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=b.bmWidth;info.bmiHeader.biHeight=-b.bmHeight;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
                    HDC dc=GetDC(nullptr);if(dc){if(GetDIBits(dc,image,0,b.bmHeight,out->pixels,&info,DIB_RGB_COLORS)==UINT(b.bmHeight)){out->width=b.bmWidth;out->height=b.bmHeight;result=0;}ReleaseDC(nullptr,dc);}
                }
            }
            if(image)DeleteObject(image);UnmapViewOfFile(out);
        }CloseHandle(m);
    }CoUninitialize();return result;
}
class ThumbnailJob {
    HANDLE job_=nullptr,process_=nullptr,mapping_=nullptr;ThumbnailWire* data_=nullptr;ULONGLONG start_=0;
public:
    ~ThumbnailJob(){cancel();}
    bool active()const{return process_!=nullptr;}
    void cancel(){if(job_)CloseHandle(job_);job_=nullptr;if(process_)CloseHandle(process_);process_=nullptr;if(data_)UnmapViewOfFile(data_);data_=nullptr;if(mapping_)CloseHandle(mapping_);mapping_=nullptr;}
    bool start(const std::wstring& source){
        cancel();GUID id{};if(FAILED(CoCreateGuid(&id)))return false;wchar_t guid[40];StringFromGUID2(id,guid,40);std::wstring name=L"Local\\LiteReplayThumb"+std::wstring(guid);
        mapping_=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(ThumbnailWire),name.c_str());if(!mapping_)return false;
        data_=static_cast<ThumbnailWire*>(MapViewOfFile(mapping_,FILE_MAP_ALL_ACCESS,0,0,sizeof(ThumbnailWire)));if(!data_){cancel();return false;}ZeroMemory(data_,sizeof(*data_));
        job_=CreateJobObjectW(nullptr,nullptr);JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if(!job_||!SetInformationJobObject(job_,JobObjectExtendedLimitInformation,&limits,sizeof(limits))){cancel();return false;}
        wchar_t exe[32768];DWORD length=GetModuleFileNameW(nullptr,exe,32768);if(!length||length>=32768||source.find(L'"')!=std::wstring::npos){cancel();return false;}
        std::wstring command=L"\""+std::wstring(exe)+L"\" --thumbnail-worker \""+name+L"\" \""+source+L"\"";
        STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};
        if(!CreateProcessW(exe,command.data(),nullptr,nullptr,FALSE,CREATE_SUSPENDED|CREATE_NO_WINDOW|IDLE_PRIORITY_CLASS,nullptr,nullptr,&si,&pi)){cancel();return false;}
        process_=pi.hProcess;
        if(!AssignProcessToJobObject(job_,process_)||ResumeThread(pi.hThread)==DWORD(-1)){TerminateProcess(process_,4);CloseHandle(pi.hThread);cancel();return false;}
        CloseHandle(pi.hThread);start_=GetTickCount64();return true;
    }
    // false = pending. Empty pixels = failure/timeout; caller may negative-cache it.
    bool poll(ThumbnailPixels& out){
        if(!active())return true;auto wait=WaitForSingleObject(process_,0);
        if(wait==WAIT_TIMEOUT&&GetTickCount64()-start_<10000)return false;
        DWORD code=1;if(wait==WAIT_OBJECT_0&&GetExitCodeProcess(process_,&code)&&code==0&&data_->width>0&&data_->width<=192&&data_->height>0&&data_->height<=108){out.width=data_->width;out.height=data_->height;out.pixels.assign(data_->pixels,data_->pixels+out.width*out.height);}
        cancel();return true;
    }
};
}
