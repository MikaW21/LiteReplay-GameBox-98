#pragma once
#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace lr {
struct MediaAsset { uint64_t id=0,bytes=0,stamp=0;std::string path,key,date,type; };
class MediaCatalog {
    std::vector<MediaAsset> rows_;
    uint64_t next_=1;
public:
    static constexpr size_t limit=10000;
    const auto& rows()const{return rows_;}
    MediaAsset* find(uint64_t id){for(auto& a:rows_)if(a.id==id)return &a;return nullptr;}
    uint64_t add(MediaAsset a){
        if(a.path.empty()||a.key.empty()||a.path.size()>131072||a.key.size()>131072)throw std::runtime_error("Invalid media path");
        for(auto& row:rows_)if(row.key==a.key){auto id=row.id;a.id=id;row=std::move(a);return id;}
        if(rows_.size()>=limit||next_==std::numeric_limits<uint64_t>::max())throw std::runtime_error("Media library limit reached (10000)");
        a.id=next_++;rows_.push_back(std::move(a));return rows_.back().id;
    }
    void remove(uint64_t id){rows_.erase(std::remove_if(rows_.begin(),rows_.end(),[id](auto& a){return a.id==id;}),rows_.end());}
    std::string serialize()const{
        std::ostringstream out;out<<"LITEMEDIA 1 "<<next_<<' '<<rows_.size()<<'\n';
        for(auto& a:rows_)out<<a.id<<' '<<a.bytes<<' '<<a.stamp<<' '<<std::quoted(a.path)<<' '<<std::quoted(a.key)<<' '<<std::quoted(a.date)<<' '<<std::quoted(a.type)<<'\n';
        return out.str();
    }
    void load(const std::string& text){
        if(text.size()>32*1024*1024)throw std::runtime_error("Library index too large");
        std::istringstream in(text);std::string magic;unsigned version;uint64_t next;size_t count;
        if(!(in>>magic>>version>>next>>count)||magic!="LITEMEDIA"||version!=1||count>limit||!next)throw std::runtime_error("Invalid library index header");
        MediaCatalog candidate;candidate.next_=next;std::set<uint64_t> ids;std::set<std::string> keys;
        for(size_t i=0;i<count;++i){MediaAsset a;
            if(!(in>>a.id>>a.bytes>>a.stamp>>std::quoted(a.path)>>std::quoted(a.key)>>std::quoted(a.date)>>std::quoted(a.type))||!a.id||a.id>=next||a.path.empty()||a.key.empty()||a.path.size()>131072||a.key.size()>131072||a.date.size()>32||a.type.size()>16||!ids.insert(a.id).second||!keys.insert(a.key).second)throw std::runtime_error("Invalid library index row");
            candidate.rows_.push_back(std::move(a));
        }
        in>>std::ws;if(!in.eof())throw std::runtime_error("Trailing library data");
        *this=std::move(candidate);
    }
};
}
