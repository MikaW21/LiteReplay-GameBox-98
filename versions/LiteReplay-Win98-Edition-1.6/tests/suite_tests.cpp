#include "suite.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
int main(int argc,char** argv){
    using namespace lr;
    SuiteState s;assert(s.mode==RunMode::Balanced);assert(!s.english);
    assert(s.needsMetrics(true,false));s.page=1;assert(!s.needsMetrics(true,false));
    assert(s.needsMetrics(false,true));s.page=5;assert(!s.needsMetrics(false,false));assert(s.needsMetrics(true,false));
    assert(parseMode(-10)==RunMode::Balanced);assert(parseMode(99)==RunMode::Balanced);
    assert(policy(RunMode::Low).monitorMs>policy(RunMode::Balanced).monitorMs);
    assert(policy(RunMode::Performance).thumbnailMiB>=policy(RunMode::Low).thumbnailMiB);
    std::istringstream good("hello=世界\r\n# comment\nvalue=a=b\n");Catalog c;c.read(good);assert(c.get("hello")=="世界");assert(c.get("value")=="a=b");
    bool failed=false;try{std::istringstream bad("x=one\nx=two\n");c.read(bad);}catch(const std::exception&){failed=true;}
    assert(failed);assert(c.get("hello")=="世界");
    failed=false;try{c.get("missing");}catch(const std::exception&){failed=true;}assert(failed);
    assert(argc==2);Catalog en,zh;std::ifstream a(std::string(argv[1])+"/en-US.lang"),b(std::string(argv[1])+"/zh-CN.lang");assert(a&&b);en.read(a);zh.read(b);assert(en.values().size()==zh.values().size());
    for(auto& v:en.values())assert(!zh.get(v.first).empty());
    std::cout<<"PASS: policy, subscriptions, invalid settings, catalog rollback and bilingual key coverage\n";
}
