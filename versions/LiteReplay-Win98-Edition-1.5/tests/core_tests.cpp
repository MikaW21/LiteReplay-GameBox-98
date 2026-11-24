#include "core.hpp"
#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>
using namespace lr;
Packet packet(int64_t t, bool key, size_t bytes=10, int stream=0) {
    return {std::make_shared<const std::vector<uint8_t>>(bytes, 42), stream, t, t, 100000, key};
}
int main() {
    { PacketRing r(2,10000); r.push(packet(0,false)); assert(r.snapshot(2).empty());
      for(int i=0;i<60;++i) r.push(packet(i*100000LL,i%10==0));
      auto s=r.snapshot(2); assert(!s.empty() && s.front().key && s.front().pts==4000000);
      assert(r.duration()==2000000); }
    { PacketRing r(30,50); for(int i=0;i<20;++i) r.push(packet(i*100000LL,i%3==0));
      assert(r.bytes()<=50); auto s=r.snapshot(30); assert(s.empty() || s.front().key);
      auto saved=s; r.push(packet(2000000,true,100)); assert(r.bytes()<=50);
      if(!saved.empty()) assert(saved.front().data->at(0)==42); }
    { PacketRing r(30,20); r.push(packet(0,true)); r.push(packet(1,false)); r.push(packet(2,false));
      assert(r.snapshot(30).empty()); r.push(packet(3,false)); assert(r.bytes()==0);
      r.push(packet(4,true)); assert(r.snapshot(30).front().pts==4); }
    { PacketRing r(30,1000); r.push(packet(100,true)); r.push(packet(90,false,10,1));
      r.push(packet(200,false)); r.push(packet(150,false,10,1));
      auto s=r.snapshot(30); assert(s.size()==3 && s[1].pts==150); }
    { PacketRing r(600,1000,4); for(int i=0;i<100;++i) r.push(packet(i,i%2==0,1));
      auto s=r.snapshot(600); assert(s.size()<=4 && s.front().key); }
    { AudioTimeline t(8); float in[]={.1f,.2f,.3f,.4f}; float out[4];
      t.put(0,in,2); t.get(0,out,2); assert(out[2]==.3f); t.get(2,out,2); assert(out[0]==0);
      t.put(8,in,2); t.get(0,out,2); assert(out[0]==0); t.put(0,in,2);
      t.get(8,out,2); assert(out[2]==.3f); assert(mix(.8f,.8f)==1); }
    { PacketRing r(5,10000); std::atomic<bool> done{false};
      std::thread p([&]{for(int i=0;i<50000;++i) r.push(packet(i*1000LL,i%100==0));done=true;});
      while(!done) {auto s=r.snapshot(5); assert(s.empty()||s.front().key); assert(r.bytes()<=10000);}
      p.join(); }
    std::cout << "7 core test groups passed\n";
}
