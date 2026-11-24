#pragma once
#include "common.hpp"
namespace lr {
class AudioCapture {
    std::atomic<bool> stop_{false};
    std::thread thread_;
    void run(bool microphone, int64_t epoch, AudioTimeline& target, Report report,std::wstring deviceId);
public:
    AudioCapture(bool microphone,int64_t epoch,AudioTimeline& target,Report report,std::wstring deviceId=L"")
        :thread_([this,microphone,epoch,&target,report,deviceId]{run(microphone,epoch,target,report,deviceId);}){}
    ~AudioCapture(){stop_=true;if(thread_.joinable())thread_.join();}
};
class AudioEncoder {
    Codec codec_;
public:
    AudioEncoder();
    AVCodecContext* encoder()const{return codec_.get();}
    Frame frame(int64_t position,const float* pcm);
    int frameSize()const{return codec_->frame_size;}
};
}
