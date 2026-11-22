#pragma once
#include <cstdint>
#include <list>
#include <string>
#include <vector>
namespace lr {
struct ThumbnailPixels {int width=0,height=0;std::vector<uint32_t> pixels;};
class ThumbnailCache {
    struct Entry {std::wstring key;ThumbnailPixels image;};
    std::list<Entry> entries_;size_t limit_=8*1024*1024,bytes_=0;
    void trim(){while(!entries_.empty()&&(bytes_>limit_||entries_.size()>128)){bytes_-=entries_.back().image.pixels.size()*4;entries_.pop_back();}}
public:
    void limit(size_t bytes){limit_=bytes;trim();}
    void clear(){entries_.clear();bytes_=0;}
    size_t bytes()const{return bytes_;}
    size_t size()const{return entries_.size();}
    void erase(const std::wstring& key){for(auto i=entries_.begin();i!=entries_.end();++i)if(i->key==key){bytes_-=i->image.pixels.size()*4;entries_.erase(i);return;}}
    const ThumbnailPixels* get(const std::wstring& key){for(auto i=entries_.begin();i!=entries_.end();++i)if(i->key==key){entries_.splice(entries_.begin(),entries_,i);return &entries_.front().image;}return nullptr;}
    void put(std::wstring key,ThumbnailPixels image){erase(key);bytes_+=image.pixels.size()*4;entries_.push_front({std::move(key),std::move(image)});trim();}
};
}
