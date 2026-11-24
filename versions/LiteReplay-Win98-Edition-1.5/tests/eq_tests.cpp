#include "eq_config.hpp"
#include <cassert>
#include <iostream>
#include <functional>
using namespace lr;
static void rejects(const std::function<void()>& f){bool failed=false;try{f();}catch(const std::runtime_error&){failed=true;}assert(failed);}
int main(){
    const std::string endpoint="{0.0.0.00000000}.{12345678-abcd-ABCD-1234-1234567890ab}";
    std::array<int,10> gains{};
    assert(endpointGuid(endpoint)=="{12345678-abcd-ABCD-1234-1234567890ab}");
    rejects([]{endpointGuid("headphones");});rejects([]{endpointGuid("{12345678-abcd-ABCD-1234-1234567890az}");});
    auto flat=eqText(endpoint,gains,true);assert(flat.find("Preamp: 0 dB")!=std::string::npos);assert(flat.find("GraphicEQ: 31 0; 62 0;")!=std::string::npos);
    assert(flat.find("Device: all")==std::string::npos);assert(flat.find("Device: {12345678-")!=std::string::npos);
    gains[0]=12;gains[9]=-12;assert(eqHeadroom(gains)==-13);assert(eqText(endpoint,gains,true).find("Preamp: -13 dB")!=std::string::npos);
    assert(eqText(endpoint,gains,false).find("GraphicEQ:")==std::string::npos);
    gains[2]=13;rejects([&]{eqText(endpoint,gains,true);});
    const std::string original="# user filters\r\nDevice: Speakers\r\nPreamp: -4 dB\r\nInclude: other.txt\r\n";
    auto linked=attachEqBlock(original);assert(linked.substr(0,original.size())==original);assert(attachEqBlock(linked)==linked);assert(removeEqBlock(linked)==original);
    auto noNewline=attachEqBlock("# original");assert(removeEqBlock(noNewline)=="# original\n");
    rejects([]{attachEqBlock("Include: LiteReplay-EQ.txt\n");});
    rejects([]{removeEqBlock(std::string(eqBegin)+"\n# user edits\n"+eqEnd);});
    rejects([&]{removeEqBlock(linked+eqBegin);});
    rejects([]{removeEqBlock(eqEnd);});
    rejects([]{attachEqBlock(std::string("a\0b",3));});
    const auto after=linked+"# later changes\nPreamp: -1 dB\n";
    assert(removeEqBlock(after)==original+"# later changes\nPreamp: -1 dB\n");
    std::cout<<"EQ generation, headroom, device scoping, idempotent attach and safe removal passed\n";
}
