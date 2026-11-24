#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wrl/client.h>
#include <string>
#include <filesystem>
#include <functional>
#include <atomic>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libswresample/swresample.h>
}
#include "core.hpp"
namespace lr {
using Microsoft::WRL::ComPtr;
inline void check(HRESULT h, const char* what) {
    if (FAILED(h)) {std::ostringstream s; s<<what<<" (0x"<<std::hex<<uint32_t(h)<<")"; throw std::runtime_error(s.str());}
}
inline void avcheck(int n, const char* what) {
    if(n<0) {char msg[AV_ERROR_MAX_STRING_SIZE]; av_strerror(n,msg,sizeof msg); throw std::runtime_error(std::string(what)+": "+msg);}
}
inline int64_t clock100ns() {
    LARGE_INTEGER t,f; QueryPerformanceCounter(&t); QueryPerformanceFrequency(&f);
    return t.QuadPart / f.QuadPart * 10000000 + t.QuadPart % f.QuadPart * 10000000 / f.QuadPart;
}
inline std::string utf8(const std::wstring& s) {
    int n=WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),nullptr,0,nullptr,nullptr);
    std::string out(n,0); if(n) WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),out.data(),n,nullptr,nullptr); return out;
}
inline std::wstring wide(const std::string& s) {
    int n=MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),nullptr,0);
    std::wstring out(n,0); if(n) MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),out.data(),n); return out;
}
struct Config {
    int seconds=60, width=1920, height=1080, fps=60, mbps=20, monitor=0;
    size_t memoryMiB=512;
    std::string codec="h264_nvenc", container="mp4", backend="auto";
    bool allowSoftware=false;
    int systemVolume=100,micVolume=100;
    bool systemAudio=true, microphone=false, separateMic=false, startup=false;
    UINT hotkey=VK_F10, modifiers=MOD_CONTROL|MOD_SHIFT;
    std::wstring folder, prefix=L"LiteReplay",systemDeviceId,micDeviceId;
};
struct FrameDelete {void operator()(AVFrame* p)const{av_frame_free(&p);}};
using Frame=std::unique_ptr<AVFrame,FrameDelete>;
struct CodecDelete {void operator()(AVCodecContext* p)const{avcodec_free_context(&p);}};
using Codec=std::unique_ptr<AVCodecContext,CodecDelete>;
struct ParamsDelete {void operator()(AVCodecParameters* p)const{avcodec_parameters_free(&p);}};
using Params=std::shared_ptr<AVCodecParameters>;
inline Params parameters(AVCodecContext* c) {
    Params p(avcodec_parameters_alloc(),ParamsDelete{}); if(!p) throw std::bad_alloc();
    avcheck(avcodec_parameters_from_context(p.get(),c),"codec parameters"); return p;
}
struct Buffer {
    AVBufferRef* p=nullptr;
    ~Buffer(){av_buffer_unref(&p);}
    Buffer()=default; Buffer(const Buffer&)=delete; Buffer& operator=(const Buffer&)=delete;
};
using Report=std::function<void(const std::string&)>;
inline void pauseStop(const std::atomic<bool>& stop, int ms) {
    for(int i=0;i<ms&&!stop;i+=10) std::this_thread::sleep_for(std::chrono::milliseconds(10));
}
}
