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
#include "../src/app.cpp"
#undef ShellExecuteW
#undef wWinMain

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int){
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    const auto root=std::filesystem::current_path();
    std::ofstream report(root/L"win98-1.5-smoke.txt");
    int failures=0;
    auto expect=[&](bool good,const char* name){
        report<<(good?"PASS ":"FAIL ")<<name<<std::endl;
        failures+=!good;
    };
    try{
        executableRoot=root.parent_path()/L"bin"/L"windows-x64";
        auto testSettings=root/L"win98-1.5-smoke-settings";
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
        expect(std::wstring(version).find(L"1.5")!=std::wstring::npos,"About displays version 1.5");
        expect(IsWindowVisible(GetDlgItem(about,RetroExtras::GitHubProfile)),"GitHub profile action is visible");
        SendMessageW(about,WM_COMMAND,MAKEWPARAM(RetroExtras::GitHubProfile,BN_CLICKED),0);
        expect(opened==L"https://github.com/MikaW21","profile action opens GitHub profile");
        SendMessageW(about,WM_COMMAND,MAKEWPARAM(RetroExtras::GitHubDownloads,BN_CLICKED),0);
        expect(opened==L"https://github.com/MikaW21/LiteReplay/releases","download action opens project releases");
        expect(wc.hIcon&&retroIcon&&GetClassLongPtrW(window,GCLP_HICONSM)==reinterpret_cast<LONG_PTR>(retroIcon),
            "tray and window use the embedded icon group");
        games::Card card;card.id=7;card.name=L"验证游戏";card.categories=L"动作；独立";
        wchar_t exe[32768];GetModuleFileNameW(nullptr,exe,32768);
        card.launcher=L"C:/launcher.exe";card.process=exe;card.recording=config;
        card.recording.seconds=120;card.recording.fps=30;card.recording.width=1280;
        games::write(card);games::load();games::scan();
        expect(games::active==7,"matches actual process by path");
        auto effective=games::effective();
        expect(effective.width==1280&&effective.height==720&&effective.seconds==120&&effective.fps==30,"profile overrides applied");
        games::open();expect(games::view&&ListView_GetItemCount(GetDlgItem(games::view,15))==1,"native game library shows saved card");
        expect(SendDlgItemMessageW(games::view,14,LB_GETCOUNT,0,0)==4,"both custom categories loaded");
        games::countReplay((std::filesystem::path(effective.folder)/L"test.mp4").wstring());
        expect(games::find(7)->replays>=1,"completed replay counted by game output folder");
        games::draft=*games::find(7);
        HWND sheet=CreateDialogW(instance,MAKEINTRESOURCEW(7101),games::view,games::properties);
        expect(sheet&&TabCtrl_GetItemCount(GetDlgItem(sheet,20))==3,"property dialog has three pages");
        DestroyWindow(sheet);DestroyWindow(games::view);
        games::cards.clear();games::scan();expect(games::effective().folder==config.folder,"global settings restored");
        DestroyWindow(window);
        if(wc.hIcon)DestroyIcon(wc.hIcon);
        if(retroIcon)DestroyIcon(retroIcon);
    }catch(const std::exception& e){report<<e.what()<<std::endl;++failures;}
    CoUninitialize();
    return failures?1:0;
}


