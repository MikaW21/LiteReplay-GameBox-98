#include "core.hpp"
#include <cassert>
#include <limits>
int main(){using namespace lr;assert(recordingGain(.5f,100)==.5f);assert(recordingGain(.5f,0)==0);assert(recordingGain(.5f,50)==.25f);assert(recordingGain(-.75f,200)==-1);assert(recordingGain(.8f,200)==1);assert(recordingGain(.25f,999)==.5f);assert(recordingGain(.5f,-5)==0);assert(recordingGain(std::numeric_limits<float>::quiet_NaN(),100)==0);assert(recordingGain(std::numeric_limits<float>::infinity(),100)==0);assert(mix(recordingGain(.5f,50),recordingGain(.25f,200))==.75f);assert(mix(recordingGain(.8f,200),recordingGain(.5f,100))==1);}
