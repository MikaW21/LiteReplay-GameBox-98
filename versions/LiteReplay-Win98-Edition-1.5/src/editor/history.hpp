#pragma once
#include "core.hpp"
#include <deque>
namespace lc {
class History {
 std::deque<std::string> past_,future_;size_t bytes_=0;static constexpr size_t limit=16*1024*1024;
 void trim(){while(bytes_>limit||past_.size()>64){if(past_.empty())break;bytes_-=past_.front().size();past_.pop_front();}}
public:
 void reset(const Project& p){past_.clear();future_.clear();auto s=serialize(p);bytes_=s.size();past_.push_back(std::move(s));}
 void record(const Project& p){auto s=serialize(p);if(!past_.empty()&&s==past_.back())return;for(auto& f:future_)bytes_-=f.size();future_.clear();bytes_+=s.size();past_.push_back(std::move(s));trim();}
 bool undo(Project& p){if(past_.size()<2)return false;future_.push_back(std::move(past_.back()));past_.pop_back();p=deserialize(past_.back());return true;}
 bool redo(Project& p){if(future_.empty())return false;past_.push_back(std::move(future_.back()));future_.pop_back();p=deserialize(past_.back());return true;}
 size_t bytes()const{return bytes_;}
};
inline std::vector<std::string> copyArgs(const Clip& c,const std::string& output){validate(c);if(c.volume!=100)throw std::runtime_error("Stream copy requires unchanged volume");return {"-hide_banner","-nostdin","-n","-ss",decimal(c.in),"-i",c.path,"-t",decimal(c.out-c.in),"-map","0:v:0","-map","0:a:0?","-c","copy","-avoid_negative_ts","make_zero","-movflags","+faststart",output};}
}
