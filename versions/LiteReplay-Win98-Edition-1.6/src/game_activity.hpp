// Persistent activity and media associations for the GameBox catalog.
enum class MediaKind { Unknown, Replay, Screenshot, Export };
struct Context { int game=0; uint64_t visit=0; };
struct Visit {
    uint64_t id=0;
    int game=0;
    int64_t start=0,end=0;
};
struct Association { Context context; MediaKind kind=MediaKind::Unknown; };
std::vector<Visit> visits;
std::map<std::wstring,Association> associations;
uint64_t nextVisit=1,currentVisit=0;
int manualGame=-1; // -1 means automatic, 0 explicitly uses the global profile.
bool eventSounds=true,eventWindows=true;
int diskThreshold=5;
bool shuttingDown=false;
int64_t now(){return std::chrono::duration_cast<std::chrono::seconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();}
std::wstring dateText(int64_t time){
    if(!time)return L"进行中";
    auto value=static_cast<time_t>(time);tm local{};
    if(localtime_s(&local,&value))return L"—";
    wchar_t text[32]{};wcsftime(text,32,L"%Y-%m-%d %H:%M:%S",&local);return text;
}
std::wstring gameName(int id){
    if(auto card=find(id))return card->name;
    return id?L"游戏 #"+std::to_wstring(id):L"未归类";
}
std::wstring key(const std::wstring& path){
    auto text=std::filesystem::path(path).lexically_normal().wstring();
    if(!text.empty())CharLowerBuffW(text.data(),DWORD(text.size()));
    return text;
}
void saveActivity(){
    auto dir=directory();std::filesystem::create_directories(dir);
    auto file=dir/L"activity.txt",temp=dir/L"activity.tmp";
    std::ofstream out(temp,std::ios::binary|std::ios::trunc);
    out<<"GAMEBOX 1 "<<nextVisit<<'\n';
    for(auto& v:visits)out<<"V "<<v.id<<' '<<v.game<<' '<<v.start<<' '<<v.end<<'\n';
    for(auto& [path,a]:associations)
        out<<"A "<<a.context.game<<' '<<a.context.visit<<' '<<int(a.kind)<<' '<<std::quoted(utf8(path))<<'\n';
    out.close();
    if(!out||!MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("无法写入游戏会话与素材关联");
}
bool activityReadable=true;
void persistActivity(){
    if(!activityReadable)throw std::runtime_error("活动索引不可读，请保留并检查 activity.txt");
    saveActivity();
}
void loadActivity(){
    eventSounds=GetPrivateProfileIntW(L"GameBox",L"Sounds",1,settings.c_str())!=0;
    eventWindows=GetPrivateProfileIntW(L"GameBox",L"Windows",1,settings.c_str())!=0;
    diskThreshold=std::clamp(int(GetPrivateProfileIntW(L"GameBox",L"ThresholdGiB",5,settings.c_str())),0,999);
    auto file=directory()/L"activity.txt";
    if(!std::filesystem::exists(file))return;
    std::ifstream in(file,std::ios::binary);
    std::string magic;int version=0;uint64_t sequence=0;
    if(!(in>>magic>>version>>sequence)||magic!="GAMEBOX"||version!=1||!sequence){
        activityReadable=false;return;
    }
    std::vector<Visit> loadedVisits;
    std::map<std::wstring,Association> loadedLinks;
    char type;
    while(in>>type){
        if(type=='V'){
            Visit v;
            if(!(in>>v.id>>v.game>>v.start>>v.end)||!v.id||v.id>=sequence||v.start<=0){activityReadable=false;return;}
            if(!v.end)v.end=-1; // Unexpected shutdown: end time remains editable.
            loadedVisits.push_back(v);
        }else if(type=='A'){
            Association a;int kind=0;std::string path;
            if(!(in>>a.context.game>>a.context.visit>>kind>>std::quoted(path))||kind<0||kind>3||path.empty()){activityReadable=false;return;}
            a.kind=static_cast<MediaKind>(kind);loadedLinks.emplace(wide(path),a);
        }else {activityReadable=false;return;}
    }
    if(!in.eof()){activityReadable=false;return;}
    visits=std::move(loadedVisits);associations=std::move(loadedLinks);nextVisit=sequence;
}
Context current(){return {active,currentVisit};}
Association classify(const std::wstring& path){
    if(auto item=associations.find(key(path));item!=associations.end())return item->second;
    auto parent=std::filesystem::path(path).parent_path();
    bool shot=!lstrcmpiW(parent.filename().c_str(),L"Screenshots");
    if(shot)parent=parent.parent_path();
    auto number=parent.filename().wstring();
    if(!lstrcmpiW(parent.parent_path().filename().c_str(),L"GameBox98")&&!number.empty()&&
        number.find_first_not_of(L"0123456789")==number.npos&&find(_wtoi(number.c_str())))
        return {{_wtoi(number.c_str()),0},shot?MediaKind::Screenshot:MediaKind::Replay};
    if(!lstrcmpiW(std::filesystem::path(path).extension().c_str(),L".png"))return {{},MediaKind::Screenshot};
    return {};
}
void associate(const std::wstring& path,Association association){
    auto normalized=key(path);
    auto node=associations.extract(normalized);
    associations[normalized]=association;
    try{persistActivity();}catch(...){associations.erase(normalized);associations.insert(std::move(node));throw;}
}
void changePath(const std::wstring& from,const std::wstring& to){
    auto node=associations.extract(key(from));
    if(node.empty())return;
    auto previous=node.key();
    auto destination=key(to);
    auto replaced=associations.extract(destination);
    node.key()=destination;associations.insert(std::move(node));
    try{persistActivity();}catch(...){
        node=associations.extract(destination);
        node.key()=previous;associations.insert(std::move(node));associations.insert(std::move(replaced));throw;
    }
}
void trackGame(int game){
    if(game==active)return;
    auto stamp=now();
    const auto oldVisit=currentVisit;
    auto previous=std::find_if(visits.begin(),visits.end(),[&](const Visit& v){return v.id==oldVisit;});
    const size_t previousIndex=size_t(previous-visits.begin());
    if(previous!=visits.end())previous->end=std::max(stamp,previous->start);
    if(game)visits.push_back({nextVisit++,game,stamp,0});
    try{if(previousIndex<visits.size()||game)persistActivity();}
    catch(...){
        if(game){visits.pop_back();--nextVisit;}
        if(previousIndex<visits.size())visits[previousIndex].end=0;
        throw;
    }
    active=game;currentVisit=game?visits.back().id:0;
}

HWND noticeWindow{};
int noticeGame=0;
std::wstring noticeEvent;
ULONGLONG noticeTime=0;
bool noticeRepeat(int game,const std::wstring& event,ULONGLONG time){
    return game==noticeGame&&event==noticeEvent&&time-noticeTime<8000;
}
void openActivity(int mode,int game,HWND owner=nullptr);
void showGameMedia(int game);
INT_PTR CALLBACK noticeProc(HWND h,UINT msg,WPARAM w,LPARAM){
    if(msg==WM_INITDIALOG){
        COLORREF blue=RGB(0,0,128);DwmSetWindowAttribute(h,35,&blue,sizeof blue);
        SendMessageW(h,WM_SETICON,ICON_SMALL,(LPARAM)retroIcon);
        SetTimer(h,1,8500,nullptr);return TRUE;
    }
    if(msg==WM_TIMER||msg==WM_CLOSE){DestroyWindow(h);return TRUE;}
    if(msg==WM_DESTROY){noticeWindow=nullptr;return TRUE;}
    if(msg==WM_COMMAND){
        if(LOWORD(w)==10){DestroyWindow(h);try{showGameMedia(noticeGame);}catch(const std::exception& e){logLine(e.what());}return TRUE;}
        if(LOWORD(w)==IDCANCEL){DestroyWindow(h);return TRUE;}
    }
    return FALSE;
}
void event(int game,const std::wstring& message){
    if(shuttingDown||(!eventSounds&&!eventWindows))return;
    auto tick=GetTickCount64();
    if(noticeRepeat(game,message,tick))return;
    auto previousEvent=noticeEvent;
    bool combine=noticeWindow&&game==noticeGame&&tick-noticeTime<3000;
    noticeEvent=message;noticeGame=game;noticeTime=tick;
    if(eventSounds&&!combine)PlaySoundW((executableRoot/L"assets"/L"gamebox-event.wav").c_str(),nullptr,SND_FILENAME|SND_ASYNC|SND_NODEFAULT);
    if(!eventWindows)return;
    if(!noticeWindow)noticeWindow=CreateDialogW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(7103),window,noticeProc);
    if(!noticeWindow)return;
    auto text=(game?gameName(game):L"全局录制")+L"\n"+(combine?previousEvent+L" · "+message:message);
    SetDlgItemTextW(noticeWindow,11,text.c_str());
    RECT box{},work{};GetWindowRect(noticeWindow,&box);
    MONITORINFO monitor{sizeof monitor};
    if(GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor))work=monitor.rcWork;
    else SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    SetWindowPos(noticeWindow,HWND_TOP,work.right-(box.right-box.left)-12,work.bottom-(box.bottom-box.top)-12,0,0,
        SWP_NOACTIVATE|SWP_NOSIZE|SWP_SHOWWINDOW);
    SetTimer(noticeWindow,1,8500,nullptr);
}
std::filesystem::path storageDirectory(const std::filesystem::path& path){
    auto dir=path;
    while(!dir.empty()&&dir!=dir.root_path()){
        std::error_code ec;
        if(std::filesystem::is_directory(dir,ec))break;
        if(ec&&ec!=std::errc::no_such_file_or_directory)return path;
        dir=dir.parent_path();
    }
    return dir;
}
std::map<std::wstring,bool> lowSpace;
ULONGLONG diskChecked=0;
void checkDirectory(const std::filesystem::path& folder,int game){
    if(!diskThreshold)return;
    auto existing=storageDirectory(folder);
    ULARGE_INTEGER available{};
    if(!GetDiskFreeSpaceExW(existing.c_str(),&available,nullptr,nullptr))return;
    wchar_t volume[MAX_PATH]{};
    auto identity=GetVolumePathNameW(existing.c_str(),volume,MAX_PATH)?key(volume):key(existing.wstring());
    bool low=available.QuadPart<uint64_t(diskThreshold)*1024*1024*1024;
    if(low&&!lowSpace[identity])event(game,L"磁盘空间不足 · "+identity);
    lowSpace[identity]=low;
}
void checkDisk(){
    auto tick=GetTickCount64();
    if(diskChecked&&tick-diskChecked<30000)return;
    diskChecked=tick;
    checkDirectory(config.folder,active);
}
