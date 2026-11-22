#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace lr {
struct Packet {
    std::shared_ptr<const std::vector<uint8_t>> data;
    int stream = 0;
    int64_t pts = 0, dts = 0, duration = 0; // microseconds, same epoch
    bool key = false;
};
// Single producer, snapshots from any thread. Encoded payloads are immutable.
// The byte limit is for payloads; a single in-flight snapshot may retain another
// byte-limit's worth while the ring wraps. Metadata is bounded independently.
class PacketRing {
    struct Gop {int64_t start; size_t bytes=0; std::vector<Packet> packets;};
    mutable std::mutex mutex_;
    std::deque<Gop> groups_;
    size_t bytes_=0, count_=0, limit_, countLimit_;
    int64_t span_, end_=0;
    void evict() {
        bytes_-=groups_.front().bytes;count_-=groups_.front().packets.size();groups_.pop_front();
    }
public:
    PacketRing(int seconds, size_t bytes, size_t countLimit=200000)
        :limit_(bytes),countLimit_(countLimit),span_(int64_t(seconds)*1000000) {
        if(seconds<=0||bytes==0||countLimit==0)throw std::invalid_argument("ring limits");
    }
    void push(Packet p) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!p.data||p.pts<0)return;
        if(p.data->size()>limit_) {
            // Losing a video packet invalidates its dependent GOP.
            if(p.stream==0){groups_.clear();bytes_=count_=0;}
            return;
        }
        if(p.stream==0&&p.key) {
            if(!groups_.empty()&&p.pts<=groups_.back().start)return;
            groups_.push_back(Gop{p.pts,0,{}});
        }
        if(groups_.empty()||p.pts<groups_.front().start)return;
        if(p.stream==0)end_=std::max(end_,p.pts+p.duration);
        // Audio encoder latency means audio can belong to the preceding GOP.
        auto it=groups_.rbegin();
        while(it!=groups_.rend()&&p.pts<it->start)++it;
        if(it==groups_.rend())return;
        it->bytes+=p.data->size();bytes_+=p.data->size();++count_;
        it->packets.push_back(std::move(p));
        while(groups_.size()>1&&groups_[1].start<=end_-span_)evict();
        while(!groups_.empty()&&(bytes_>limit_||count_>countLimit_))evict();
    }
    std::vector<Packet> snapshot(int seconds) const {
        std::vector<Packet> out;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if(groups_.empty())return out;
            size_t first=0;
            while(first+1<groups_.size()&&groups_[first+1].start<=end_-int64_t(seconds)*1000000)++first;
            out.reserve(count_);
            for(size_t i=first;i<groups_.size();++i)
                for(const auto& p:groups_[i].packets)if(p.pts<end_)out.push_back(p);
        }
        // Sorting is export work, and must not hold the capture-side mutex.
        std::stable_sort(out.begin(),out.end(),[](const Packet& a,const Packet& b){
            return a.dts<b.dts||(a.dts==b.dts&&a.stream<b.stream);
        });
        return out;
    }
    size_t bytes()const{std::lock_guard<std::mutex> lock(mutex_);return bytes_;}
    int64_t duration()const{
        std::lock_guard<std::mutex> lock(mutex_);return groups_.empty()?0:end_-groups_.front().start;
    }
};
// Timestamp-addressed stereo PCM, bounded to two seconds. Silence fills gaps.
// Old/late data cannot be mistaken for current samples after circular wrap.
class AudioTimeline {
    struct Sample { int64_t tag = -1; float l = 0, r = 0; };
    std::vector<Sample> samples_;
    std::mutex mutex_;
public:
    explicit AudioTimeline(size_t capacity = 96000) : samples_(capacity) {
        if (!capacity) throw std::invalid_argument("audio capacity");
    }
    void put(int64_t pos, const float* interleaved, size_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (size_t i=0; i<count; ++i) {
            auto t = pos + int64_t(i); if (t < 0) continue;
            auto& s = samples_[size_t(t) % samples_.size()];
            if (s.tag > t) continue;
            s = {t, interleaved[2*i], interleaved[2*i+1]};
        }
    }
    void get(int64_t pos, float* interleaved, size_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (size_t i=0; i<count; ++i) {
            auto t = pos + int64_t(i);
            auto& s = samples_[size_t(std::max<int64_t>(0,t)) % samples_.size()];
            const bool valid = t >= 0 && s.tag == t;
            interleaved[2*i] = valid ? s.l : 0;
            interleaved[2*i+1] = valid ? s.r : 0;
        }
    }
};
inline float recordingGain(float sample,int percent){if(!std::isfinite(sample))sample=0;return std::clamp(sample*(std::clamp(percent,0,200)/100.0f),-1.0f,1.0f);}
inline float mix(float a, float b) { return std::clamp(a+b, -1.0f, 1.0f); }
}
