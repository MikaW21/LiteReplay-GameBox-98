#include "editor/media_probe.hpp"
#include <fstream>
#include <iostream>
int main(int argc,char** argv){if(argc!=4)return 1;std::ifstream file(argv[2]);std::string text((std::istreambuf_iterator<char>(file)),{});lc::Project p;p.clips={lc::parseMediaProbe(argv[1],text)};p.settings=lc::sourceLimit(p);p.settings.encoder=1;lc::enforceLimit(p,p.settings);for(const auto& arg:lc::segmentArgs(p.clips[0],p.settings,argv[3]))std::cout<<arg<<'\0';}
