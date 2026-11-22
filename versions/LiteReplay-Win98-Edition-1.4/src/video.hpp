#pragma once
#include "common.hpp"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <map>
namespace lr {
class Video {
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGIOutputDuplication> duplication_;
    ComPtr<ID3D11Texture2D> desktop_;
    ComPtr<ID3D11VideoDevice> videoDevice_;
    ComPtr<ID3D11VideoContext> videoContext_;
    ComPtr<ID3D11VideoProcessorEnumerator> enumerator_;
    ComPtr<ID3D11VideoProcessor> processor_;
    ComPtr<ID3D11VideoProcessorInputView> input_;
    std::map<std::pair<ID3D11Texture2D*,UINT>,ComPtr<ID3D11VideoProcessorOutputView>> outputViews_;
    Buffer hwDevice_, hwFrames_;
    std::vector<Frame> gpuFrames_;
    Codec encoder_;
    Config config_;
    bool hasFrame_=false,cpuFrames_=false;
    std::vector<Frame> cpuPool_;
    std::string diagnostic_;
public:
    explicit Video(const Config& c);
    bool capture();
    Frame frame(int64_t pts);
    const std::string& diagnostic()const{return diagnostic_;}
    AVCodecContext* encoder()const{return encoder_.get();}
};
}
