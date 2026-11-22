#pragma once
#include "common.hpp"
#include <condition_variable>
namespace lr {
struct Session {
    Config config;
    PacketRing ring;
    std::vector<Params> codecs;
    explicit Session(const Config& c):config(c),ring(c.seconds,c.memoryMiB*1024*1024){}
};
class Engine {
    std::atomic<int> systemVolume_{100},micVolume_{100};
    std::atomic<bool> stop_{true}, running_{false}, saving_{false};
    std::thread capture_,writer_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    bool quitWriter_=false, saveRequested_=false;
    std::shared_ptr<Session> session_,saveSession_;
    std::wstring lastPath_;
    std::string status_="已停止",audioStatus_;
    Report log_;
    std::function<void(const std::wstring&)> saved_;
    void capture(Config c);
    void writer();
    void status(const std::string& s);
public:
    explicit Engine(Report log,std::function<void(const std::wstring&)> saved={}):log_(std::move(log)),saved_(std::move(saved)){writer_=std::thread([this]{writer();});}
    ~Engine();
    void start(Config c);
    void audioVolumes(int system,int mic){systemVolume_=std::clamp(system,0,200);micVolume_=std::clamp(mic,0,200);}
    void stop(){stop_=true;}
    bool save();
    bool running()const{return running_;}
    bool saving()const{return saving_;}
    std::string statusText()const;
    std::wstring lastPath()const{std::lock_guard<std::mutex> l(mutex_);return lastPath_;}
    std::atomic<uint64_t> frames{0},skipped{0},restarts{0};
};
}
