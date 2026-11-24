#include "video.hpp"
#include "encoder_policy.hpp"
#include <dxgi1_6.h>
namespace lr {
Video::Video(const Config& c): config_(c) {
    ComPtr<IDXGIFactory1> factory;
    check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"DXGI factory");
    ComPtr<IDXGIAdapter1> selected;
    ComPtr<IDXGIOutput> output;
    int index=0;UINT vendor=0;std::wstring adapterName,outputName;
    for(UINT a=0; !selected; ++a) {
        ComPtr<IDXGIAdapter1> adapter;
        HRESULT hr=factory->EnumAdapters1(a,&adapter);
        if(hr==DXGI_ERROR_NOT_FOUND) break;
        check(hr,"enumerate adapters");
        DXGI_ADAPTER_DESC1 ad{}; check(adapter->GetDesc1(&ad),"adapter description");
        if(ad.Flags&DXGI_ADAPTER_FLAG_SOFTWARE) continue;
        for(UINT o=0;;++o) {
            ComPtr<IDXGIOutput> item;
            hr=adapter->EnumOutputs(o,&item); if(hr==DXGI_ERROR_NOT_FOUND)break;
            check(hr,"enumerate outputs"); DXGI_OUTPUT_DESC od{}; check(item->GetDesc(&od),"output description");
            if(!od.AttachedToDesktop)continue;
            if(index++==c.monitor){selected=adapter;output=item;vendor=ad.VendorId;adapterName=ad.Description;outputName=od.DeviceName;break;}
        }
    }
    if(!selected)throw std::runtime_error("找不到所选屏幕。编号包含所有显卡当前连接的屏幕；请先选择 0 或查看诊断屏幕列表。");
    D3D_FEATURE_LEVEL level;
    check(D3D11CreateDevice(selected.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT|D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
        nullptr,0,D3D11_SDK_VERSION,&device_,&level,&context_),"D3D11 device");
    ComPtr<IDXGIOutput1> output1; check(output.As(&output1),"DXGI output1");
    check(output1->DuplicateOutput(device_.Get(),&duplication_),"DuplicateOutput");
    DXGI_OUTDUPL_DESC dd{}; duplication_->GetDesc(&dd);
    if(dd.Rotation!=DXGI_MODE_ROTATION_IDENTITY && dd.Rotation!=DXGI_MODE_ROTATION_UNSPECIFIED)
        throw std::runtime_error("当前版本暂不支持旋转屏幕。");
    // Reject HDR rather than silently emit incorrect SDR colors.
    ComPtr<IDXGIOutput6> output6;
    if(SUCCEEDED(output.As(&output6))) {
        DXGI_OUTPUT_DESC1 od{};
        if(SUCCEEDED(output6->GetDesc1(&od)) && od.ColorSpace==DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
            throw std::runtime_error("当前版本只支持 SDR，请关闭 Windows HDR。");
    }
    check(device_.As(&videoDevice_),"D3D11 video device");
    check(context_.As(&videoContext_),"D3D11 video context");
    D3D11_TEXTURE2D_DESC td{}; td.Width=dd.ModeDesc.Width;td.Height=dd.ModeDesc.Height;
    td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_RENDER_TARGET;
    check(device_->CreateTexture2D(&td,nullptr,&desktop_),"desktop GPU texture");
    D3D11_VIDEO_PROCESSOR_CONTENT_DESC pd{};pd.InputFrameFormat=D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    pd.InputWidth=td.Width;pd.InputHeight=td.Height;pd.OutputWidth=c.width;pd.OutputHeight=c.height;
    pd.InputFrameRate={UINT(c.fps),1};pd.OutputFrameRate={UINT(c.fps),1};pd.Usage=D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
    check(videoDevice_->CreateVideoProcessorEnumerator(&pd,&enumerator_),"video processor enumerator");
    UINT support=0;check(enumerator_->CheckVideoProcessorFormat(DXGI_FORMAT_NV12,&support),"NV12 support");
    if(!(support&D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT))throw std::runtime_error("显卡无法输出所需的 NV12 格式");
    check(videoDevice_->CreateVideoProcessor(enumerator_.Get(),0,&processor_),"video processor");
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC iv{};iv.ViewDimension=D3D11_VPIV_DIMENSION_TEXTURE2D;
    check(videoDevice_->CreateVideoProcessorInputView(desktop_.Get(),enumerator_.Get(),&iv,&input_),"video input view");
    RECT src{0,0,LONG(td.Width),LONG(td.Height)};
    // Letterbox instead of stretching non-16:9 desktops.
    const double scale=std::min(double(c.width)/td.Width,double(c.height)/td.Height);
    const LONG w=LONG(td.Width*scale)&~1L,h=LONG(td.Height*scale)&~1L;
    RECT dst{(c.width-w)/2,(c.height-h)/2,(c.width+w)/2,(c.height+h)/2};
    RECT target{0,0,c.width,c.height};
    videoContext_->VideoProcessorSetStreamSourceRect(processor_.Get(),0,TRUE,&src);
    videoContext_->VideoProcessorSetStreamDestRect(processor_.Get(),0,TRUE,&dst);
    videoContext_->VideoProcessorSetOutputTargetRect(processor_.Get(),TRUE,&target);
    videoContext_->VideoProcessorSetStreamFrameFormat(processor_.Get(),0,D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
    videoContext_->VideoProcessorSetStreamAutoProcessingMode(processor_.Get(),0,FALSE);
    D3D11_VIDEO_COLOR black{}; black.RGBA.A=1;
    videoContext_->VideoProcessorSetOutputBackgroundColor(processor_.Get(),FALSE,&black);
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE in{};in.RGB_Range=0;in.YCbCr_Matrix=1;
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE out{};out.YCbCr_Matrix=1;out.Nominal_Range=1;
    videoContext_->VideoProcessorSetStreamColorSpace(processor_.Get(),0,&in);
    videoContext_->VideoProcessorSetOutputColorSpace(processor_.Get(),&out);
    hwDevice_.p=av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA); if(!hwDevice_.p)throw std::bad_alloc();
    auto hd=reinterpret_cast<AVHWDeviceContext*>(hwDevice_.p->data);
    auto d3d=static_cast<AVD3D11VADeviceContext*>(hd->hwctx);
    d3d->device=device_.Get(); d3d->device->AddRef();
    avcheck(av_hwdevice_ctx_init(hwDevice_.p),"FFmpeg D3D11 device");
    hwFrames_.p=av_hwframe_ctx_alloc(hwDevice_.p);if(!hwFrames_.p)throw std::bad_alloc();
    auto frames=reinterpret_cast<AVHWFramesContext*>(hwFrames_.p->data);
    frames->format=AV_PIX_FMT_D3D11;frames->sw_format=AV_PIX_FMT_NV12;
    frames->width=c.width;frames->height=c.height;frames->initial_pool_size=16;
    auto f3=static_cast<AVD3D11VAFramesContext*>(frames->hwctx);
    f3->BindFlags=D3D11_BIND_RENDER_TARGET;
    avcheck(av_hwframe_ctx_init(hwFrames_.p),"D3D11 NV12 frame pool");
    // FFmpeg's render-target pool can grow dynamically. Hold exactly sixteen
    // frames here and reuse only those whose AVBuffer has no encoder reference.
    for(int i=0;i<16;++i){
        Frame f(av_frame_alloc());if(!f)throw std::bad_alloc();
        avcheck(av_hwframe_get_buffer(hwFrames_.p,f.get(),0),"preallocate NV12 GPU frame");
        gpuFrames_.push_back(std::move(f));
    }
    std::string errors;
    for(const auto& choice:encoderChoices(c.backend,c.codec,vendor,c.allowSoftware)){
        try{
            auto codec=avcodec_find_encoder_by_name(choice.name.c_str());
            if(!codec)throw std::runtime_error("Encoder not compiled into this FFmpeg: "+choice.name);
            Codec candidate(avcodec_alloc_context3(codec));if(!candidate)throw std::bad_alloc();
            auto e=candidate.get();e->width=c.width;e->height=c.height;e->pix_fmt=choice.cpuFrames?AV_PIX_FMT_NV12:AV_PIX_FMT_D3D11;
            e->time_base={1,c.fps};e->framerate={c.fps,1};e->gop_size=c.fps;e->max_b_frames=0;
            e->bit_rate=int64_t(c.mbps)*1000000;e->rc_max_rate=e->bit_rate;e->rc_buffer_size=int(e->bit_rate);
            e->flags|=AV_CODEC_FLAG_GLOBAL_HEADER;e->thread_count=2;
            e->color_range=AVCOL_RANGE_MPEG;e->colorspace=AVCOL_SPC_BT709;e->color_primaries=AVCOL_PRI_BT709;e->color_trc=AVCOL_TRC_BT709;
            if(!choice.cpuFrames){e->hw_frames_ctx=av_buffer_ref(hwFrames_.p);if(!e->hw_frames_ctx)throw std::bad_alloc();}
            if(choice.name.find("nvenc")!=std::string::npos){
                avcheck(av_opt_set(e->priv_data,"preset","p1",0),"NVENC preset");
                avcheck(av_opt_set(e->priv_data,"tune","ll",0),"NVENC tune");
                avcheck(av_opt_set(e->priv_data,"rc","cbr",0),"NVENC CBR");
                avcheck(av_opt_set_int(e->priv_data,"rc-lookahead",0,0),"NVENC lookahead");
                avcheck(av_opt_set_int(e->priv_data,"delay",0,0),"NVENC delay");
                avcheck(av_opt_set_int(e->priv_data,"forced-idr",1,0),"NVENC IDR");
            }else if(choice.name.find("amf")!=std::string::npos){
                avcheck(av_opt_set(e->priv_data,"usage","ultralowlatency",0),"AMF usage");
                avcheck(av_opt_set(e->priv_data,"quality","speed",0),"AMF quality");
                avcheck(av_opt_set(e->priv_data,"rc","cbr",0),"AMF CBR");
            }else{
                avcheck(av_opt_set_int(e->priv_data,"hw_encoding",choice.software?0:1,0),"Media Foundation encoder selection");
            }
            avcheck(avcodec_open2(e,codec,nullptr),choice.name.c_str());
            encoder_=std::move(candidate);cpuFrames_=choice.cpuFrames;
            diagnostic_="Capture: "+utf8(outputName)+" / "+utf8(adapterName)+"; encoder: "+choice.name+(choice.software?" [SOFTWARE]":" [HARDWARE]")+(cpuFrames_?"; GPU -> CPU NV12 transfer":"; D3D11 GPU frames");
            if(!errors.empty())diagnostic_+="; earlier attempts: "+errors;
            break;
        }catch(const std::exception& e){errors+=choice.name+(choice.software?"(software): ":"(hardware): ")+e.what()+"; ";}
    }
    if(!encoder_)throw std::runtime_error("编码器初始化失败 / Encoder initialization failed: "+errors+"请选 H.264 + Windows MF 硬件；仍失败时明确选择 CPU 兼容模式，建议先用 720p30。精简系统可能缺少 Media Foundation。");
    if(cpuFrames_){for(int i=0;i<8;++i){Frame f(av_frame_alloc());if(!f)throw std::bad_alloc();f->format=AV_PIX_FMT_NV12;f->width=c.width;f->height=c.height;avcheck(av_frame_get_buffer(f.get(),32),"CPU NV12 frame pool");cpuPool_.push_back(std::move(f));}}

}
bool Video::capture() {
    DXGI_OUTDUPL_FRAME_INFO info{};ComPtr<IDXGIResource> resource;
    HRESULT hr=duplication_->AcquireNextFrame(0,&info,&resource);
    if(hr==DXGI_ERROR_WAIT_TIMEOUT)return hasFrame_;
    check(hr,"AcquireNextFrame");
    struct Release {IDXGIOutputDuplication* d;~Release(){d->ReleaseFrame();}} release{duplication_.Get()};
    ComPtr<ID3D11Texture2D> texture;check(resource.As(&texture),"desktop texture");
    context_->CopyResource(desktop_.Get(),texture.Get());hasFrame_=true;return true;
}
Frame Video::frame(int64_t pts) {
    Frame f;
    for(auto& candidate:gpuFrames_)if(av_frame_is_writable(candidate.get())){
        f.reset(av_frame_clone(candidate.get()));break;
    }
    if(!f)throw std::runtime_error("16 个 GPU 帧均繁忙，正在重建采集会话以避免缓存增长");
    auto texture=reinterpret_cast<ID3D11Texture2D*>(f->data[0]);
    const UINT slice=UINT(reinterpret_cast<uintptr_t>(f->data[1]));
    D3D11_TEXTURE2D_DESC td{};texture->GetDesc(&td);
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC ov{};
    if(td.ArraySize==1){ov.ViewDimension=D3D11_VPOV_DIMENSION_TEXTURE2D;ov.Texture2D.MipSlice=0;}
    else{
        ov.ViewDimension=D3D11_VPOV_DIMENSION_TEXTURE2DARRAY;
        ov.Texture2DArray.MipSlice=0;ov.Texture2DArray.FirstArraySlice=slice;ov.Texture2DArray.ArraySize=1;
    }
    auto key=std::make_pair(texture,slice);
    auto& view=outputViews_[key];
    if(!view)check(videoDevice_->CreateVideoProcessorOutputView(key.first,
        enumerator_.Get(),&ov,&view),"NV12 video output view");
    D3D11_VIDEO_PROCESSOR_STREAM stream{};stream.Enable=TRUE;stream.pInputSurface=input_.Get();
    check(videoContext_->VideoProcessorBlt(processor_.Get(),view.Get(),0,1,&stream),"GPU scale / BGRA to NV12");
    context_->Flush();f->pts=pts;
    if(pts%config_.fps==0)f->pict_type=AV_PICTURE_TYPE_I;
    if(cpuFrames_){
        Frame cpu;for(auto& candidate:cpuPool_)if(av_frame_is_writable(candidate.get())){cpu.reset(av_frame_clone(candidate.get()));break;}
        if(!cpu)throw std::runtime_error("CPU frame pool busy; reduce recording resolution/FPS");
        avcheck(av_hwframe_transfer_data(cpu.get(),f.get(),0),"GPU -> CPU NV12 transfer");
        avcheck(av_frame_copy_props(cpu.get(),f.get()),"copy frame timing");return cpu;
    }
    return f;
}
}
