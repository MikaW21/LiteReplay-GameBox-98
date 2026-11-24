#pragma once
#include <string>
#include <functional>
#include <cmath>
namespace lc {
class ProgressLines {
 std::string pending;
public:
 void feed(const std::string& chunk,const std::function<void(double)>& report){
  pending+=chunk;size_t end;
  while((end=pending.find('\n'))!=std::string::npos){auto line=pending.substr(0,end);pending.erase(0,end+1);if(line.rfind("out_time_us=",0)==0){try{size_t used;double us=std::stod(line.substr(12),&used);if((used==line.size()-12||(used==line.size()-13&&line.back()=='\r'))&&std::isfinite(us)&&us>=0)report(us/1000000.0);}catch(...){}}}
  if(pending.size()>8192)pending.clear();
 }
};
}
