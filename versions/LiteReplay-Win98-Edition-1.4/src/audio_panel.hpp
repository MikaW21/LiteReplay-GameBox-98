#pragma once
#include "common.hpp"
#include "ui_theme.hpp"
#include "eq_config.hpp"
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <functiondiscoverykeys_devpkey.h>
#include <commctrl.h>
#include <commdlg.h>
#include <fstream>
namespace lr {
class AudioPanel {
    static constexpr PROPERTYKEY friendlyNameKey_={{0xa45c254e,0xdf1c,0x4efd,{0x80,0x20,0x67,0xd1,0x46,0xa8,0x50,0xe0}},14};
    struct Device {std::wstring id,name;};
    struct Session {ComPtr<ISimpleAudioVolume> volume;std::wstring id,name;};
    HWND window_=nullptr;HFONT font_;
    std::filesystem::path settings_,apoConfig_;
    ComPtr<IMMDeviceEnumerator> enumerator_;
    ComPtr<IAudioEndpointVolume> master_;
    std::vector<Device> devices_;std::vector<Session> sessions_;
    std::wstring deviceId_;
    std::array<int,10> gains_{};
    bool dragging_=false;
    static AudioPanel* self(HWND h){return reinterpret_cast<AudioPanel*>(GetWindowLongPtrW(h,GWLP_USERDATA));}
    HWND item(int id)const{return GetDlgItem(window_,id);}
    void status(const std::wstring& text){SetWindowTextW(item(90),text.c_str());}
    int selected(int id)const{return int(SendMessageW(item(id),id==2?LB_GETCURSEL:CB_GETCURSEL,0,0));}
    static std::string readFile(const std::filesystem::path& path){
        if(std::filesystem::file_size(path)>1024*1024)throw std::runtime_error("APO 配置文件超过 1 MiB，未修改");
        std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("无法读取 APO 配置");return {std::istreambuf_iterator<char>(in),{}};
    }
    static void atomicWrite(const std::filesystem::path& path,const std::string& data){
        auto temp=path;temp+=L".LiteReplay.tmp";
        HANDLE h=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(h==INVALID_HANDLE_VALUE)throw std::runtime_error("无法写入配置目录（权限不足或存在 .LiteReplay.tmp）。可先导出 EQ；不要删除不明临时文件。");
        DWORD wrote=0;bool ok=WriteFile(h,data.data(),DWORD(data.size()),&wrote,nullptr)&&wrote==data.size();if(ok)ok=FlushFileBuffers(h);CloseHandle(h);
        if(!ok){DeleteFileW(temp.c_str());throw std::runtime_error("配置写入失败，原文件未替换");}
        if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){DeleteFileW(temp.c_str());throw std::runtime_error("无法替换配置，原文件未修改");}
    }
    void persist(){
        auto put=[&](const wchar_t* key,const std::wstring& value){if(!WritePrivateProfileStringW(L"AudioPanel",key,value.c_str(),settings_.c_str()))throw std::runtime_error("音频面板设置保存失败");};
        put(L"Device",deviceId_);put(L"APOConfig",apoConfig_.wstring());for(size_t i=0;i<10;++i)put((L"Band"+std::to_wstring(i)).c_str(),std::to_wstring(gains_[i]));
    }
    void devices(){
        const auto old=deviceId_;devices_.clear();SendMessageW(item(1),CB_RESETCONTENT,0,0);
        enumerator_.Reset();
        check(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator_)),"MMDeviceEnumerator");
        ComPtr<IMMDeviceCollection> list;check(enumerator_->EnumAudioEndpoints(eRender,DEVICE_STATE_ACTIVE,&list),"EnumAudioEndpoints");UINT count=0;check(list->GetCount(&count),"GetCount");
        ComPtr<IMMDevice> def;std::wstring defaultId;if(SUCCEEDED(enumerator_->GetDefaultAudioEndpoint(eRender,eConsole,&def))){LPWSTR id=nullptr;if(SUCCEEDED(def->GetId(&id))){defaultId=id;CoTaskMemFree(id);}}
        for(UINT i=0;i<count;++i){ComPtr<IMMDevice> d;if(FAILED(list->Item(i,&d)))continue;LPWSTR id=nullptr;if(FAILED(d->GetId(&id)))continue;Device value{id,id};CoTaskMemFree(id);
            ComPtr<IPropertyStore> props;if(SUCCEEDED(d->OpenPropertyStore(STGM_READ,&props))){PROPVARIANT v;PropVariantInit(&v);if(SUCCEEDED(props->GetValue(friendlyNameKey_,&v))&&v.vt==VT_LPWSTR)value.name=v.pwszVal;PropVariantClear(&v);}
            std::wstring label=value.name+(value.id==defaultId?L"（默认）":L"");SendMessageW(item(1),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));devices_.push_back(std::move(value));
        }
        int index=-1;for(size_t i=0;i<devices_.size();++i)if(devices_[i].id==old)index=int(i);
        if(index<0)for(size_t i=0;i<devices_.size();++i)if(devices_[i].id==defaultId)index=int(i);
        if(index<0&&!devices_.empty())index=0;SendMessageW(item(1),CB_SETCURSEL,index,0);
        deviceId_=index<0?L"":devices_[size_t(index)].id;sessions(true);
    }
    void sessions(bool rebind=false){
        std::wstring old;int index=selected(2);if(index>=0&&size_t(index)<sessions_.size())old=sessions_[size_t(index)].id;
        sessions_.clear();SendMessageW(item(2),LB_RESETCONTENT,0,0);if(rebind)master_.Reset();
        if(deviceId_.empty()){EnableWindow(item(5),FALSE);EnableWindow(item(6),FALSE);enableSession(false);status(L"没有可用播放设备，请连接耳机后刷新。");return;}
        ComPtr<IMMDevice> device;check(enumerator_->GetDevice(deviceId_.c_str(),&device),"耳机已断开，请刷新设备");
        if(!master_)check(device->Activate(__uuidof(IAudioEndpointVolume),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(master_.GetAddressOf())),"EndpointVolume");
        EnableWindow(item(5),TRUE);EnableWindow(item(6),TRUE);
        ComPtr<IAudioSessionManager2> manager;check(device->Activate(__uuidof(IAudioSessionManager2),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(manager.GetAddressOf())),"AudioSessionManager");
        ComPtr<IAudioSessionEnumerator> list;check(manager->GetSessionEnumerator(&list),"SessionEnumerator");int count=0;check(list->GetCount(&count),"SessionCount");
        for(int i=0;i<count;++i){ComPtr<IAudioSessionControl> ctrl;ComPtr<IAudioSessionControl2> ctrl2;Session row;
            if(FAILED(list->GetSession(i,&ctrl))||FAILED(ctrl.As(&ctrl2))||FAILED(ctrl.As(&row.volume)))continue;
            AudioSessionState state;if(FAILED(ctrl->GetState(&state))||state==AudioSessionStateExpired)continue;
            LPWSTR id=nullptr;if(FAILED(ctrl2->GetSessionInstanceIdentifier(&id)))continue;row.id=id;CoTaskMemFree(id);
            DWORD pid=0;ctrl2->GetProcessId(&pid);
            if(ctrl2->IsSystemSoundsSession()==S_OK)row.name=L"系统提示音";
            else {HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(process){wchar_t path[32768];DWORD n=32768;if(QueryFullProcessImageNameW(process,0,path,&n))row.name=std::filesystem::path(path).filename().wstring();CloseHandle(process);}
                if(row.name.empty())row.name=L"应用";row.name+=L" · PID "+std::to_wstring(pid);}
            row.name+=state==AudioSessionStateActive?L" · 播放中":L" · 空闲";
            SendMessageW(item(2),LB_ADDSTRING,0,reinterpret_cast<LPARAM>(row.name.c_str()));sessions_.push_back(std::move(row));
        }
        int target=sessions_.empty()?-1:0;for(size_t i=0;i<sessions_.size();++i)if(sessions_[i].id==old)target=int(i);SendMessageW(item(2),LB_SETCURSEL,target,0);
        enableSession(target>=0);syncVolumes();SetTimer(window_,1,1000,nullptr);
    }
    void enableSession(bool yes){EnableWindow(item(3),yes);EnableWindow(item(4),yes);if(!yes){SetWindowTextW(item(7),L"先让软件播放声音，再点刷新应用");SendMessageW(item(3),TBM_SETPOS,TRUE,0);SendMessageW(item(4),BM_SETCHECK,BST_UNCHECKED,0);}}
    void syncVolumes(){
        if(dragging_)return;
        float value;BOOL mute;
        if(master_){check(master_->GetMasterVolumeLevelScalar(&value),"读取设备音量失败，请刷新设备");check(master_->GetMute(&mute),"GetMute");SendMessageW(item(5),TBM_SETPOS,TRUE,int(value*100+.5f));SendMessageW(item(6),BM_SETCHECK,mute?BST_CHECKED:BST_UNCHECKED,0);SetWindowTextW(item(8),(L"耳机总音量 "+std::to_wstring(int(value*100+.5f))+L"%").c_str());}
        int index=selected(2);if(index>=0&&size_t(index)<sessions_.size()){auto& v=sessions_[size_t(index)].volume;check(v->GetMasterVolume(&value),"应用已退出，请刷新应用");check(v->GetMute(&mute),"GetMute");SendMessageW(item(3),TBM_SETPOS,TRUE,int(value*100+.5f));SendMessageW(item(4),BM_SETCHECK,mute?BST_CHECKED:BST_UNCHECKED,0);SetWindowTextW(item(7),(L"所选会话音量 "+std::to_wstring(int(value*100+.5f))+L"%").c_str());}
    }
    void eqLabels(){for(int i=0;i<10;++i){SetWindowTextW(item(120+i),(std::to_wstring(gains_[size_t(i)])+L" dB").c_str());}SetWindowTextW(item(41),(L"自动前级："+std::to_wstring(eqHeadroom(gains_))+L" dB · 改动后点击应用，尚未试听验证").c_str());}
    void setGains(){for(int i=0;i<10;++i)SendMessageW(item(100+i),TBM_SETPOS,TRUE,12-gains_[size_t(i)]);eqLabels();}
    void preset(int index){
        static const std::array<int,10> presets[]={{0,0,0,0,0,0,0,0,0,0},{4,3,2,0,-1,0,0,0,0,0},{-2,-2,-1,0,2,3,2,1,0,-1},{-3,-3,-2,0,1,2,3,2,0,-1},{2,3,1,-1,0,1,2,1,1,0},{0,0,0,0,0,0,-1,-2,-3,-4}};
        if(index>=0&&index<6)gains_=presets[index];setGains();persist();
    }
    void chooseConfig(){wchar_t file[32768]{};wcsncpy_s(file,apoConfig_.c_str(),_TRUNCATE);OPENFILENAMEW of{sizeof of};of.hwndOwner=window_;of.lpstrFilter=L"Equalizer APO config.txt\0config.txt\0\0";of.lpstrFile=file;of.nMaxFile=32768;of.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
        if(GetOpenFileNameW(&of)){apoConfig_=file;persist();SetWindowTextW(item(42),apoConfig_.c_str());status(L"已选择配置文件；未应用 EQ。请确认 APO 已绑定所选耳机。");}}
    void applyEq(bool bypass){
        if(deviceId_.empty())throw std::runtime_error("请先选择耳机");
        if(apoConfig_.empty()||apoConfig_.filename()!=L"config.txt")throw std::runtime_error("请先安装并配置 Equalizer APO，然后选择其 config.txt");
        const auto original=readFile(apoConfig_);auto updated=attachEqBlock(original);
        auto child=apoConfig_.parent_path()/L"LiteReplay-EQ.txt";
        if(std::filesystem::exists(child)&&readFile(child).rfind(eqMarker,0)!=0)throw std::runtime_error("LiteReplay-EQ.txt 已被其他内容占用，未覆盖");
        // Backup before either file changes. Main config has only an owned include added.
        auto backup=apoConfig_;backup+=L".LiteReplay-"+std::to_wstring(GetTickCount64())+L".bak";
        if(updated!=original)std::filesystem::copy_file(apoConfig_,backup);
        atomicWrite(child,eqText(utf8(deviceId_),gains_,!bypass));
        try{if(updated!=original)atomicWrite(apoConfig_,updated);}catch(...){status(L"EQ 子文件已写入，但主配置接入失败；请检查目录权限。已有接入时参数可能已更新。");throw;}
        persist();status(bypass?L"已写入旁路（仅停用 LiteReplay EQ）；需试听验证。":L"EQ 配置已写入；设备 APO 是否实际生效需试听验证。退出 LiteReplay 后 EQ 仍保留。");
    }
    void detach(){if(apoConfig_.empty())throw std::runtime_error("请先选择 APO config.txt");auto original=readFile(apoConfig_);auto updated=removeEqBlock(original);if(updated==original){status(L"未找到 LiteReplay 接入块，没有修改。");return;}auto backup=apoConfig_;backup+=L".LiteReplay-"+std::to_wstring(GetTickCount64())+L".bak";std::filesystem::copy_file(apoConfig_,backup);atomicWrite(apoConfig_,updated);status(L"已移除 LiteReplay 接入块；保留其他配置与备份。请试听验证。");}
    void exportEq(){if(deviceId_.empty())throw std::runtime_error("请先选择耳机");wchar_t file[32768]=L"LiteReplay-EQ.txt";OPENFILENAMEW of{sizeof of};of.hwndOwner=window_;of.lpstrFilter=L"EQ 文本\0*.txt\0\0";of.lpstrFile=file;of.nMaxFile=32768;of.lpstrDefExt=L"txt";of.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;if(GetSaveFileNameW(&of)){atomicWrite(file,eqText(utf8(deviceId_),gains_,true));status(L"已导出 EQ 文件，导出本身不会改变耳机声音。");}}
    static LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM w,LPARAM l){
        if(msg==WM_NCCREATE)SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams));auto p=self(h);if(!p)return DefWindowProcW(h,msg,w,l);
        try{
            if(msg==WM_ERASEBKGND){lr_ui::paint(h,reinterpret_cast<HDC>(w));return 1;}
            if(msg==WM_CTLCOLORSTATIC||msg==WM_CTLCOLOREDIT||msg==WM_CTLCOLORLISTBOX||msg==WM_CTLCOLORBTN)
                return lr_ui::color(h,msg,w,l);
            if(msg==WM_CLOSE){KillTimer(h,1);p->dragging_=false;ShowWindow(h,SW_HIDE);p->sessions_.clear();p->master_.Reset();p->enumerator_.Reset();return 0;}
            if(msg==WM_TIMER){if(IsIconic(h))return 0;if(!p->dragging_)p->syncVolumes();return 0;}
            if(msg==WM_HSCROLL||msg==WM_VSCROLL){int id=GetDlgCtrlID(reinterpret_cast<HWND>(l));int pos=int(SendMessageW(reinterpret_cast<HWND>(l),TBM_GETPOS,0,0));
                p->dragging_=LOWORD(w)==TB_THUMBTRACK;
                if(id==3){int index=p->selected(2);if(index>=0&&size_t(index)<p->sessions_.size())check(p->sessions_[size_t(index)].volume->SetMasterVolume(pos/100.f,nullptr),"调整应用音量失败");}
                if(id==5&&p->master_)check(p->master_->SetMasterVolumeLevelScalar(pos/100.f,nullptr),"调整耳机音量失败");
                if(id>=100&&id<110){p->gains_[size_t(id-100)]=12-pos;SendMessageW(p->item(40),CB_SETCURSEL,-1,0);p->eqLabels();if(!p->dragging_)p->persist();}
                else if(!p->dragging_)p->syncVolumes();return 0;}
            if(msg==WM_COMMAND){int id=LOWORD(w);int notice=HIWORD(w);
                if(id==1&&notice==CBN_SELCHANGE){int i=p->selected(1);if(i>=0&&size_t(i)<p->devices_.size()){p->deviceId_=p->devices_[size_t(i)].id;p->sessions(true);p->persist();p->status(L"仅切换控制对象；未更改系统默认输出，EQ 需重新应用。");}}
                if(id==2&&notice==LBN_SELCHANGE)p->syncVolumes();
                if(id==4){int i=p->selected(2);if(i>=0&&size_t(i)<p->sessions_.size())check(p->sessions_[size_t(i)].volume->SetMute(SendMessageW(p->item(4),BM_GETCHECK,0,0)==BST_CHECKED,nullptr),"应用静音失败");}
                if(id==6&&p->master_)check(p->master_->SetMute(SendMessageW(p->item(6),BM_GETCHECK,0,0)==BST_CHECKED,nullptr),"耳机静音失败");
                if(id==10)p->devices();if(id==11)p->sessions();
                if(id==40&&notice==CBN_SELCHANGE)p->preset(p->selected(40));
                if(id==43)p->chooseConfig();if(id==44)p->applyEq(false);if(id==45)p->applyEq(true);if(id==46)p->detach();if(id==47)p->exportEq();
                if(id==48)MessageBoxW(h,L"1. 从 Equalizer APO 官网安装，并在设备选择器中只勾选目标耳机，按安装器要求重启。\n2. 在本面板选择相同耳机，选择 EqualizerAPO\\config\\config.txt。\n3. 调整 EQ 后点击应用；有写入权限才会成功。权限不足可导出后手动接入。\n4. 用旁路对比试听；程序不能确认驱动处理链是否生效。\n\n已有其他 EQ 会叠加。独占/ASIO/关闭音频增强可能绕过 APO。EQ 不依赖 LiteReplay 持续运行。详细说明在 docs/AUDIO_1_4.md。",L"耳机 EQ 接入",MB_OK);
                return 0;
            }
        }catch(const std::exception& e){p->dragging_=false;p->status(wide(e.what()));if(msg==WM_TIMER){KillTimer(h,1);p->master_.Reset();p->enableSession(false);EnableWindow(p->item(5),FALSE);EnableWindow(p->item(6),FALSE);}else MessageBoxW(h,wide(e.what()).c_str(),L"LiteReplay 音频",MB_ICONERROR);}return DefWindowProcW(h,msg,w,l);
    }
