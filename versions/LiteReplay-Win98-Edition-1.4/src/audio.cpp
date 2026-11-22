#include "audio.hpp"
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>
#include <avrt.h>
namespace lr {
namespace {
struct CoMemory {void* p=nullptr;~CoMemory(){CoTaskMemFree(p);}};
struct Resampler {SwrContext* p=nullptr;~Resampler(){swr_free(&p);}};
std::wstring deviceId(IMMDevice* d){LPWSTR p=nullptr;check(d->GetId(&p),"audio device ID");CoMemory m{p};return p;}
}
void AudioCapture::run(bool microphone,int64_t epoch,AudioTimeline& target,Report report,std::wstring selectedDevice) {
    const HRESULT co=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    if(FAILED(co)){report("音频组件初始化失败");return;}
    DWORD task=0;HANDLE mmcss=AvSetMmThreadCharacteristicsW(L"Audio",&task);
    while(!stop_) {
        try {
            ComPtr<IMMDeviceEnumerator> enumerator;
            check(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator)),"audio enumerator");
            ComPtr<IMMDevice> device;
            check(selectedDevice.empty()?enumerator->GetDefaultAudioEndpoint(microphone?eCapture:eRender,eConsole,&device):enumerator->GetDevice(selectedDevice.c_str(),&device),"selected audio endpoint");
            DWORD state=0;check(device->GetState(&state),"audio endpoint state");if(state!=DEVICE_STATE_ACTIVE)throw std::runtime_error("所选音频设备未连接或已停用；不会切换到其他设备");
            auto id=deviceId(device.Get());
            ComPtr<IAudioClient> client;
            check(device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(client.GetAddressOf())),"activate WASAPI");
            WAVEFORMATEX* format=nullptr;check(client->GetMixFormat(&format),"WASAPI mix format");CoMemory memory{format};
            AVSampleFormat sample=AV_SAMPLE_FMT_NONE;
            WORD tag=format->wFormatTag;uint64_t mask=0;
            if(tag==WAVE_FORMAT_EXTENSIBLE && format->cbSize>=22) {
                auto ex=reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format);mask=ex->dwChannelMask;
                if(IsEqualGUID(ex->SubFormat,KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))tag=WAVE_FORMAT_IEEE_FLOAT;
                else if(IsEqualGUID(ex->SubFormat,KSDATAFORMAT_SUBTYPE_PCM))tag=WAVE_FORMAT_PCM;
            }
            if(tag==WAVE_FORMAT_IEEE_FLOAT && format->wBitsPerSample==32)sample=AV_SAMPLE_FMT_FLT;
            if(tag==WAVE_FORMAT_PCM && format->wBitsPerSample==16)sample=AV_SAMPLE_FMT_S16;
            if(tag==WAVE_FORMAT_PCM && format->wBitsPerSample==32)sample=AV_SAMPLE_FMT_S32;
            if(sample==AV_SAMPLE_FMT_NONE)throw std::runtime_error("音频格式不支持，请使用 16/32 位 PCM 或 32 位浮点");
            AVChannelLayout input{},output=AV_CHANNEL_LAYOUT_STEREO;
            if(mask)avcheck(av_channel_layout_from_mask(&input,mask),"audio channel mask");
            else av_channel_layout_default(&input,format->nChannels);
            Resampler resampler;
            int result=swr_alloc_set_opts2(&resampler.p,&output,AV_SAMPLE_FMT_FLT,48000,
                &input,sample,int(format->nSamplesPerSec),0,nullptr);
            av_channel_layout_uninit(&input);avcheck(result,"audio resampler allocation");
            avcheck(swr_init(resampler.p),"audio resampler initialization");
            check(client->Initialize(AUDCLNT_SHAREMODE_SHARED,microphone?0:AUDCLNT_STREAMFLAGS_LOOPBACK,
                1000000,0,format,nullptr),"initialize WASAPI");
            ComPtr<IAudioCaptureClient> capture;
            check(client->GetService(IID_PPV_ARGS(&capture)),"WASAPI capture service");
            check(client->Start(),"start WASAPI");
            struct Stop {IAudioClient* c;~Stop(){c->Stop();}} stop{client.Get()};
            report(microphone?"麦克风已连接":"系统声音已连接");
            auto nextCheck=clock100ns()+10000000;
            while(!stop_) {
                UINT32 count=0;check(capture->GetNextPacketSize(&count),"audio packet size");
                while(count && !stop_) {
                    {
                    BYTE* bytes=nullptr;UINT32 frames=0;DWORD flags=0;UINT64 position=0,qpc=0;
                    check(capture->GetBuffer(&bytes,&frames,&flags,&position,&qpc),"audio GetBuffer");
                    struct Release {IAudioCaptureClient* c;UINT32 n;~Release(){c->ReleaseBuffer(n);}} release{capture.Get(),frames};
                    if(flags&AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) {swr_close(resampler.p);avcheck(swr_init(resampler.p),"resampler reset");}
                    // Invalid timestamps are discarded rather than introducing unknown A/V offset.
                    if(!(flags&AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)) {
                        int64_t delay=swr_get_delay(resampler.p,format->nSamplesPerSec);
                        int maxOut=int(av_rescale_rnd(delay+frames,48000,format->nSamplesPerSec,AV_ROUND_UP));
                        std::vector<float> converted(size_t(maxOut)*2);
                        std::vector<uint8_t> silence;
                        if(flags&AUDCLNT_BUFFERFLAGS_SILENT){silence.resize(size_t(frames)*format->nBlockAlign);bytes=silence.data();}
                        const uint8_t* source[]={bytes};uint8_t* destination[]={reinterpret_cast<uint8_t*>(converted.data())};
                        int samples=swr_convert(resampler.p,destination,maxOut,source,int(frames));avcheck(samples,"resample WASAPI");
                        int64_t start=av_rescale(int64_t(qpc)-epoch,48000,10000000)
                            - av_rescale(delay,48000,format->nSamplesPerSec);
                        target.put(start,converted.data(),size_t(samples));
                    }
                    } // release current buffer before asking for the next one
                    check(capture->GetNextPacketSize(&count),"drain WASAPI packets");
                }
                if(clock100ns()>=nextCheck) {
                    if(!selectedDevice.empty()){DWORD state=0;check(device->GetState(&state),"selected endpoint state");if(state!=DEVICE_STATE_ACTIVE)throw std::runtime_error("所选音频设备已断开，等待原设备重新连接");nextCheck=clock100ns()+10000000;continue;}
                    ComPtr<IMMDevice> current;
                    check(enumerator->GetDefaultAudioEndpoint(microphone?eCapture:eRender,eConsole,&current),"audio device switch");
                    if(deviceId(current.Get())!=id)throw std::runtime_error("默认音频设备已变化，正在重新连接");
                    nextCheck=clock100ns()+10000000;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(count?1:4));
            }
        } catch(const std::exception& e) {
            report(std::string(microphone?"麦克风：":"系统声音：")+e.what()+"；1 秒后重试，期间补静音");
            pauseStop(stop_,1000);
        }
    }
    if(mmcss)AvRevertMmThreadCharacteristics(mmcss);
    CoUninitialize();
}
AudioEncoder::AudioEncoder() {
    auto codec=avcodec_find_encoder(AV_CODEC_ID_AAC);if(!codec)throw std::runtime_error("缺少 AAC 音频编码器");
    codec_.reset(avcodec_alloc_context3(codec));if(!codec_)throw std::bad_alloc();
    auto e=codec_.get();e->sample_rate=48000;e->sample_fmt=AV_SAMPLE_FMT_FLTP;
    AVChannelLayout stereo=AV_CHANNEL_LAYOUT_STEREO;avcheck(av_channel_layout_copy(&e->ch_layout,&stereo),"stereo layout");
    e->time_base={1,48000};e->bit_rate=192000;e->flags|=AV_CODEC_FLAG_GLOBAL_HEADER;
    avcheck(avcodec_open2(e,codec,nullptr),"AAC encoder");
}
Frame AudioEncoder::frame(int64_t position,const float* pcm) {
    Frame f(av_frame_alloc());if(!f)throw std::bad_alloc();
    f->format=codec_->sample_fmt;f->sample_rate=48000;f->nb_samples=codec_->frame_size;f->pts=position;
    avcheck(av_channel_layout_copy(&f->ch_layout,&codec_->ch_layout),"audio frame layout");
    avcheck(av_frame_get_buffer(f.get(),0),"audio frame buffer");
    for(int i=0;i<f->nb_samples;++i) {
        reinterpret_cast<float*>(f->data[0])[i]=pcm[2*i];
        reinterpret_cast<float*>(f->data[1])[i]=pcm[2*i+1];
    }
    return f;
}
}
