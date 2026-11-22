#include "engine.hpp"
#include <dxgi1_2.h>
#include <iostream>
int main(int argc,char** argv){
 SetConsoleOutputCP(CP_UTF8);
 std::cout<<"FFmpeg: "<<av_version_info()<<"\nBuild: "<<avcodec_configuration()<<"\n";
 try{
  Microsoft::WRL::ComPtr<IDXGIFactory1> factory;lr::check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"DXGI factory");int index=0;
  for(UINT n=0;;++n){Microsoft::WRL::ComPtr<IDXGIAdapter1> a;auto hr=factory->EnumAdapters1(n,&a);if(hr==DXGI_ERROR_NOT_FOUND)break;lr::check(hr,"EnumAdapters");DXGI_ADAPTER_DESC1 ad{};lr::check(a->GetDesc1(&ad),"GetDesc1");if(ad.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
   for(UINT o=0;;++o){Microsoft::WRL::ComPtr<IDXGIOutput> output;hr=a->EnumOutputs(o,&output);if(hr==DXGI_ERROR_NOT_FOUND)break;lr::check(hr,"EnumOutputs");DXGI_OUTPUT_DESC d{};lr::check(output->GetDesc(&d),"Output desc");if(!d.AttachedToDesktop)continue;std::cout<<"Display "<<index++<<": "<<lr::utf8(d.DeviceName)<<" / "<<lr::utf8(ad.Description)<<" vendor="<<ad.VendorId<<"\n";}
  }
  bool compiled=true;for(const char* n:{"h264_nvenc","hevc_nvenc","av1_nvenc","h264_amf","hevc_amf","av1_amf","h264_mf","aac"}){bool ok=avcodec_find_encoder_by_name(n)!=nullptr;std::cout<<n<<": "<<(ok?"compiled (driver/hardware NOT yet tested)":"NOT compiled")<<"\n";compiled&=ok;}
  lr::Config c;c.width=1280;c.height=720;c.fps=30;c.mbps=6;c.memoryMiB=128;c.folder=(std::filesystem::current_path()/L"LiteReplayTest").wstring();bool capture=false;
  for(int i=1;i<argc;++i){std::string arg=argv[i];if(arg=="--capture")capture=true;else if(arg=="--allow-cpu")c.allowSoftware=true;else if(arg.rfind("--backend=",0)==0){c.backend=arg.substr(10);if(c.backend!="auto"&&c.backend!="nvenc"&&c.backend!="amf"&&c.backend!="mf_hw"&&c.backend!="mf_sw")throw std::runtime_error("Invalid backend");}else if(arg.rfind("--monitor=",0)==0){size_t used=0;auto val=arg.substr(10);c.monitor=std::stoi(val,&used);if(used!=val.size()||c.monitor<0||c.monitor>15)throw std::runtime_error("Invalid monitor");}else throw std::runtime_error("Unknown option");}
  if(!capture){std::cout<<"No capture performed. Use --capture [--backend=auto|nvenc|amf|mf_hw|mf_sw] [--allow-cpu] [--monitor=0]\n";return compiled?0:1;}
  std::cout<<"Testing 720p30, H.264, 6 Mbps. CPU fallback: "<<(c.allowSoftware?"allowed":"disabled")<<"\n";
  lr::Engine engine([](const std::string& s){std::cerr<<s<<'\n';});engine.start(c);
  auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(35);
  while(engine.frames<150&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(100));
  if(engine.frames<150){engine.stop();std::cerr<<engine.statusText()<<"\nCapture did not reach 150 frames\n";return 2;}
  if(!engine.save()){engine.stop();std::cerr<<"Save not ready: "<<engine.statusText()<<'\n';return 3;}
  deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
  while(engine.saving()&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(100));
  engine.stop();if(engine.lastPath().empty())return 4;
  std::cout<<"Saved: "<<lr::utf8(engine.lastPath())<<"\nPlay and listen to validate. This is NOT a game performance or stability result.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 5;}
}