public:
    AudioPanel(const std::filesystem::path& settings,HFONT font):font_(font),settings_(settings){
        wchar_t value[32768];GetPrivateProfileStringW(L"AudioPanel",L"Device",L"",value,32768,settings_.c_str());deviceId_=value;
        GetPrivateProfileStringW(L"AudioPanel",L"APOConfig",L"",value,32768,settings_.c_str());apoConfig_=value;
        if(apoConfig_.empty()){DWORD bytes=sizeof value;if(RegGetValueW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\EqualizerAPO",L"ConfigPath",RRF_RT_REG_SZ,nullptr,value,&bytes)==ERROR_SUCCESS)apoConfig_=std::filesystem::path(value)/L"config.txt";}
        for(size_t i=0;i<10;++i)gains_[i]=std::clamp(int(GetPrivateProfileIntW(L"AudioPanel",(L"Band"+std::to_wstring(i)).c_str(),0,settings_.c_str())),-12,12);
    }
    ~AudioPanel(){if(window_){lr_ui::sections.erase(window_);DestroyWindow(window_);}}
    bool dialog(MSG* msg){return window_&&IsWindowVisible(window_)&&IsDialogMessageW(window_,msg);}
    bool showing()const{return window_&&IsWindowVisible(window_);}
    void hide(){if(window_)SendMessageW(window_,WM_CLOSE,0,0);}
    void refreshAppearance(){
        if(!window_)return;
        if(!(GetWindowLongPtrW(window_,GWL_STYLE)&WS_CHILD))lr_ui::caption(window_);
        lr_ui::apply(window_);lr_ui::redraw(window_);
    }
    void show(HWND parent=nullptr,int x=0,int y=0){
        if(!window_){WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);wc.lpszClassName=L"LiteReplayAudio14";RegisterClassW(&wc);
            window_=CreateWindowExW(WS_EX_CONTROLPARENT,wc.lpszClassName,L"LiteReplay 1.4 · 轻量音频面板",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,790,700,nullptr,nullptr,wc.hInstance,this);if(!window_)throw std::runtime_error("音频面板创建失败");
            auto add=[&](const wchar_t* cls,const wchar_t* title,DWORD style,int x,int y,int width,int height,int id){HWND c=CreateWindowExW(0,cls,title,WS_CHILD|WS_VISIBLE|style,x,y,width,height,window_,reinterpret_cast<HMENU>(INT_PTR(id)),wc.hInstance,nullptr);if(!c)throw std::runtime_error("音频控件创建失败");SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(font_),TRUE);return c;};
            add(L"STATIC",L"播放设备（控制目标，不会切换软件输出）",0,15,12,550,22,0);
            add(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,15,38,580,240,1);add(L"BUTTON",L"刷新设备",WS_TABSTOP,610,36,140,28,10);
            add(L"LISTBOX",L"",WS_BORDER|LBS_NOTIFY|WS_VSCROLL|WS_TABSTOP,15,80,420,160,2);
            add(L"STATIC",L"所选会话音量",0,450,80,300,22,7);auto session=add(TRACKBAR_CLASSW,L"",WS_TABSTOP,445,106,300,30,3);SendMessageW(session,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));
            add(L"BUTTON",L"静音所选会话",BS_AUTOCHECKBOX|WS_TABSTOP,450,140,290,24,4);add(L"BUTTON",L"刷新应用列表",WS_TABSTOP,450,177,290,28,11);
            add(L"STATIC",L"多个会话可能属于同一软件。",0,450,216,300,22,0);
            add(L"STATIC",L"耳机总音量",0,15,250,200,22,8);auto master=add(TRACKBAR_CLASSW,L"",WS_TABSTOP,210,247,350,30,5);SendMessageW(master,TBM_SETRANGE,TRUE,MAKELPARAM(0,100));add(L"BUTTON",L"耳机静音",BS_AUTOCHECKBOX|WS_TABSTOP,590,250,160,26,6);
            add(L"STATIC",L"耳机 10 段 EQ（需要 Equalizer APO）",0,15,291,420,22,0);auto combo=add(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_TABSTOP,450,286,300,200,40);
            for(auto s:{L"平直",L"低频增强",L"人声清晰",L"脚步参考（非游戏校准）",L"影音",L"柔和高频"})SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s));
            for(int i=0;i<10;++i){int x=20+i*73;add(L"STATIC",(std::to_wstring(eqFrequencies[size_t(i)])+L" Hz").c_str(),0,x,326,72,22,0);auto slider=add(TRACKBAR_CLASSW,L"",TBS_VERT|WS_TABSTOP,x+10,350,40,115,100+i);SendMessageW(slider,TBM_SETRANGE,TRUE,MAKELPARAM(0,24));add(L"STATIC",L"0 dB",0,x,470,70,22,120+i);}
            add(L"STATIC",L"",0,15,500,730,22,41);add(L"STATIC",apoConfig_.empty()?L"未找到 APO 配置；音量控制可直接使用，EQ 尚未接入。":apoConfig_.c_str(),SS_PATHELLIPSIS,15,529,730,22,42);
            const wchar_t* labels[]={L"选择 APO 配置",L"应用 EQ",L"旁路 EQ",L"移除接入",L"导出 EQ",L"接入说明"};for(int i=0;i<6;++i)add(L"BUTTON",labels[i],WS_TABSTOP,15+i*123,560,117,30,43+i);
            add(L"STATIC",L"音量改动立即生效；EQ 写入配置后仍需试听确认。",0,15,606,730,42,90);setGains();
            lr_ui::sections[window_]={{7,5,762,239},{7,242,762,284},{7,285,762,597}};
        }
        if(parent){SetParent(window_,parent);SetWindowLongPtrW(window_,GWL_STYLE,WS_CHILD|WS_CLIPCHILDREN);SetWindowPos(window_,nullptr,x,y,770,670,SWP_NOZORDER|SWP_FRAMECHANGED);}
        refreshAppearance();ShowWindow(window_,SW_SHOW);if(!parent)SetForegroundWindow(window_);devices();SetTimer(window_,1,1000,nullptr);
    }
};
}
