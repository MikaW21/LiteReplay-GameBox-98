#include "ui_theme.hpp"
#include "retro_extras.hpp"
#include "editor_panel.hpp"
#include "screenshot.hpp"
#include "engine.hpp"
#include "suite.hpp"
#include "media_library.hpp"
#include "profiles.hpp"
#include "performance.hpp"
#include "desktop_performance.hpp"
#include "extras.hpp"
#include "audio_panel.hpp"
#include "audio_devices.hpp"
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <psapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <fstream>
#include <map>
#include <cmath>
using namespace lr;
namespace {
constexpr UINT TrayMessage=WM_APP+1;
constexpr int Toggle=100,Save=101,Exit=102,Open=103,Apply=104,ShowPerformance=105,ShowExtras=106,ShowAudio=107,CompatPreset=108;
constexpr int Seconds=200,Size=201,Fps=202,Encoder=203,Bitrate=204,Monitor=205,Memory=206,
    System=207,Mic=208,Separate=209,Folder=210,Prefix=211,Container=212,Hotkey=213,Startup=214,Status=215,Metrics=216,Backend=217,AllowCpu=218,SystemVolume=219,MicVolume=220,ApplyVolumes=221,SystemDevice=222,MicDevice=223,RefreshDevices=224;
HWND window=nullptr;
RetroDesktop desktop;
std::unique_ptr<RetroExtras> retroExtras;
HWND pages[RetroDesktop::Count]{},homeStatus{},homeMetrics{},modeBox{},languageBox{};
HWND homeCpu{},homeGpu{},homeRam{},controlParent{},homeToggle{};
SuiteState suite;
int scrollPos[RetroDesktop::Count]{},scrollX[RetroDesktop::Count]{};
std::filesystem::path executableRoot;
std::map<std::string,std::wstring> language;
const wchar_t* tr(const char* key){auto i=language.find(key);return i==language.end()?L"[missing translation]":i->second.c_str();}
void loadLanguage(bool english){
    Catalog fallback,selected;
    std::ifstream en(executableRoot/L"languages"/L"en-US.lang");
    if(!en)throw std::runtime_error("Missing languages/en-US.lang beside LiteReplay.exe");
    fallback.read(en);
    if(!english){std::ifstream zh(executableRoot/L"languages"/L"zh-CN.lang");if(zh)selected.read(zh);}
    std::map<std::string,std::wstring> next;
    for(auto& entry:fallback.values())next[entry.first]=wide(entry.second);
    for(auto& entry:selected.values())next[entry.first]=wide(entry.second);
    language=std::move(next);
}
void layout();
void choosePage(int page);
void refreshShellLanguage();
void applyMode();
HFONT font=nullptr;
std::unique_ptr<Engine> engine;
std::map<int,HWND> controls;
std::map<HWND,std::string> replayLabels;
void updateReplayLabels(){for(auto& entry:replayLabels)SetWindowTextW(entry.first,tr(entry.second.c_str()));}
std::filesystem::path settings,logPath;
std::mutex logMutex;
Config config;std::vector<RecordingDevice> systemChoices,micChoices;
NOTIFYICONDATAW tray{};
HICON retroIcon=nullptr;
UINT taskbarCreated=0;
std::wstring observedSave;
std::mutex savedEventsMutex;
std::deque<std::wstring> savedEvents;
std::unique_ptr<MediaLibrary> mediaLibrary;
std::unique_ptr<ScreenshotTool> screenshots;
WORD quickShot=MAKEWORD(VK_F11,HOTKEYF_CONTROL|HOTKEYF_SHIFT),regionShot=MAKEWORD(VK_F8,HOTKEYF_CONTROL|HOTKEYF_SHIFT);
UINT shotModifiers(WORD h){auto f=HIBYTE(h);return ((f&HOTKEYF_CONTROL)?MOD_CONTROL:0)|((f&HOTKEYF_SHIFT)?MOD_SHIFT:0)|((f&HOTKEYF_ALT)?MOD_ALT:0);}
bool bindShot(int id,WORD h){return LOBYTE(h)&&shotModifiers(h)&&RegisterHotKey(window,id,shotModifiers(h)|MOD_NOREPEAT,LOBYTE(h));}

void logLine(const std::string& text) {
    std::lock_guard<std::mutex> l(logMutex);
    try {
        std::error_code ec;
        if(std::filesystem::file_size(logPath,ec)>1048576 && !ec) {
            auto old=logPath;old+=L".1";std::filesystem::remove(old,ec);std::filesystem::rename(logPath,old,ec);
        }
        std::ofstream file(logPath,std::ios::app);
        SYSTEMTIME t;GetLocalTime(&t);
        file<<t.wYear<<'-'<<t.wMonth<<'-'<<t.wDay<<' '<<t.wHour<<':'<<t.wMinute<<':'<<t.wSecond<<' '<<text<<'\n';
    }catch(...){}
}
std::wstring text(int id) {
    int n=GetWindowTextLengthW(controls.at(id));std::wstring s(size_t(n)+1,L'\0');
    GetWindowTextW(controls.at(id),s.data(),n+1);s.resize(n);return s;
}
void set(int id,const std::wstring& s){SetWindowTextW(controls.at(id),s.c_str());}
HWND control(const wchar_t* type,const wchar_t* title,DWORD style,int x,int y,int w,int h,int id) {
    HWND item=CreateWindowExW(type==std::wstring(L"EDIT")?WS_EX_CLIENTEDGE:0,type,title,
        WS_CHILD|WS_VISIBLE|style,x,y,w,h,controlParent?controlParent:window,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
    if(!item)throw std::runtime_error("创建界面控件失败");
    for(auto& entry:language)if(entry.second==title){replayLabels[item]=entry.first;break;}
    SendMessageW(item,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);if(id)controls[id]=item;return item;
}
void label(const wchar_t* s,int x,int y,int w=180){control(L"STATIC",s,0,x,y,w,22,0);}
void edit(int id,const std::wstring& s,int x,int y,int w=140){control(L"EDIT",s.c_str(),WS_TABSTOP|ES_AUTOHSCROLL,x,y,w,25,id);}
void combo(int id,const std::vector<std::wstring>& items,int selected,int x,int y,int w=140) {
    auto h=control(WC_COMBOBOXW,L"",WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,x,y,w,240,id);
    for(auto& s:items)SendMessageW(h,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s.c_str()));
    SendMessageW(h,CB_SETCURSEL,selected,0);
}
int selection(int id){return int(SendMessageW(controls.at(id),CB_GETCURSEL,0,0));}
void checkbox(int id,const wchar_t* s,bool checked,int x,int y,int w=280) {
    auto h=control(L"BUTTON",s,WS_TABSTOP|BS_AUTOCHECKBOX,x,y,w,24,id);
    SendMessageW(h,BM_SETCHECK,checked?BST_CHECKED:BST_UNCHECKED,0);
}
bool checked(int id){return SendMessageW(controls.at(id),BM_GETCHECK,0,0)==BST_CHECKED;}
int number(int id,int lo,int hi) {
    auto s=text(id);size_t pos=0;int n;
    try{n=std::stoi(s,&pos);}catch(...){throw std::runtime_error("请输入有效的整数设置");}
    if(pos!=s.size()||n<lo||n>hi)throw std::runtime_error("设置超出允许范围");return n;
}
int ini(const wchar_t* key,int fallback,int lo,int hi){return std::clamp(int(GetPrivateProfileIntW(L"Replay",key,fallback,settings.c_str())),lo,hi);}
std::wstring iniString(const wchar_t* key,const std::wstring& fallback) {
    wchar_t value[32768];GetPrivateProfileStringW(L"Replay",key,fallback.c_str(),value,32768,settings.c_str());return value;
}
void load() {
    PWSTR p=nullptr;check(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&p),"LocalAppData");
    auto root=std::filesystem::path(p)/L"LiteReplay";CoTaskMemFree(p);std::filesystem::create_directories(root);
    auto legacy=root/L"settings.ini";
    root/=L"2.0";std::filesystem::create_directories(root);
    settings=root/L"settings.ini";logPath=root/L"LiteReplay.log";
    if(!std::filesystem::exists(settings)&&std::filesystem::exists(legacy)){
        auto backup=root/L"settings-1.4-backup.ini";
        if(!std::filesystem::exists(backup))std::filesystem::copy_file(legacy,backup);
        auto temp=root/L"migration.tmp";
        std::filesystem::copy_file(legacy,temp,std::filesystem::copy_options::overwrite_existing);
        if(!MoveFileExW(temp.c_str(),settings.c_str(),MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Settings migration failed");
        // A new version never silently registers itself for startup.
        WritePrivateProfileStringW(L"Replay",L"Startup",L"0",settings.c_str());
    }
    wchar_t exe[32768];GetModuleFileNameW(nullptr,exe,32768);executableRoot=std::filesystem::path(exe).parent_path();
    suite.mode=parseMode(int(GetPrivateProfileIntW(L"Suite",L"Mode",0,settings.c_str())));
    suite.english=GetPrivateProfileIntW(L"Suite",L"English",0,settings.c_str())!=0;

    loadLanguage(suite.english);
    quickShot=WORD(GetPrivateProfileIntW(L"Screenshots",L"Quick",quickShot,settings.c_str()));
    regionShot=WORD(GetPrivateProfileIntW(L"Screenshots",L"Region",regionShot,settings.c_str()));
    if(!std::filesystem::exists(settings)){
        std::ofstream initial(settings,std::ios::binary);
        initial.write("\xff\xfe",2);
        if(!initial)throw std::runtime_error("无法初始化设置文件");
    }
    // A missing/redirected Videos folder must not prevent the GUI from opening.
    // Saved user settings take precedence, without querying Videos at all.
    config.folder=iniString(L"Folder",L"");
    if(config.folder.empty()){
        PWSTR videos=nullptr;
        const HRESULT hr=SHGetKnownFolderPath(FOLDERID_Videos,0,nullptr,&videos);
        if(SUCCEEDED(hr)&&videos&&*videos){
            config.folder=(std::filesystem::path(videos)/L"LiteReplay").wstring();
        }else{
            config.folder=(root/L"Replays").wstring();
            std::ostringstream message;
            message<<"Videos folder unavailable (0x"<<std::hex<<uint32_t(hr)
                   <<"); using "<<utf8(config.folder);
            logLine(message.str());
        }
        CoTaskMemFree(videos);
    }
    auto backend=iniString(L"Backend",L"auto");
    if(backend==L"nvenc"||backend==L"amf"||backend==L"mf_hw"||backend==L"mf_sw")config.backend=utf8(backend);
    config.allowSoftware=ini(L"AllowSoftware",0,0,1)!=0;
    config.prefix=iniString(L"Prefix",L"LiteReplay");
    config.seconds=ini(L"Seconds",60,30,600);
    config.fps=frameRates[fpsIndex(ini(L"FPS",60,1,1000))];
    auto resolution=resolutions[resolutionIndex(ini(L"Width",1920,1,16384))];
    config.width=resolution.width;config.height=resolution.height;
    config.mbps=ini(L"Mbps",20,2,100);config.monitor=ini(L"Monitor",0,0,15);
    config.memoryMiB=ini(L"MemoryMiB",512,128,4096);
    config.systemAudio=ini(L"SystemAudio",1,0,1)!=0;config.microphone=ini(L"Microphone",0,0,1)!=0;
    config.systemDeviceId=iniString(L"SystemDeviceId",L"");config.micDeviceId=iniString(L"MicDeviceId",L"");
    config.systemVolume=ini(L"SystemVolume",100,0,200);config.micVolume=ini(L"MicVolume",100,0,200);
    config.separateMic=ini(L"SeparateMic",0,0,1)!=0;config.startup=ini(L"Startup",0,0,1)!=0;
    config.hotkey=ini(L"Hotkey",VK_F10,1,254);config.modifiers=ini(L"Modifiers",MOD_CONTROL|MOD_SHIFT,0,7);
    auto codec=iniString(L"Codec",L"h264_nvenc");
    if(codec==L"hevc_nvenc"||codec==L"av1_nvenc")config.codec=utf8(codec);
    config.container=iniString(L"Container",L"mp4")==L"mkv"?"mkv":"mp4";
}
void autostart(bool on) {
    HKEY key;LONG rc=RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr);
    if(rc!=ERROR_SUCCESS)throw std::runtime_error("无法修改当前用户的开机启动设置");
    if(on){wchar_t exe[32768];DWORD n=GetModuleFileNameW(nullptr,exe,32768);
        std::wstring value=L"\""+std::wstring(exe,n)+L"\" --tray --start";
        rc=RegSetValueExW(key,L"LiteReplay",0,REG_SZ,reinterpret_cast<const BYTE*>(value.c_str()),DWORD((value.size()+1)*sizeof(wchar_t)));
    }else {rc=RegDeleteValueW(key,L"LiteReplay");if(rc==ERROR_FILE_NOT_FOUND)rc=ERROR_SUCCESS;}
    RegCloseKey(key);if(rc!=ERROR_SUCCESS)throw std::runtime_error("开机启动设置失败");
}
void persist() {
    auto put=[&](const wchar_t* key,const std::wstring& value){if(!WritePrivateProfileStringW(L"Replay",key,value.c_str(),settings.c_str()))throw std::runtime_error("保存设置失败");};
    put(L"Backend",wide(config.backend));put(L"AllowSoftware",std::to_wstring(config.allowSoftware));
    put(L"Folder",config.folder);put(L"Prefix",config.prefix);put(L"Codec",wide(config.codec));put(L"Container",wide(config.container));
    put(L"Seconds",std::to_wstring(config.seconds));put(L"FPS",std::to_wstring(config.fps));put(L"Width",std::to_wstring(config.width));
    put(L"Mbps",std::to_wstring(config.mbps));put(L"Monitor",std::to_wstring(config.monitor));put(L"MemoryMiB",std::to_wstring(config.memoryMiB));
    put(L"SystemAudio",std::to_wstring(config.systemAudio));put(L"Microphone",std::to_wstring(config.microphone));
    put(L"SystemDeviceId",config.systemDeviceId);put(L"MicDeviceId",config.micDeviceId);
    put(L"SystemVolume",std::to_wstring(config.systemVolume));put(L"MicVolume",std::to_wstring(config.micVolume));
    put(L"SeparateMic",std::to_wstring(config.separateMic));put(L"Startup",std::to_wstring(config.startup));
    put(L"Hotkey",std::to_wstring(config.hotkey));put(L"Modifiers",std::to_wstring(config.modifiers));
}
std::wstring selectedRecordingDevice(int id,const std::vector<RecordingDevice>& choices){int i=selection(id);if(i<0||i>=int(choices.size()))throw std::runtime_error("Select an audio device / 请选择音频设备");return choices[size_t(i)].id;}
void refreshAudioChoices(){
 auto fill=[&](int id,bool mic,std::vector<RecordingDevice>& choices,const std::wstring& saved){std::wstring selected=saved;int previous=selection(id);if(previous>=0&&previous<int(choices.size()))selected=choices[size_t(previous)].id;auto available=recordingDevices(mic);std::vector<RecordingDevice> next{{L"",tr("audio.device.default")}};next.insert(next.end(),available.begin(),available.end());if(!selected.empty()&&std::none_of(next.begin(),next.end(),[&](const auto& d){return d.id==selected;}))next.push_back({selected,tr("audio.device.missing")});SendMessageW(controls.at(id),CB_RESETCONTENT,0,0);int chosen=0;for(size_t i=0;i<next.size();++i){SendMessageW(controls.at(id),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(next[i].name.c_str()));if(next[i].id==selected)chosen=int(i);}choices=std::move(next);SendMessageW(controls.at(id),CB_SETCURSEL,chosen,0);};
 fill(SystemDevice,false,systemChoices,config.systemDeviceId);fill(MicDevice,true,micChoices,config.micDeviceId);
}
void apply() {
    Config next=config;next.systemVolume=number(SystemVolume,0,200);next.micVolume=number(MicVolume,0,200);next.seconds=number(Seconds,30,600);next.mbps=number(Bitrate,2,100);
    next.monitor=number(Monitor,0,15);next.memoryMiB=number(Memory,128,4096);
    const auto resolution=resolutions.at(size_t(selection(Size)));
    next.width=resolution.width;next.height=resolution.height;next.fps=frameRates.at(size_t(selection(Fps)));
    const char* codecs[]={"h264_nvenc","hevc_nvenc","av1_nvenc"};next.codec=codecs[selection(Encoder)];
    const char* backends[]={"auto","nvenc","amf","mf_hw","mf_sw"};
    next.backend=backends[std::clamp(selection(Backend),0,4)];next.allowSoftware=checked(AllowCpu);
    if((next.backend=="mf_sw"||next.backend=="mf_hw")&&selection(Encoder)!=0)throw std::runtime_error("Windows MF compatibility mode currently requires H.264 / MF 兼容模式请选择 H.264");
    next.container=selection(Container)?"mkv":"mp4";next.systemAudio=checked(System);next.microphone=checked(Mic);
    next.systemDeviceId=selectedRecordingDevice(SystemDevice,systemChoices);next.micDeviceId=selectedRecordingDevice(MicDevice,micChoices);
    if(next.systemAudio)validateRecordingDevice(false,next.systemDeviceId);if(next.microphone)validateRecordingDevice(true,next.micDeviceId);
    next.separateMic=checked(Separate);next.startup=checked(Startup);next.folder=text(Folder);next.prefix=text(Prefix);
    if(next.folder.empty()||!std::filesystem::path(next.folder).is_absolute())throw std::runtime_error("保存目录必须是完整路径，例如 G:\\LiteReplay\\Replays");
    if(next.prefix.empty()||next.prefix.size()>64||next.prefix.find_first_of(L"<>:\"/\\|?*")!=std::wstring::npos ||
        std::any_of(next.prefix.begin(),next.prefix.end(),[](wchar_t c){return c<32;}))throw std::runtime_error("文件名前缀无效：请输入 1–64 个字符，不能包含路径或特殊符号");
    WORD hot=WORD(SendMessageW(controls.at(Hotkey),HKM_GETHOTKEY,0,0));next.hotkey=LOBYTE(hot);
    BYTE flags=HIBYTE(hot);next.modifiers=((flags&HOTKEYF_CONTROL)?MOD_CONTROL:0)|((flags&HOTKEYF_SHIFT)?MOD_SHIFT:0)|((flags&HOTKEYF_ALT)?MOD_ALT:0);
    if((next.hotkey==VK_F9||next.hotkey==VK_F7)&&next.modifiers==(MOD_CONTROL|MOD_SHIFT))throw std::runtime_error("Ctrl+Shift+F7 / F9 已用于覆盖层，请选择其他保存热键");
    if((next.hotkey==LOBYTE(quickShot)&&next.modifiers==shotModifiers(quickShot))||(next.hotkey==LOBYTE(regionShot)&&next.modifiers==shotModifiers(regionShot)))throw std::runtime_error("Hotkey is used for screenshots / 快捷键已用于截图");
    if(!next.hotkey||!next.modifiers)throw std::runtime_error("快捷键需要 Ctrl、Shift 或 Alt 加一个按键");
    UnregisterHotKey(window,1);
    if(!RegisterHotKey(window,1,next.modifiers|MOD_NOREPEAT,next.hotkey)) {
        RegisterHotKey(window,1,config.modifiers|MOD_NOREPEAT,config.hotkey);
        throw std::runtime_error("快捷键已被其他程序占用，请更换");
    }
    try{if(next.startup!=config.startup)autostart(next.startup);}catch(...){
        UnregisterHotKey(window,1);RegisterHotKey(window,1,config.modifiers|MOD_NOREPEAT,config.hotkey);throw;
    }
    config=next;persist();if(engine)engine->audioVolumes(config.systemVolume,config.micVolume);
}
std::unique_ptr<Performance> perf;
std::unique_ptr<DesktopPerformance> desktopPerformance;
std::unique_ptr<Extras> extras;
std::unique_ptr<AudioPanel> audioPanel;
void addTray(){tray={};tray.cbSize=sizeof tray;tray.hWnd=window;tray.uID=1;tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;
    tray.uCallbackMessage=TrayMessage;tray.hIcon=retroIcon?retroIcon:LoadIconW(nullptr,IDI_APPLICATION);wcscpy_s(tray.szTip,tr("replay.000"));Shell_NotifyIconW(NIM_ADD,&tray);}
void notify(const std::wstring& message){tray.uFlags=NIF_INFO;wcscpy_s(tray.szInfoTitle,L"LiteReplay");wcsncpy_s(tray.szInfo,message.c_str(),_TRUNCATE);tray.dwInfoFlags=NIIF_INFO|NIIF_NOSOUND;Shell_NotifyIconW(NIM_MODIFY,&tray);tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;}
void save(){if(!engine->save())notify(engine->saving()?tr("replay.001"):tr("replay.002"));}
void refresh() {
    const bool mainVisible=IsWindowVisible(window)&&!IsIconic(window);
    const bool monitorVisible=mainVisible&&desktop.visible(5);
    const bool dashboardVisible=mainVisible&&desktop.visible(0);
    const bool overlayVisible=desktopPerformance&&desktopPerformance->visible();
    const bool capture=desktopPerformance&&desktopPerformance->capturing();
    const bool subscribed=overlayVisible||dashboardVisible||monitorVisible||extras->visible();
    if(subscribed&&!perf)perf=std::make_unique<Performance>();
    if(!subscribed)perf.reset();
    if(perf){
        perf->intervalMs=policy(suite.mode).monitorMs;
        perf->extended=monitorVisible||overlayVisible;
        perf->targetPid=desktopPerformance&&(monitorVisible||overlayVisible)?desktopPerformance->target():0;
    }
    if(dashboardVisible||monitorVisible||(mainVisible&&(desktop.visible(1)||engine->running()))||extras->visible()||overlayVisible||capture)
        SetTimer(window,1,capture?std::min(250,policy(suite.mode).uiMs):policy(suite.mode).uiMs,nullptr);
    else KillTimer(window,1);
    bool active=engine->running();if(homeToggle)SetWindowTextW(homeToggle,active?tr("replay.003"):tr("replay.004"));EnableWindow(GetDlgItem(pages[0],Save),active);set(Toggle,active?tr("replay.003"):tr("replay.004"));
    for(auto [id,h]:controls)if(id>=Seconds&&id<=Startup)EnableWindow(h,!active);
    for(int id:{Backend,AllowCpu,CompatPreset,SystemDevice,MicDevice,RefreshDevices})EnableWindow(controls.at(id),!active);
    EnableWindow(controls.at(Apply),!active);set(Status,suite.english?std::wstring(engine->running()?tr("running"):tr("stopped"))+L"\r\n"+wide(engine->statusText()):wide(engine->statusText()));set(Metrics,L"");
    auto replayStatus=wide(engine->statusText());
    const auto lastName=std::filesystem::path(engine->lastPath()).filename().wstring();
    if(!lastName.empty())replayStatus+=L"\r\n"+lastName;
    SetWindowTextW(homeStatus,replayStatus.c_str());
    auto sample=perf?perf->snapshot():PerformanceSnapshot{};
    if(dashboardVisible){
        auto val=[](double n){return n<0?std::wstring(L"N/A"):std::to_wstring(int(n+0.5))+L"%";};
        std::wstring line=L"CPU "+val(sample.systemCpu)+L"   GPU "+val(sample.systemGpu)+L"   RAM "+(sample.ramValid?std::to_wstring(sample.usedRam/1048576)+L" MiB":L"N/A");
        line+=L"\r\nLiteReplay CPU "+val(sample.processCpu)+L"   Private "+(sample.processRamValid?std::to_wstring(sample.privateRam/1048576)+L" MiB":L"N/A");
        SetWindowTextW(homeCpu,val(sample.systemCpu).c_str());SetWindowTextW(homeGpu,val(sample.systemGpu).c_str());SetWindowTextW(homeRam,(sample.ramValid?std::to_wstring(sample.usedRam/1048576)+L" MiB":L"N/A").c_str());
        SetWindowTextW(homeMetrics,line.substr(line.find(L"\r\n")+2).c_str());
    }
    extras->update(sample,active);
    if(desktopPerformance)desktopPerformance->update(sample,monitorVisible||overlayVisible);
    desktop.indicators(active,overlayVisible);
    if(monitorVisible)SetDlgItemTextW(pages[5],1194,
        ((suite.english?L"Replay encoded / missed / restarts: ":L"回放已编码帧 / 漏采时间格 / 重建次数：")+
        std::to_wstring(engine->frames.load())+L" / "+std::to_wstring(engine->skipped.load())+
        L" / "+std::to_wstring(engine->restarts.load())).c_str());
    auto path=engine->lastPath();if(!path.empty()&&path!=observedSave){observedSave=path;notify(tr("replay.005")+std::filesystem::path(path).filename().wstring());}
}
void showMonitor(){ShowWindow(window,SW_RESTORE);SetForegroundWindow(window);choosePage(5);}
void buildReplayPage() {
    NONCLIENTMETRICSW metrics{};metrics.cbSize=sizeof metrics;SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof metrics,&metrics,0);
    wcscpy_s(metrics.lfMessageFont.lfFaceName,L"Microsoft YaHei UI");
    if(!font)font=CreateFontIndirectW(&metrics.lfMessageFont);
    label(L"LiteReplay 2.0 · Compatibility",20,15,390);
    control(L"BUTTON",tr("replay.006"),WS_TABSTOP,430,10,190,30,ShowPerformance);
    label(tr("replay.007"),20,55);edit(Seconds,std::to_wstring(config.seconds),220,52);
    label(tr("replay.008"),20,92);
    std::vector<std::wstring> sizes,rates;
    for(const auto& r:resolutions)sizes.emplace_back(r.label);
    for(int rate:frameRates)rates.push_back(std::to_wstring(rate)+L" FPS");
    combo(Size,sizes,int(resolutionIndex(config.width)),220,89,240);
    combo(Fps,rates,int(fpsIndex(config.fps)),480,89,140);
    label(tr("replay.009"),20,129);combo(Encoder,{L"H.264",L"HEVC",L"AV1"},config.codec=="av1_nvenc"?2:config.codec=="hevc_nvenc"?1:0,220,126);
    combo(Container,{L"MP4",L"MKV"},config.container=="mkv",380,126);
    label(tr("replay.010"),20,166);edit(Bitrate,std::to_wstring(config.mbps),220,163);
    label(tr("replay.011"),20,203);edit(Monitor,std::to_wstring(config.monitor),220,200);
    label(tr("replay.012"),20,240);edit(Memory,std::to_wstring(config.memoryMiB),220,237);
    label(tr("replay.013"),20,275,600);
    checkbox(System,tr("replay.014"),config.systemAudio,20,305,330);edit(SystemVolume,std::to_wstring(config.systemVolume),435,305,70);label(tr("audio.record.percent"),515,307,100);
    checkbox(Mic,tr("replay.015"),config.microphone,20,335,330);edit(MicVolume,std::to_wstring(config.micVolume),435,335,70);label(tr("audio.record.percent"),515,337,100);
    checkbox(Separate,tr("replay.016"),config.separateMic,20,365,330);control(L"BUTTON",tr("audio.record.apply"),WS_TABSTOP,430,365,190,28,ApplyVolumes);
    label(tr("replay.017"),20,405,600);edit(Folder,config.folder,20,430,600);
    label(tr("replay.018"),20,470);edit(Prefix,config.prefix,220,467,220);
    label(tr("replay.019"),20,507);auto hot=control(HOTKEY_CLASSW,L"",WS_TABSTOP,220,504,220,25,Hotkey);
    WORD mods=WORD(((config.modifiers&MOD_CONTROL)?HOTKEYF_CONTROL:0)|((config.modifiers&MOD_SHIFT)?HOTKEYF_SHIFT:0)|((config.modifiers&MOD_ALT)?HOTKEYF_ALT:0));
    SendMessageW(hot,HKM_SETHOTKEY,MAKEWORD(config.hotkey,mods),0);
    checkbox(Startup,tr("replay.020"),config.startup,20,540,450);
    control(L"BUTTON",tr("replay.004"),WS_TABSTOP,20,580,140,32,Toggle);
    control(L"BUTTON",tr("replay.021"),WS_TABSTOP,170,580,130,32,Save);
    control(L"BUTTON",tr("replay.022"),WS_TABSTOP,310,580,140,32,Apply);
    control(L"BUTTON",tr("replay.023"),WS_TABSTOP,460,580,140,32,Open);
    control(L"STATIC",tr("replay.024"),0,20,630,600,74,Status);
    control(L"STATIC",L"",0,20,711,600,45,Metrics);
    control(L"BUTTON",tr("replay.025"),WS_TABSTOP,20,770,310,30,ShowExtras);
    control(L"BUTTON",tr("replay.026"),WS_TABSTOP,345,770,275,30,ShowAudio);
    label(tr("compat.backend"),20,820,190);
    combo(Backend,{tr("compat.auto"),L"NVIDIA NVENC",L"AMD AMF",tr("compat.mf_hw"),tr("compat.mf_sw")},config.backend=="nvenc"?1:config.backend=="amf"?2:config.backend=="mf_hw"?3:config.backend=="mf_sw"?4:0,220,817,400);
    checkbox(AllowCpu,tr("compat.fallback"),config.allowSoftware,20,855,600);
    control(L"BUTTON",tr("compat.preset"),WS_TABSTOP,20,895,600,32,CompatPreset);
    label(tr("compat.note"),20,940,600);
    // Make room beneath recording levels, preserving every existing replay control.
    EnumChildWindows(controlParent,[](HWND child,LPARAM parent)->BOOL{RECT rc;GetWindowRect(child,&rc);POINT pos{rc.left,rc.top};ScreenToClient(reinterpret_cast<HWND>(parent),&pos);if(pos.y>=405)SetWindowPos(child,nullptr,pos.x,pos.y+180,0,0,SWP_NOSIZE|SWP_NOZORDER);return TRUE;},reinterpret_cast<LPARAM>(controlParent));
    label(tr("audio.device.system"),20,405,450);combo(SystemDevice,{tr("audio.device.default")},0,20,432,440);
    label(tr("audio.device.mic"),20,472,450);combo(MicDevice,{tr("audio.device.default")},0,20,498,440);
    control(L"BUTTON",tr("audio.device.refresh"),WS_TABSTOP,478,432,142,32,RefreshDevices);label(tr("audio.device.note"),20,540,600);
    try{refreshAudioChoices();}catch(const std::exception& e){logLine(e.what());systemChoices={{config.systemDeviceId,tr("audio.device.missing")}};micChoices={{config.micDeviceId,tr("audio.device.missing")}};for(int id:{SystemDevice,MicDevice}){SendMessageW(controls.at(id),CB_RESETCONTENT,0,0);SendMessageW(controls.at(id),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(tr("audio.device.missing")));SendMessageW(controls.at(id),CB_SETCURSEL,0,0);}}
    BOOL dark=TRUE;DwmSetWindowAttribute(window,20,&dark,sizeof dark);
}

