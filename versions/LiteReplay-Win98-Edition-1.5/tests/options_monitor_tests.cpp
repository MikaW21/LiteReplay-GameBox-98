#include "profiles.hpp"
#include "monitor_math.hpp"
#include <cassert>
#include <limits>
#include <iostream>
int main(){
    using namespace lr;
    assert(resolutions[resolutionIndex(1280)].height==720);
    assert(frameRates[fpsIndex(75)]==75);
    assert(resolutions[resolutionIndex(1920)].height==1080);
    assert(frameRates[fpsIndex(60)]==60);
    assert(frameRates[fpsIndex(0)]==60 && resolutions[resolutionIndex(123)].width==1920);
    for(const auto& r:resolutions)assert(r.width%2==0&&r.height%2==0);
    GpuUsage g;
    g.add(L"pid_1_luid_0x0_0x1_phys_0_eng_0_engtype_3D",60,1);
    g.add(L"pid_10_luid_0x0_0x1_phys_0_eng_0_engtype_3D",30,1);
    g.add(L"pid_1_luid_0x0_0x1_phys_0_eng_1_engtype_VideoEncode",20,1);
    assert(GpuUsage::peak(g.system)==90);
    assert(GpuUsage::peak(g.process)==60);
    assert(GpuUsage::peak(g.encode)==20);
    g.add(L"pid_1_luid_0x0_0x1_phys_0_eng_0_engtype_3D#1",5,1);
    assert(GpuUsage::peak(g.system)==95);
    g.add(L"pid_2_luid_0x0_0x2_phys_0_eng_0_engtype_3D",99,1);
    assert(GpuUsage::peak(g.system)==99);
    g.add(L"pid_1_luid_0x0_0x1_phys_0_eng_1_engtype_VideoEncode",std::numeric_limits<double>::quiet_NaN(),1);
    g.add(L"pid_1_luid_0x0_0x1_phys_0_eng_1_engtype_VideoEncode",-5,1);
    assert(GpuUsage::peak(g.encode)==20);
    assert(GpuUsage::peak({})==-1);
    assert(cpuBusy(100,80)==20 && cpuBusy(0,0)==-1 && cpuBusy(10,11)==-1);
    std::cout<<"Options migration and performance aggregation checks passed\n";
}
