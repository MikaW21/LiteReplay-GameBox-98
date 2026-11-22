#include "editor/core.hpp"
#include <iostream>
int main(int argc,char** argv){if(argc!=4)return 1;lc::Clip c{argv[1],3,.25,1.75,50,std::string(argv[3])=="1"};lc::Export e{1280,720,30,8,1};for(auto& arg:lc::segmentArgs(c,e,argv[2])){std::cout<<arg<<'\0';}}
