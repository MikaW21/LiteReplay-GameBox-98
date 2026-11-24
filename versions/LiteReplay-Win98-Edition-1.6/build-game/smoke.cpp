#include <windows.h>
#include <shellapi.h>
#include <string>
static std::wstring opened;
static HINSTANCE recordOpen(HWND,LPCWSTR,LPCWSTR url,LPCWSTR,LPCWSTR,INT){
    opened=url;
    return reinterpret_cast<HINSTANCE>(INT_PTR(33));
}
#include <vector>
static UINT testChoice=0;
static std::vector<UINT> commands;
static int nested=0;
static BOOL WINAPI inspectMenu(HMENU menu,UINT,int,int,int,HWND,const RECT*){
    commands.clear();nested=0;
    for(int i=0;i<GetMenuItemCount(menu);++i){
        commands.push_back(GetMenuItemID(menu,i));
        if(auto sub=GetSubMenu(menu,i))nested=GetMenuItemCount(sub);
    }
    return testChoice;
}
#define TrackPopupMenu inspectMenu
#define wWinMain liteReplayMain
#define ShellExecuteW recordOpen
static int dialogAnswer=IDNO;
static std::wstring lastPrompt;
static int WINAPI recordDialog(HWND,LPCWSTR message,LPCWSTR,UINT){lastPrompt=message;return dialogAnswer;}
#define MessageBoxW recordDialog
#include "../src/app.cpp"
#undef ShellExecuteW
#undef MessageBoxW
#undef wWinMain

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int){
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    const auto root=std::filesystem::current_path();
    std::ofstream report(root/L"win98-1.6-smoke.txt");
    int failures=0;
    auto expect=[&](bool good,const char* name){
        report<<(good?"PASS ":"FAIL ")<<name<<std::endl;
        failures+=!good;
    };
    try{
        executableRoot=root.parent_path()/L"bin"/L"windows-x64";
        auto testSettings=root/(L"smoke-settings-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
        std::filesystem::create_directories(testSettings);
        settings=testSettings/L"settings.ini";logPath=testSettings/L"test.log";
        loadLanguage(false);
        INITCOMMONCONTROLSEX common{sizeof(common),ICC_WIN95_CLASSES|ICC_STANDARD_CLASSES};
        InitCommonControlsEx(&common);
        WNDCLASSEXW wc{sizeof(wc)};
        wc.hInstance=instance;wc.lpfnWndProc=proc;wc.lpszClassName=L"LiteReplayTestAbout";
        wc.hIcon=reinterpret_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(IDI_LITEREPLAY),IMAGE_ICON,32,32,0));
        retroIcon=reinterpret_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(IDI_LITEREPLAY),IMAGE_ICON,16,16,0));
        wc.hIconSm=retroIcon;
        if(!RegisterClassExW(&wc))throw std::runtime_error("Cannot register test shell");
        window=CreateWindowExW(0,wc.lpszClassName,L"Test shell",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
            0,0,1080,720,nullptr,nullptr,instance,nullptr);
        if(!window)throw std::runtime_error("Cannot create test shell");
        buildShell();ShowWindow(window,SW_SHOWNOACTIVATE);
        choosePage(RetroDesktop::About);
        HWND about=pages[RetroDesktop::About];
        BITMAP avatar{};
        EnumChildWindows(about,[](HWND h,LPARAM context)->BOOL{
            wchar_t name[32]{};
            GetClassNameW(h,name,32);
            if(wcscmp(name,L"Static")!=0||(GetWindowLongPtrW(h,GWL_STYLE)&SS_TYPEMASK)!=SS_BITMAP)return TRUE;
            auto bitmap=reinterpret_cast<HBITMAP>(SendMessageW(h,STM_GETIMAGE,IMAGE_BITMAP,0));
            GetObjectW(bitmap,sizeof(BITMAP),reinterpret_cast<BITMAP*>(context));
            return FALSE;
        },reinterpret_cast<LPARAM>(&avatar));
        expect(avatar.bmWidth==108&&avatar.bmHeight==108,"About loads the embedded GitHub avatar");
        wchar_t version[40]{};GetDlgItemTextW(about,RetroExtras::Version,version,40);
        expect(std::wstring(version).find(L"1.6")!=std::wstring::npos,"About displays version 1.6");
        expect(IsWindowVisible(GetDlgItem(about,RetroExtras::GitHubProfile)),"GitHub profile action is visible");
        SendMessageW(about,WM_COMMAND,MAKEWPARAM(RetroExtras::GitHubProfile,BN_CLICKED),0);
        expect(opened==L"https://github.com/MikaW21","profile action opens GitHub profile");
        SendMessageW(about,WM_COMMAND,MAKEWPARAM(RetroExtras::GitHubDownloads,BN_CLICKED),0);
        expect(opened==L"https://github.com/MikaW21/LiteReplay/releases","download action opens project releases");
        expect(wc.hIcon&&retroIcon&&GetClassLongPtrW(window,GCLP_HICONSM)==reinterpret_cast<LONG_PTR>(retroIcon),
            "tray and window use the embedded icon group");
        games::Card c;c.id=7;c.name=L"测试游戏";c.categories=L"动作；独立";
        wchar_t exe[32768];GetModuleFileNameW(nullptr,exe,32768);
        c.launcher=L"C:/launcher.exe";c.process=exe;c.recording=config;
        c.recording.seconds=120;c.recording.width=1280;c.recording.fps=30;
        games::write(c);games::load();games::loadActivity();games::eventSounds=false;games::eventWindows=false;
        config.folder=(testSettings/L"media").wstring();
        games::scan();auto context=games::current();auto profile=games::effective();
        expect(context.game==7&&context.visit,"profile loaded and actual EXE starts session");
        expect(profile.width==1280&&profile.height==720&&profile.seconds==120&&profile.fps==30,"game recording overrides");
        games::open();expect(ListView_GetItemCount(GetDlgItem(games::view,15))==1,"game browser shows saved card");
        games::draft=*games::find(7);
        auto sheet=CreateDialogW(instance,MAKEINTRESOURCEW(7101),games::view,games::properties);
        expect(sheet&&TabCtrl_GetItemCount(GetDlgItem(sheet,20))==3,"three property tabs");DestroyWindow(sheet);
        auto folder=std::filesystem::path(profile.folder);
        std::filesystem::create_directories(folder/L"Screenshots");
        auto replay=folder/L"sample.mp4",shot=folder/L"Screenshots"/L"sample.png";
        auto exported=testSettings/L"sample-export.mp4",unassigned=testSettings/L"manual.mp4";
        for(auto path:{replay,shot,exported,unassigned}){std::ofstream file(path,std::ios::binary);file<<std::string(1048576,'a');}
        games::replayContext=context;games::manualGame=0;games::scan();
        expect(games::effective().folder==config.folder&&games::visits.back().end>=games::visits.back().start,"manual end closes session and restores global defaults");
        games::savedReplay(replay.wstring());
        games::recordMedia(shot.wstring(),games::MediaKind::Screenshot,context);
        games::exportContext=context;choosePage(2);lr_editor::saved(exported.wstring());
        mediaLibrary->addSaved(unassigned.wstring());
        expect(games::find(7)->replays==1&&games::classify(replay.wstring()).context.visit==context.visit,"late replay retains original session");
        expect(games::classify(shot.wstring()).kind==games::MediaKind::Screenshot&&games::classify(exported.wstring()).kind==games::MediaKind::Export,"screenshot and editor export retain kind");
        games::visits.clear();games::associations.clear();games::loadActivity();
        expect(games::visits.size()==1&&games::classify(exported.wstring()).context.visit==context.visit,"history and associations survive reload");
        games::showGameMedia(7);
        expect(ListView_GetItemCount(GetDlgItem(pages[3],1104))==3,"game media filter");
        games::selectGameMedia(0);
        expect(ListView_GetItemCount(GetDlgItem(pages[3],1104))==1,"uncategorized media filter");
        games::ActivityView diskState{1,7};
        auto disk=CreateDialogParamW(instance,MAKEINTRESOURCEW(7105),window,games::activityDialog,(LPARAM)&diskState);
        expect(disk&&ListView_GetItemCount(GetDlgItem(disk,81))==3&&games::field(disk,86).find(L"3.00 MiB")!=std::wstring::npos,"disk totals from actual files");
        std::filesystem::rename(shot,folder/L"moved.png");
        games::renderActivity(disk,diskState);
        expect(games::field(disk,86).find(L"不可访问 1")!=std::wstring::npos,"disk reports moved file");
        SetDlgItemInt(disk,84,0,FALSE);SendMessageW(disk,WM_COMMAND,92,0);
        expect(games::diskThreshold==0&&GetPrivateProfileIntW(L"GameBox",L"ThresholdGiB",5,settings.c_str())==0,"disk threshold persists");
        auto list=GetDlgItem(disk,81);
        ListView_SetItemState(list,0,LVIS_SELECTED,LVIS_SELECTED);
        dialogAnswer=IDNO;SendMessageW(disk,WM_COMMAND,95,0);
        expect(mediaLibrary->assets().size()==4&&lastPrompt.find(replay.wstring())!=std::wstring::npos,"cleanup lists exact file and honors cancel");
        ListView_SetItemState(list,0,LVIS_SELECTED,LVIS_SELECTED);
        dialogAnswer=IDYES;SendMessageW(disk,WM_COMMAND,95,0);
        expect(mediaLibrary->assets().size()==3&&std::filesystem::is_regular_file(replay),"remove index retains actual file");
        SendDlgItemMessageW(disk,80,CB_SETCURSEL,1,0);games::renderActivity(disk,diskState);
        ListView_SetItemState(list,0,LVIS_SELECTED,LVIS_SELECTED);
        SendDlgItemMessageW(disk,98,CB_SETCURSEL,1,0);SendMessageW(disk,WM_COMMAND,97,0);
        expect(games::classify(unassigned.wstring()).context.game==7&&std::filesystem::is_regular_file(unassigned),"manual categorization does not move file");
        DestroyWindow(disk);
        games::ActivityView historyState{0,-1};
        auto history=CreateDialogParamW(instance,MAKEINTRESOURCEW(7102),window,games::activityDialog,(LPARAM)&historyState);
        expect(history&&ListView_GetItemCount(GetDlgItem(history,81))==1,"history shows closed session");
        SetDlgItemTextW(history,83,L"1900-01-01");SendMessageW(history,WM_COMMAND,85,0);
        expect(ListView_GetItemCount(GetDlgItem(history,81))==0,"history filters by date");
        DestroyWindow(history);
        for(int id:{1594,1595}){
            SendDlgItemMessageW(pages[6],id,BM_SETCHECK,BST_UNCHECKED,0);
            SendMessageW(window,WM_COMMAND,id,0);
        }
        expect(!games::eventSounds&&!games::eventWindows&&GetPrivateProfileIntW(L"GameBox",L"Sounds",1,settings.c_str())==0&&GetPrivateProfileIntW(L"GameBox",L"Windows",1,settings.c_str())==0,"independent notification settings persist");
        games::event(7,L"通知测试");expect(!games::noticeWindow,"disabled notification stays hidden");
        games::eventWindows=true;games::event(7,L"通知测试");
        expect(games::noticeWindow&&games::field(games::noticeWindow,11).find(c.name)!=std::wstring::npos,"enabled notification shows game");
        if(games::noticeWindow)DestroyWindow(games::noticeWindow);
        games::eventWindows=false;
        wchar_t system[32768];GetSystemDirectoryW(system,32768);
        auto ping=std::filesystem::path(system)/L"ping.exe";
        games::Card other=c;other.id=8;other.name=L"进程退出测试";other.launcher=ping;other.process=ping;
        games::cards={other};
        STARTUPINFOW si{sizeof si};PROCESS_INFORMATION pi{};
        std::wstring command=L"ping -n 3 127.0.0.1";
        bool started=CreateProcessW(ping.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi);
        expect(started,"test process started");
        if(started){
            games::manualGame=-1;
            for(int i=0;i<10&&games::active!=8;++i){Sleep(50);games::scan();}
            expect(games::active==8,"new process starts another game session automatically");
            WaitForSingleObject(pi.hProcess,6000);games::scan();
            expect(games::active==0&&games::visits.back().game==8&&games::visits.back().end>0,"process exit closes session automatically");
            CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
        }

        DestroyWindow(window);
        std::error_code cleanup;
        std::filesystem::remove_all(testSettings,cleanup);
    }catch(const std::exception& e){report<<e.what()<<std::endl;++failures;}
    CoUninitialize();
    return failures?1:0;
}