const char* navKeys[]={"retro.dashboard","replay","editor","retro.library","audio","performance","retro.control","retro.about","retro.dos"};
LRESULT CALLBACK pageProc(HWND h,UINT msg,WPARAM w,LPARAM l){
    if(h==pages[3]&&mediaLibrary){try{if(mediaLibrary->debounce(msg,w)||mediaLibrary->message(msg,w,l))return 0;}catch(const std::exception& e){logLine(e.what());MessageBoxW(h,wide(e.what()).c_str(),L"LiteReplay Media",MB_ICONERROR);return 0;}}
    if(msg==WM_ERASEBKGND){lr_ui::paint(h,reinterpret_cast<HDC>(w));return 1;}
    if(msg==WM_COMMAND)return SendMessageW(window,msg,w,l);
    if(h==pages[8]&&retroExtras){
        if(auto brush=retroExtras->terminalColor(msg,w,l))return reinterpret_cast<LRESULT>(brush);
    }
    if(msg==WM_CTLCOLORSTATIC||msg==WM_CTLCOLOREDIT||msg==WM_CTLCOLORLISTBOX||msg==WM_CTLCOLORBTN)return lr_ui::color(h,msg,w,l);
    if(msg==WM_MOUSEWHEEL||msg==WM_VSCROLL||msg==WM_HSCROLL){
        int page=0;while(page<RetroDesktop::Count&&pages[page]!=h)++page;
        if(page==RetroDesktop::Count)return 0;
        bool horizontal=msg==WM_HSCROLL;int bar=horizontal?SB_HORZ:SB_VERT;
        SCROLLINFO si{sizeof si,SIF_ALL};GetScrollInfo(h,bar,&si);int next=si.nPos;
        if(msg==WM_MOUSEWHEEL)next-=GET_WHEEL_DELTA_WPARAM(w)/WHEEL_DELTA*60;
        else switch(LOWORD(w)){case SB_LINEUP:next-=30;break;case SB_LINEDOWN:next+=30;break;case SB_PAGEUP:next-=int(si.nPage);break;case SB_PAGEDOWN:next+=int(si.nPage);break;case SB_THUMBTRACK:next=si.nTrackPos;break;}
        next=std::clamp(next,0,std::max(0,si.nMax-int(si.nPage)+1));
        int delta=si.nPos-next;si.fMask=SIF_POS;si.nPos=next;SetScrollInfo(h,bar,&si,TRUE);if(horizontal)scrollX[page]=next;else scrollPos[page]=next;
        ScrollWindowEx(h,horizontal?delta:0,horizontal?0:delta,nullptr,nullptr,nullptr,nullptr,SW_SCROLLCHILDREN|SW_INVALIDATE|SW_ERASE);return 0;
    }
    return DefWindowProcW(h,msg,w,l);
}
void layoutPage(int page,int width,int height){
    HWND body=pages[page];
    if(!body)return;
    constexpr int maxW[]={0,640,940,710,790,813,630,482,646};
    constexpr int maxH[]={600,1170,624,454,680,801,725,375,390};
    auto syncScroll=[&](int bar,int maximum,int extent,int& offset){
        SCROLLINFO info{sizeof info,SIF_RANGE|SIF_PAGE|SIF_POS,0,maximum,UINT(std::max(1,extent)),offset};
        SetScrollInfo(body,bar,&info,TRUE);
        const int current=GetScrollPos(body,bar);
        if(current!=offset){
            ScrollWindowEx(body,bar==SB_HORZ?offset-current:0,bar==SB_VERT?offset-current:0,
                nullptr,nullptr,nullptr,nullptr,SW_SCROLLCHILDREN|SW_INVALIDATE|SW_ERASE);
            offset=current;
        }
    };
    syncScroll(SB_HORZ,maxW[page],width,scrollX[page]);
    syncScroll(SB_VERT,maxH[page],height,scrollPos[page]);
    if(page==2&&lr_editor::mainWindow)
        MoveWindow(lr_editor::mainWindow,-scrollX[2],-scrollPos[2],std::max(940,width-18),std::max(624,height-18),TRUE);
    if(page==3&&mediaLibrary)
        mediaLibrary->layout(std::max(710,width-18),std::max(454,height-18),scrollX[3],scrollPos[3]);
    if(page==0&&homeStatus){
        const int content=std::max(294,width-30);
        lr_ui::sections[body]={{16,14,content,199},{16,213,content,367},{16,381,content,490}};
        auto place=[&](HWND h,int x,int y,int w,int hgt){MoveWindow(h,x,y-scrollPos[0],w,hgt,TRUE);};
        place(GetDlgItem(body,920),34,35,content-55,26);
        place(homeStatus,34,69,content-55,58);
        const int actionWidth=(content-68)/3;
        const int actions[]={Toggle,Save,801};
        for(int i=0;i<3;++i)place(GetDlgItem(body,actions[i]),34+i*(actionWidth+9),138,actionWidth,40);
        place(GetDlgItem(body,921),34,233,content-55,24);
        place(homeMetrics,34,338,content-55,22);
        int column=(content-55)/3;
        HWND values[]={homeCpu,homeGpu,homeRam};
        for(int i=0;i<3;++i){
            place(GetDlgItem(body,924+i),34+i*column,266,column-12,22);
            place(values[i],34+i*column,289,column-12,38);
        }
        place(GetDlgItem(body,922),34,397,content-55,24);
        const int toolWidth=(content-62)/2;
        place(GetDlgItem(body,802),34,435,toolWidth,34);
        place(GetDlgItem(body,803),44+toolWidth,435,toolWidth,34);
        place(GetDlgItem(body,916),34,512,content-55,24);
    }
    InvalidateRect(body,nullptr,TRUE);
}
void layout(){desktop.layout();}
HWND simple(HWND parent,const wchar_t* cls,const wchar_t* title,int id,int x,int y,int w,int h,DWORD style=0){
    HWND c=CreateWindowExW(0,cls,title,WS_CHILD|WS_VISIBLE|style,x,y,w,h,parent,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
    if(!c)throw std::runtime_error("Cannot create shell control");SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);return c;
}
void refreshShellLanguage(){
    lr_editor::localize(suite.english);
    if(desktopPerformance)desktopPerformance->localize(suite.english);
    std::array<std::wstring,RetroDesktop::Count> names;
    for(int i=0;i<RetroDesktop::Count;++i)names[i]=tr(navKeys[i]);
    desktop.localize(suite.english,names);
    if(retroExtras)retroExtras->localize(suite.english);
    for(int i:{0,2,3,5,6})SetDlgItemTextW(pages[i],990,tr(navKeys[i]));
    SetDlgItemTextW(pages[0],991,tr("welcome"));
    SetDlgItemTextW(pages[0],Save,tr("save"));SetDlgItemTextW(pages[0],801,tr("replay"));
    SetDlgItemTextW(pages[2],991,tr("editor_pending"));if(mediaLibrary)mediaLibrary->localize();
    SetDlgItemTextW(pages[6],992,tr("mode"));SetDlgItemTextW(pages[6],993,tr("language"));SetDlgItemTextW(pages[6],994,tr("policy_note"));
    SetDlgItemTextW(pages[6],1980,tr("retro.links"));
    for(int i:{1,4,5})SetDlgItemTextW(pages[6],800+i,tr(navKeys[i]));
    SendMessageW(modeBox,CB_RESETCONTENT,0,0);for(auto key:{"balanced","low","fast"})SendMessageW(modeBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(tr(key)));
    SendMessageW(modeBox,CB_SETCURSEL,int(suite.mode),0);
    // Replay controls retain unsaved values. Only their labels are translated below.
    for(auto item:{std::pair<int,const char*>{913,"shot.quick"},{914,"shot.region"},{912,"shot.apply"},{915,"shot.note"},{916,"credits"}})SetDlgItemTextW(pages[6],item.first,tr(item.second));
    SetDlgItemTextW(pages[0],916,tr("credits"));

    for(auto entry:{std::pair<int,const char*>{920,"ui.record"},{921,"ui.hardware"},{922,"ui.workspace"},{923,"ui.start"},{802,"editor"},{803,"library"}})SetDlgItemTextW(pages[0],entry.first,tr(entry.second));
    updateReplayLabels();
    if(controls.count(Backend)){int selected=selection(Backend);auto h=controls.at(Backend);SendMessageW(h,CB_RESETCONTENT,0,0);for(auto value:{tr("compat.auto"),L"NVIDIA NVENC",L"AMD AMF",tr("compat.mf_hw"),tr("compat.mf_sw")})SendMessageW(h,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(value));SendMessageW(h,CB_SETCURSEL,selected,0);}
}
void refreshAppearance(){
    lr_ui::caption(window);lr_ui::apply(window);layout();lr_ui::redraw(window);
    if(desktopPerformance)desktopPerformance->refreshAppearance();
    if(audioPanel)audioPanel->refreshAppearance();
}
void applyMode(){
    auto p=policy(suite.mode);
    lr_editor::exportThreads=p.exportThreads;lc::processPriority=suite.mode==RunMode::Performance?BELOW_NORMAL_PRIORITY_CLASS:IDLE_PRIORITY_CLASS;
    if(mediaLibrary)mediaLibrary->thumbnailBudget(p.thumbnailMiB*1024*1024);
    if(perf)perf->intervalMs=p.monitorMs;
    KillTimer(window,1);SetTimer(window,1,p.uiMs,nullptr);
    if(!WritePrivateProfileStringW(L"Suite",L"Mode",std::to_wstring(int(suite.mode)).c_str(),settings.c_str()))throw std::runtime_error("Cannot save mode");
    if(!WritePrivateProfileStringW(L"Suite",L"Schema",L"2",settings.c_str()))throw std::runtime_error("Cannot save schema");
}
void pauseHiddenPanels(){
    lr_editor::hide();
    if(mediaLibrary)mediaLibrary->hide();
    if(audioPanel)audioPanel->hide();
    if(retroExtras)retroExtras->promptVisibility(false);
}
void setPageVisible(int page,bool visible){
    if(!visible){
        if(page==2)lr_editor::hide();
        if(page==3&&mediaLibrary)mediaLibrary->hide();
        if(page==4&&audioPanel)audioPanel->hide();
    }else{
        if(page==2){
            lr_editor::initialize(pages[2],settings.parent_path(),executableRoot/L"editor-tools",suite.english,
                []{return engine&&engine->running();},[](const std::wstring& path){
                    if(retroExtras)retroExtras->addEvent(RetroExtras::Exports);
                    if(mediaLibrary)mediaLibrary->addSaved(path);
                });
            RECT r{};GetWindowRect(pages[2],&r);layoutPage(2,r.right-r.left,r.bottom-r.top);
        }
        if(page==4&&audioPanel&&IsWindowVisible(window)&&!IsIconic(window))
            audioPanel->show(pages[4],-scrollX[4],-scrollPos[4]);
        if(page==3&&mediaLibrary)mediaLibrary->show();
    }
    if(page==RetroDesktop::Prompt&&retroExtras)retroExtras->promptVisibility(visible);
    if(engine&&extras)refresh();
}
void choosePage(int page){
    if(page<0||page>=RetroDesktop::Count)return;
    if(page==RetroDesktop::Prompt&&!(retroExtras&&retroExtras->unlocked()))return;
    desktop.show(page);
}
void buildShell(){
    lr_ui::current=lr_ui::Theme::Classic98;
    if(!lr_ui::init())throw std::runtime_error("Cannot initialize appearance");
    font=lr_ui::makeFont(15,FW_NORMAL,L"Tahoma");
    if(!font)throw std::runtime_error("Cannot initialize Retro font");
    desktop.init(window,font,setPageVisible,layoutPage);
    WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=pageProc;
    wc.lpszClassName=L"LiteReplayRetroPage";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    if(!RegisterClassW(&wc))throw std::runtime_error("Cannot register suite page");
    for(int i=0;i<RetroDesktop::Count;++i){
        pages[i]=CreateWindowExW(WS_EX_CONTROLPARENT,wc.lpszClassName,L"",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_VSCROLL|WS_HSCROLL,
            0,0,1,1,desktop.frame(i),nullptr,wc.hInstance,nullptr);
        if(!pages[i])throw std::runtime_error("Cannot create suite page");
        desktop.attach(i,pages[i]);
    }
    controlParent=pages[1];buildReplayPage();controlParent=nullptr;
    for(int i:{0,3,5,6}){auto title=simple(pages[i],L"STATIC",tr(navKeys[i]),990,20,25,550,35);ShowWindow(title,SW_HIDE);}
    simple(pages[0],L"STATIC",tr("ui.record"),920,36,36,600,26);
    homeStatus=simple(pages[0],L"STATIC",L"",0,36,70,640,64);
    homeToggle=simple(pages[0],L"BUTTON",tr("replay.004"),Toggle,36,154,180,40,WS_TABSTOP);
    simple(pages[0],L"BUTTON",tr("save"),Save,232,154,180,40,WS_TABSTOP);
    simple(pages[0],L"BUTTON",tr("replay"),801,428,154,180,40,WS_TABSTOP);
    simple(pages[0],L"STATIC",tr("ui.hardware"),921,36,252,600,26);
    homeMetrics=simple(pages[0],L"STATIC",L"",0,36,354,640,24);
    const wchar_t* names[]={L"CPU",L"GPU",L"RAM"};HWND* values[]={&homeCpu,&homeGpu,&homeRam};for(int i=0;i<3;++i){simple(pages[0],L"STATIC",names[i],924+i,36+i*216,286,200,22);*values[i]=simple(pages[0],L"STATIC",L"N/A",0,36+i*216,309,200,36);SendMessageW(*values[i],WM_SETFONT,reinterpret_cast<WPARAM>(lr_ui::heading),TRUE);}
    simple(pages[0],L"STATIC",tr("ui.workspace"),922,36,426,600,26);
    simple(pages[0],L"BUTTON",tr("editor"),802,36,474,240,40,WS_TABSTOP);
    simple(pages[0],L"BUTTON",tr("library"),803,292,474,240,40,WS_TABSTOP);
    mediaLibrary=std::make_unique<MediaLibrary>(pages[3],font,settings.parent_path()/L"media.library",tr,[](const std::wstring& path){choosePage(2);lr_editor::importVideos({std::filesystem::path(path)});});
    desktopPerformance=std::make_unique<DesktopPerformance>(window,pages[5],font,settings,executableRoot,suite.english);
    simple(pages[5],L"STATIC",L"",1194,24,769,760,24);
    simple(pages[6],L"STATIC",tr("retro.links"),1980,24,26,488,23);
    simple(pages[6],L"BUTTON",tr("replay"),801,24,61,164,35,WS_TABSTOP);
    simple(pages[6],L"BUTTON",tr("audio"),804,205,61,164,35,WS_TABSTOP);
    simple(pages[6],L"BUTTON",tr("performance"),805,386,61,164,35,WS_TABSTOP);
    retroExtras=std::make_unique<RetroExtras>(pages[7],pages[8],font,settings,choosePage,[]{
        PostMessageW(window,WM_COMMAND,1600+RetroDesktop::Count+RetroDesktop::Prompt,0);
    });
    simple(pages[6],L"STATIC",tr("mode"),992,24,155,220,25);
    modeBox=simple(pages[6],WC_COMBOBOXW,L"",900,250,150,300,200,CBS_DROPDOWNLIST|WS_TABSTOP);
    simple(pages[6],L"STATIC",tr("language"),993,24,210,220,25);
    languageBox=simple(pages[6],WC_COMBOBOXW,L"",901,250,205,300,200,CBS_DROPDOWNLIST|WS_TABSTOP);
    for(auto value:{L"简体中文",L"English"})SendMessageW(languageBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(value));
    SendMessageW(languageBox,CB_SETCURSEL,suite.english?1:0,0);
    simple(pages[6],L"STATIC",tr("policy_note"),994,24,270,530,140);
    simple(pages[6],L"STATIC",tr("shot.quick"),913,24,446,230,28);
    simple(pages[6],HOTKEY_CLASSW,L"",910,260,446,285,28,WS_TABSTOP);
    simple(pages[6],L"STATIC",tr("shot.region"),914,24,491,230,28);
    simple(pages[6],HOTKEY_CLASSW,L"",911,260,491,285,28,WS_TABSTOP);
    SendDlgItemMessageW(pages[6],910,HKM_SETHOTKEY,quickShot,0);SendDlgItemMessageW(pages[6],911,HKM_SETHOTKEY,regionShot,0);
    simple(pages[6],L"BUTTON",tr("shot.apply"),912,24,537,240,32,WS_TABSTOP);
    simple(pages[6],L"STATIC",tr("shot.note"),915,24,583,530,80);
    simple(pages[6],L"STATIC",tr("credits"),916,24,685,550,24);
    simple(pages[0],L"STATIC",tr("credits"),916,36,570,600,24);
    lr_ui::sections[pages[1]]={{8,42,636,292},{8,298,636,578},{8,582,636,752},{8,802,636,940},{8,988,636,1158}};
    lr_ui::sections[pages[6]]={{8,12,580,124},{8,135,580,247},{8,255,580,419},{8,428,580,668}};
    lr_ui::sections[pages[5]]={{8,12,802,67},{8,69,387,512},{392,69,802,512},{8,575,802,801}};
    lr_ui::primary(homeToggle);lr_ui::primary(controls.at(Toggle));lr_ui::primary(controls.at(Apply));
    lr_ui::primary(GetDlgItem(pages[5],1100));
    refreshShellLanguage();layout();refreshAppearance();
}
void saveScreenshotKeys(){
    WORD a=WORD(SendDlgItemMessageW(pages[6],910,HKM_GETHOTKEY,0,0)),b=WORD(SendDlgItemMessageW(pages[6],911,HKM_GETHOTKEY,0,0));
    auto collision=[&](WORD h){return !LOBYTE(h)||!shotModifiers(h)||((LOBYTE(h)==VK_F9||LOBYTE(h)==VK_F7)&&shotModifiers(h)==(MOD_CONTROL|MOD_SHIFT))||(LOBYTE(h)==config.hotkey&&shotModifiers(h)==config.modifiers);};
    if(collision(a)||collision(b)||(LOBYTE(a)==LOBYTE(b)&&shotModifiers(a)==shotModifiers(b)))throw std::runtime_error("Choose two different Ctrl/Alt/Shift hotkeys, separate from Replay and HUD / 请设置不同的组合键，勿与回放或 HUD 冲突");
    UnregisterHotKey(window,3);UnregisterHotKey(window,4);
    try{
        if(!bindShot(3,a)||!bindShot(4,b))throw std::runtime_error("Screenshot hotkey occupied / 截图快捷键已被占用");
        std::wstring values=L"Quick="+std::to_wstring(a);values.push_back(0);values+=L"Region="+std::to_wstring(b);values.push_back(0);values.push_back(0);
        if(!WritePrivateProfileSectionW(L"Screenshots",values.c_str(),settings.c_str()))throw std::runtime_error("Cannot save screenshot settings / 无法保存截图设置");
        quickShot=a;regionShot=b;
    }catch(...){UnregisterHotKey(window,3);UnregisterHotKey(window,4);bindShot(3,quickShot);bindShot(4,regionShot);throw;}
    notify(tr("shot.applied"));
}
void takeScreenshot(bool region){if(screenshots)screenshots->start(region,std::filesystem::path(config.folder)/L"Screenshots",suite.english);}
LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM w,LPARAM l) {
    if(taskbarCreated&&msg==taskbarCreated){addTray();return 0;}
    try {
        switch(msg) {
        case WM_DRAWITEM:if(desktop.draw(*reinterpret_cast<DRAWITEMSTRUCT*>(l)))return TRUE;break;
        case WM_MEASUREITEM:if(desktop.measure(*reinterpret_cast<MEASUREITEMSTRUCT*>(l)))return TRUE;break;
        case WM_ERASEBKGND:desktop.paint(reinterpret_cast<HDC>(w));return 1;
        case WM_SHOWWINDOW:
            if(w){desktop.tick();SetTimer(h,RetroDesktop::ClockTimer,30000,nullptr);}
            else{KillTimer(h,RetroDesktop::ClockTimer);pauseHiddenPanels();}
            break;
        case WM_TIMECHANGE:desktop.tick();return 0;
        case WM_CLOSE:ShowWindow(h,SW_HIDE);refresh();notify(tr("replay.027"));return 0;
        case WM_HOTKEY:if(w==1)save();else if(w==2){extras->toggle();refresh();}else if(w==3)takeScreenshot(false);else if(w==4)takeScreenshot(true);else if(w==5){desktopPerformance->toggle();refresh();}return 0;
        case OverlayChanged:refresh();return 0;
        case WM_SIZE:
            if(w==SIZE_MINIMIZED){KillTimer(h,RetroDesktop::ClockTimer);pauseHiddenPanels();}
            else{
                layout();
                if(IsWindowVisible(h)){
                    desktop.tick();SetTimer(h,RetroDesktop::ClockTimer,30000,nullptr);
                    if(audioPanel&&desktop.visible(4)&&!audioPanel->showing())audioPanel->show(pages[4],-scrollX[4],-scrollPos[4]);
                }
            }
            if(engine&&extras)refresh();
            return 0;
        case WM_GETMINMAXINFO:reinterpret_cast<MINMAXINFO*>(l)->ptMinTrackSize={780,520};return 0;
        case WM_TIMER:
            if(w==76){if(screenshots)screenshots->pumpClipboard();return 0;}
            if(w==RetroDesktop::ClockTimer){desktop.tick();return 0;}
            if(engine&&extras)refresh();
            return 0;
        case WM_APP+2:{
            std::deque<std::wstring> completed;{std::lock_guard<std::mutex> lock(savedEventsMutex);completed.swap(savedEvents);}
            for(auto& path:completed){
                if(retroExtras)retroExtras->addEvent(RetroExtras::Replays);
                extras->play();try{mediaLibrary->addSaved(path);}catch(const std::exception& e){logLine(std::string("Media import failed: ")+utf8(path)+": "+e.what());notify(tr("lib.auto_failed"));}}
            refresh();return 0;}
        case WM_APP+3:refresh();return 0;
        case WM_APP+4:if(screenshots){auto result=screenshots->takeResult();if(!result.empty()){logLine(utf8(result));notify(result);}for(auto& path:screenshots->takeSavedPaths()){
                    if(retroExtras)retroExtras->addEvent(RetroExtras::Screenshots);
                    try{mediaLibrary->addSaved(path);}catch(const std::exception& e){logLine(std::string("Screenshot library import: ")+e.what());notify(tr("lib.auto_failed"));}}}return 0;
        case WM_COMMAND:
            if(desktop.command(LOWORD(w)))return 0;
            if(retroExtras&&retroExtras->command(LOWORD(w),HIWORD(w)))return 0;
            if(desktopPerformance&&desktopPerformance->command(LOWORD(w),HIWORD(w))){refresh();return 0;}
            if(LOWORD(w)==912){saveScreenshotKeys();return 0;}

            if(LOWORD(w)>=800&&LOWORD(w)<800+RetroDesktop::Count){choosePage(LOWORD(w)-800);return 0;}
            if(LOWORD(w)==900&&HIWORD(w)==CBN_SELCHANGE){
                suite.mode=parseMode(int(SendMessageW(modeBox,CB_GETCURSEL,0,0)));applyMode();return 0;}
            if(LOWORD(w)==901&&HIWORD(w)==CBN_SELCHANGE){
                bool next=SendMessageW(languageBox,CB_GETCURSEL,0,0)==1;
                loadLanguage(next);suite.english=next;
                if(!WritePrivateProfileStringW(L"Suite",L"English",next?L"1":L"0",settings.c_str()))throw std::runtime_error("Cannot save language");
                refreshShellLanguage();refresh();return 0;}
            switch(LOWORD(w)) {
            case ApplyVolumes:{int sys=number(SystemVolume,0,200),mic=number(MicVolume,0,200);config.systemVolume=sys;config.micVolume=mic;persist();engine->audioVolumes(sys,mic);notify(tr("audio.record.saved"));return 0;}
            case RefreshDevices:if(!engine->running())refreshAudioChoices();return 0;
            case CompatPreset:
                if(engine->running())return 0;
                SendMessageW(controls.at(Size),CB_SETCURSEL,resolutionIndex(1280),0);
                SendMessageW(controls.at(Fps),CB_SETCURSEL,fpsIndex(30),0);
                SendMessageW(controls.at(Encoder),CB_SETCURSEL,0,0);SendMessageW(controls.at(Backend),CB_SETCURSEL,0,0);
                SendMessageW(controls.at(AllowCpu),BM_SETCHECK,BST_CHECKED,0);
                set(Monitor,L"0");set(Bitrate,L"6");set(Memory,L"128");set(Seconds,L"60");
                notify(tr("compat.preset_ready"));return 0;
            case Toggle:if(engine->running())engine->stop();else{apply();lr_editor::prioritizeReplay();engine->start(config);}refresh();return 0;
            case Save:save();return 0;
            case Apply:apply();notify(tr("replay.028"));return 0;
            case ShowPerformance:showMonitor();return 0;
            case ShowExtras:extras->show();return 0;
            case ShowAudio:ShowWindow(window,SW_RESTORE);SetForegroundWindow(window);choosePage(4);return 0;
            case Open:std::filesystem::create_directories(config.folder);ShellExecuteW(h,L"open",config.folder.c_str(),nullptr,nullptr,SW_SHOWNORMAL);return 0;
            case RetroDesktop::CaptureTray:SendMessageW(h,WM_COMMAND,Toggle,0);return 0;
            case RetroDesktop::OverlayTray:desktopPerformance->toggle();refresh();return 0;
            case 1590:case Exit:DestroyWindow(h);return 0;
            }break;
        case TrayMessage:
            if(l==WM_LBUTTONDBLCLK){
                ShowWindow(h,SW_RESTORE);SetForegroundWindow(h);
                if(audioPanel&&desktop.visible(4)&&!audioPanel->showing())audioPanel->show(pages[4],-scrollX[4],-scrollPos[4]);
                refresh();
            }
            if(l==WM_RBUTTONUP){HMENU m=CreatePopupMenu();AppendMenuW(m,MF_STRING,Toggle,engine->running()?tr("replay.003"):tr("replay.004"));
                AppendMenuW(m,MF_STRING,Save,tr("replay.021"));AppendMenuW(m,MF_STRING,Open,tr("replay.023"));AppendMenuW(m,MF_STRING,ShowPerformance,tr("replay.029"));AppendMenuW(m,MF_STRING,ShowExtras,tr("replay.030"));AppendMenuW(m,MF_STRING,ShowAudio,tr("replay.026"));AppendMenuW(m,MF_STRING,Exit,tr("replay.031"));
                POINT p;GetCursorPos(&p);SetForegroundWindow(h);TrackPopupMenu(m,TPM_RIGHTBUTTON,p.x,p.y,0,h,nullptr);DestroyMenu(m);PostMessageW(h,WM_NULL,0,0);}
            return 0;
        case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:return lr_ui::color(h,msg,w,l);
        case WM_DESTROY:
            lr_editor::shutdown();if(engine){engine->stop();engine.reset();}
            {std::deque<std::wstring> pending;{std::lock_guard<std::mutex> lock(savedEventsMutex);pending.swap(savedEvents);}
            for(auto& path:pending){
                if(retroExtras)retroExtras->addEvent(RetroExtras::Replays);
                try{mediaLibrary->addSaved(path);}catch(const std::exception& e){logLine(std::string("Shutdown media import failed: ")+utf8(path)+": "+e.what());}}}
            if(screenshots){screenshots->finish();for(auto& path:screenshots->takeSavedPaths()){
                    if(retroExtras)retroExtras->addEvent(RetroExtras::Screenshots);
                    try{mediaLibrary->addSaved(path);}catch(const std::exception& e){logLine(std::string("Shutdown screenshot import: ")+e.what());}}}
            desktopPerformance.reset();screenshots.reset();UnregisterHotKey(h,3);UnregisterHotKey(h,4);UnregisterHotKey(h,5);mediaLibrary.reset();audioPanel.reset();extras.reset();UnregisterHotKey(h,2);KillTimer(h,1);KillTimer(h,RetroDesktop::ClockTimer);UnregisterHotKey(h,1);Shell_NotifyIconW(NIM_DELETE,&tray);PostQuitMessage(0);return 0;
        }
    }catch(const std::exception& e){logLine(e.what());MessageBoxW(h,wide(e.what()).c_str(),L"LiteReplay",MB_ICONERROR);}
    return DefWindowProcW(h,msg,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR args,int show) {
    int argc=0;LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(argv&&argc>=2&&wcscmp(argv[1],L"--thumbnail-worker")==0){int code=argc==4?lr::thumbnailWorker(argv[2],argv[3]):2;LocalFree(argv);return code;}
    if(argv)LocalFree(argv);
    SetProcessDPIAware();
    HANDLE single=CreateMutexW(nullptr,TRUE,L"Local\\LiteReplay.Singleton");
    if(!single)return 1;
    if(GetLastError()==ERROR_ALREADY_EXISTS){MessageBoxW(nullptr,L"LiteReplay 已在运行，请查看系统托盘。",L"LiteReplay",MB_OK);CloseHandle(single);return 0;}
    HRESULT co=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    int result=0;
    try {
        load();INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_STANDARD_CLASSES|ICC_WIN95_CLASSES|ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&ic);
        WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=instance;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        retroIcon=RetroDesktop::appIcon();
        wc.lpszClassName=L"LiteReplayWindow";wc.hIcon=retroIcon?retroIcon:LoadIconW(nullptr,IDI_APPLICATION);
        if(!RegisterClassW(&wc))throw std::runtime_error("注册窗口失败");
        window=CreateWindowExW(0,wc.lpszClassName,L"LiteReplay Win98 Edition — Yuzi_Lj",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
            CW_USEDEFAULT,CW_USEDEFAULT,1360,920,nullptr,nullptr,instance,nullptr);
        if(!window)throw std::runtime_error("创建窗口失败");
        buildShell();screenshots=std::make_unique<ScreenshotTool>(window);engine=std::make_unique<Engine>(logLine,[](const std::wstring& path){{std::lock_guard<std::mutex> lock(savedEventsMutex);savedEvents.push_back(path);}PostMessageW(window,WM_APP+2,0,0);});extras=std::make_unique<Extras>(settings,font);audioPanel=std::make_unique<AudioPanel>(settings,font);
        if(!RegisterHotKey(window,2,MOD_CONTROL|MOD_SHIFT|MOD_NOREPEAT,VK_F9))MessageBoxW(window,tr("replay.032"),L"LiteReplay",MB_OK);
        taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");addTray();applyMode();refresh();
        if(!RegisterHotKey(window,1,config.modifiers|MOD_NOREPEAT,config.hotkey))notify(tr("replay.033"));
        std::wstring cmd=args;
        if(cmd.find(L"--tray")==std::wstring::npos)ShowWindow(window,show);
        if(cmd.find(L"--start")!=std::wstring::npos){engine->start(config);refresh();}
        if(!bindShot(3,quickShot))notify(tr("shot.conflict"));
        if(!bindShot(4,regionShot))notify(tr("shot.conflict"));
        if(!RegisterHotKey(window,5,MOD_CONTROL|MOD_SHIFT|MOD_NOREPEAT,VK_F7))
            notify(suite.english?L"Ctrl+Shift+F7 is in use. Toggle the overlay on the Performance page.":L"Ctrl+Shift+F7 已被占用，可在性能页打开覆盖层。");
        MSG msg;
        while(GetMessageW(&msg,nullptr,0,0)>0){
            if(msg.message==WM_LBUTTONDOWN||msg.message==WM_RBUTTONDOWN||msg.message==WM_KEYDOWN)desktop.activateFrom(msg.hwnd);
            if(retroExtras&&retroExtras->key(msg))continue;
            if(!lr_editor::dialog(&msg)&&!(audioPanel&&audioPanel->dialog(&msg))&&!IsDialogMessageW(window,&msg)){
                TranslateMessage(&msg);DispatchMessageW(&msg);
            }
        }
    }catch(const std::exception& e){MessageBoxW(nullptr,wide(e.what()).c_str(),L"LiteReplay 启动错误",MB_ICONERROR);result=1;}
    lr_editor::shutdown();desktopPerformance.reset();screenshots.reset();mediaLibrary.reset();audioPanel.reset();extras.reset();retroExtras.reset();perf.reset();engine.reset();if(font)DeleteObject(font);lr_ui::shutdown();
    if(retroIcon)DestroyIcon(retroIcon);
    if(SUCCEEDED(co))CoUninitialize();CloseHandle(single);return result;
}
