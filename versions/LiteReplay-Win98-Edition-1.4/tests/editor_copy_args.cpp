#include "editor/history.hpp"
#include <iostream>
int main(int argc,char** argv){if(argc!=3)return 1;lc::Clip c{argv[1],4,1.3,2.8,100,true};for(auto& a:lc::copyArgs(c,argv[2]))std::cout<<a<<'\0';}
