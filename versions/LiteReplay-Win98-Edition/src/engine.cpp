#include "engine.hpp"
#include "video.hpp"
#include "audio.hpp"
#include <ctime>
namespace lr {
namespace {
class FrameTimer {
    HANDLE handle_=nullptr;
public:
    FrameTimer(){
        // Win10 1803+: a high-resolution per-thread wait, no timeBeginPeriod.
        handle_=CreateWaitableTimerExW(nullptr,nullptr,0x00000002,TIMER_ALL_ACCESS);
        if(!handle_)handle_=CreateWaitableTimerW(nullptr,FALSE,nullptr);
    }
    ~FrameTimer(){if(handle_)CloseHandle(handle_);}
    void until(int64_t deadline){
        if(!handle_){Sleep(1);return;}
        LARGE_INTEGER due;due.QuadPart=-std::max<int64_t>(1000,deadline-clock100ns());
        if(!SetWaitableTimer(handle_,&due,0,nullptr,nullptr,FALSE)){Sleep(1);return;}
        WaitForSingleObject(handle_,100);
    }
};
void encode(AVCodecContext* encoder,AVFrame* frame,int stream,PacketRing& ring) {
    avcheck(avcodec_send_frame(encoder,frame),"send encoder frame");
    AVPacket* raw=av_packet_alloc();if(!raw)throw std::bad_alloc();
    struct Release {AVPacket* p;~Release(){av_packet_free(&p);}} release{raw};
    while(true) {
        int rc=avcodec_receive_packet(encoder,raw);
        if(rc==AVERROR(EAGAIN)||rc==AVERROR_EOF)break;
        avcheck(rc,"receive encoded packet");
        const auto us=AVRational{1,1000000};
        Packet p;p.stream=stream;p.pts=av_rescale_q(raw->pts,encoder->time_base,us);
        p.dts=av_rescale_q(raw->dts,encoder->time_base,us);
        p.duration=raw->duration?av_rescale_q(raw->duration,encoder->time_base,us):
            (stream==0?1000000LL*encoder->time_base.num/encoder->time_base.den:1024LL*1000000/48000);
        p.key=(raw->flags&AV_PKT_FLAG_KEY)!=0;
        p.data=std::make_shared<const std::vector<uint8_t>>(raw->data,raw->data+raw->size);
        ring.push(std::move(p));av_packet_unref(raw);
    }
}
std::wstring saveFile(const std::shared_ptr<Session>& s,std::vector<Packet> packets) {
    if(packets.empty())throw std::runtime_error("缓存尚未建立，请稍后保存");
    // Every video session has no B-frames. Preserve the common A/V offset.
    int64_t start=-1;
    for(auto& p:packets)if(p.stream==0&&p.key){start=p.pts;break;}
    if(start<0)throw std::runtime_error("当前缓存缺少视频关键帧");
    // Export only the common completed interval. AAC is deliberately collected
    // behind video; do not emit a ~100ms silent tail just because Save was pressed.
    std::vector<int64_t> ends(s->codecs.size(),-1);
    for(const auto& p:packets)ends.at(size_t(p.stream))=std::max(ends.at(size_t(p.stream)),p.pts+p.duration);
    const int64_t end=*std::min_element(ends.begin(),ends.end());
    if(end<=start)throw std::runtime_error("音视频缓存正在建立，请稍后保存");
    packets.erase(std::remove_if(packets.begin(),packets.end(),[end](const Packet& p){return p.pts>=end;}),packets.end());
    std::filesystem::create_directories(s->config.folder);
    SYSTEMTIME t;GetLocalTime(&t);
    wchar_t stamp[80];swprintf_s(stamp,L"_%04u%02u%02u_%02u%02u%02u_%03u_%llu",t.wYear,t.wMonth,t.wDay,
        t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,static_cast<unsigned long long>(GetTickCount64()));
    auto path=std::filesystem::path(s->config.folder)/(s->config.prefix+stamp+L"."+wide(s->config.container));
    auto partial=path;partial+=L".partial";
    AVFormatContext* f=nullptr;
    avcheck(avformat_alloc_output_context2(&f,nullptr,s->config.container=="mkv"?"matroska":"mp4",nullptr),"output muxer");
    if(!f)throw std::bad_alloc();
    struct Cleanup {AVFormatContext* f;~Cleanup(){if(f->pb)avio_closep(&f->pb);avformat_free_context(f);}} cleanup{f};
    bool ownPartial=false;
    struct DeletePartial {std::filesystem::path p;bool& owned;AVFormatContext* f;~DeletePartial(){if(owned){
        if(f->pb)avio_closep(&f->pb);
        std::error_code ec;std::filesystem::remove(p,ec);
    }}} remove{partial,ownPartial,f};
    for(auto& codec:s->codecs) {
        auto stream=avformat_new_stream(f,nullptr);if(!stream)throw std::bad_alloc();
        avcheck(avcodec_parameters_copy(stream->codecpar,codec.get()),"muxer stream parameters");
        stream->time_base={1,1000000};stream->codecpar->codec_tag=0;
        if(codec->codec_id==AV_CODEC_ID_HEVC && s->config.container=="mp4")stream->codecpar->codec_tag=MKTAG('h','v','c','1');
    }
    f->streams[0]->avg_frame_rate={s->config.fps,1};
    std::string recordingInfo="LiteReplayRecordingV1;width="+std::to_string(f->streams[0]->codecpar->width)+";height="+std::to_string(f->streams[0]->codecpar->height)+";fps="+std::to_string(s->config.fps)+";mbps="+std::to_string(s->config.mbps);
    av_dict_set(&f->metadata,"comment",recordingInfo.c_str(),0);
    if(f->nb_streams>1)av_dict_set(&f->streams[1]->metadata,"title",s->config.separateMic?"System audio":"System + microphone",0);
    if(f->nb_streams>2)av_dict_set(&f->streams[2]->metadata,"title","Microphone",0);
    // Open only a new path; never overwrite an existing replay.
    HANDLE reservation=CreateFileW(partial.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(reservation==INVALID_HANDLE_VALUE)throw std::runtime_error("无法创建回放文件，请检查路径、权限或文件名冲突");
    ownPartial=true;CloseHandle(reservation);
    avcheck(avio_open(&f->pb,utf8(partial.wstring()).c_str(),AVIO_FLAG_WRITE),"open output file");
    avcheck(avformat_write_header(f,nullptr),"write container header");
    AVPacket* raw=av_packet_alloc();if(!raw)throw std::bad_alloc();
    struct PacketCleanup {AVPacket* p;~PacketCleanup(){av_packet_free(&p);}} packetCleanup{raw};
    for(const auto& p:packets) {
        if(p.pts<start||p.dts<start)continue;
        avcheck(av_new_packet(raw,int(p.data->size())),"mux packet allocation");
        memcpy(raw->data,p.data->data(),p.data->size());raw->stream_index=p.stream;
        raw->pts=p.pts-start;raw->dts=p.dts-start;raw->duration=p.duration;
        if(p.key)raw->flags|=AV_PKT_FLAG_KEY;
        av_packet_rescale_ts(raw,AVRational{1,1000000},f->streams[p.stream]->time_base);
        avcheck(av_interleaved_write_frame(f,raw),"write replay packet (check free disk space)");
        av_packet_unref(raw);
    }
    avcheck(av_write_trailer(f),"finish container");avcheck(avio_closep(&f->pb),"close container");
    // MoveFileEx without REPLACE_EXISTING is an atomic same-directory commit.
    if(!MoveFileExW(partial.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH))throw std::runtime_error("无法完成回放文件重命名");
    ownPartial=false;return path.wstring();
}
}
Engine::~Engine(){stop_=true;if(capture_.joinable())capture_.join();{
    std::lock_guard<std::mutex> l(mutex_);quitWriter_=true;}wake_.notify_one();if(writer_.joinable())writer_.join();}
void Engine::status(const std::string& s){{std::lock_guard<std::mutex> l(mutex_);status_=s;}log_(s);}
void Engine::start(Config c) {
    if(running_)return;
    if(capture_.joinable())capture_.join();
    audioVolumes(c.systemVolume,c.micVolume);stop_=false;running_=true;frames=0;skipped=0;restarts=0;
    capture_=std::thread([this,c]{capture(c);});
}
std::string Engine::statusText()const {
    std::lock_guard<std::mutex> l(mutex_);std::ostringstream s;s<<status_;
    if(session_)s<<" | 缓存 "<<session_->ring.duration()/1000000<<" 秒，"<<session_->ring.bytes()/1048576<<" MiB";
    if(saving_)s<<" | 正在保存";
    if(!audioStatus_.empty())s<<"\n"<<audioStatus_;
    return s.str();
}
bool Engine::save() {
    std::lock_guard<std::mutex> l(mutex_);
    if(saving_||!session_)return false;
    saving_=true;saveSession_=session_;saveRequested_=true;wake_.notify_one();return true;
}
void Engine::writer() {
    while(true) {
        std::shared_ptr<Session> s;
        {std::unique_lock<std::mutex> l(mutex_);wake_.wait(l,[this]{return quitWriter_||saveRequested_;});
         if(quitWriter_&&!saveRequested_)break;s=std::move(saveSession_);saveRequested_=false;}
        try {auto path=saveFile(s,s->ring.snapshot(s->config.seconds));
            {std::lock_guard<std::mutex> l(mutex_);lastPath_=path;}log_("已保存："+utf8(path));if(saved_)saved_(path);
        }catch(const std::exception& e){status(std::string("保存失败：")+e.what());}
        saving_=false;
    }
}
void Engine::capture(Config c) {
    HRESULT co=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    int failures=0;
    while(!stop_) {
        try {
            status("正在初始化屏幕和编码器 / Initializing capture and encoder");
            Video video(c);log_(video.diagnostic());
            auto s=std::make_shared<Session>(c);s->codecs.push_back(parameters(video.encoder()));
            bool sound=c.systemAudio||c.microphone;
            std::unique_ptr<AudioEncoder> a,b;
            if(sound){a=std::make_unique<AudioEncoder>();s->codecs.push_back(parameters(a->encoder()));}
            if(c.microphone&&c.separateMic){b=std::make_unique<AudioEncoder>();s->codecs.push_back(parameters(b->encoder()));}
            AudioTimeline system,microphone;
            int64_t epoch=clock100ns();
            auto report=[this](const std::string& text){{std::lock_guard<std::mutex> l(mutex_);audioStatus_=text;}log_(text);};
            std::unique_ptr<AudioCapture> sys,mic;
            if(c.systemAudio)sys=std::make_unique<AudioCapture>(false,epoch,system,report,c.systemDeviceId);
            if(c.microphone)mic=std::make_unique<AudioCapture>(true,epoch,microphone,report,c.micDeviceId);
            bool published=false;
            status("正在录制 / Recording (SDR) — "+video.diagnostic());failures=0;
            FrameTimer timer;
            int64_t lastVideo=-1,audioPosition=0;
            const int n=sound?a->frameSize():1024;
            std::vector<float> x(size_t(n)*2),y(size_t(n)*2),mixing(size_t(n)*2);
            while(!stop_) {
                const auto now=clock100ns()-epoch;
                const int64_t tick=av_rescale_rnd(now,c.fps,10000000,AV_ROUND_DOWN);
                if(tick!=lastVideo && video.capture()) {
                    if(lastVideo>=0&&tick>lastVideo+1)skipped+=uint64_t(tick-lastVideo-1);
                    auto frame=video.frame(tick);encode(video.encoder(),frame.get(),0,s->ring);
                    lastVideo=tick;++frames;
                    // Some Media Foundation transforms expose codec headers only after the first input.
                    // Publish immutable codec parameters only once headers exist; writers never race updates.
                    if(!published&&video.encoder()->extradata_size>0){
                        s->codecs[0]=parameters(video.encoder());{std::lock_guard<std::mutex> l(mutex_);session_=s;}published=true;
                    }
                    if(!published&&tick>c.fps*5)throw std::runtime_error("Encoder did not return stream headers; choose another encoder / 编码器未返回视频参数，请更换编码后端");
                }
                // 100 ms lets WASAPI deliver packets before we fill missing spans with silence.
                const auto available=av_rescale(now-1000000,48000,10000000);
                // On a long stall, drop stale PCM and jump to the current timestamp.
                if(sound&&available-audioPosition>96000)audioPosition=(available-n)/n*n;
                while(sound && audioPosition+n<=available && !stop_) {
                    system.get(audioPosition,x.data(),n);microphone.get(audioPosition,y.data(),n);
                    const int sg=systemVolume_.load(),mg=micVolume_.load();
                    for(int i=0;i<2*n;++i){x[i]=recordingGain(x[i],sg);y[i]=recordingGain(y[i],mg);mixing[i]=c.separateMic?x[i]:mix(x[i],y[i]);}
                    auto f=a->frame(audioPosition,mixing.data());encode(a->encoder(),f.get(),1,s->ring);
                    if(b){auto m=b->frame(audioPosition,y.data());encode(b->encoder(),m.get(),2,s->ring);}
                    audioPosition+=n;
                }
                timer.until(epoch+av_rescale(tick+1,10000000,c.fps));
            }
        }catch(const std::exception& e){
            {std::lock_guard<std::mutex> l(mutex_);session_.reset();}
            ++restarts;++failures;status(std::string("采集已中断：")+e.what()+"；正在重试");
            pauseStop(stop_,std::min(10000,1000*failures));
        }
    }
    {std::lock_guard<std::mutex> l(mutex_);session_.reset();audioStatus_.clear();}
    status("已停止");running_=false;if(SUCCEEDED(co))CoUninitialize();
}
}
