#pragma once
#include "retro_desktop.hpp"
#include "app_resources.h"
#include <climits>
#include <filesystem>
#include <shellapi.h>

namespace lr {
class RetroExtras {
public:
    enum Event { Replays,Screenshots,Exports };
    static constexpr int Version=1700,OpenPrompt=1701,Run=1702,GitHubProfile=1703,GitHubDownloads=1704;
private:
    static constexpr UINT AnimationTimer=88;
    HWND about_,prompt_,version_{},open_{},stats_{},details_{},avatarView_{},output_{},input_{},run_{},animation_{};
    HBITMAP avatar_{};
    HFONT font_{},mono_{};
    std::filesystem::path settings_;
    std::function<void(int)> openPage_;
    std::function<void()> closePrompt_;
    std::array<int,3> counts_{};
    int clicks_=0,frame_=0;
    ULONGLONG lastClick_=0;
    bool english_=false,unlocked_=false,played_=false;
    const wchar_t* lang(const wchar_t* zh,const wchar_t* en)const{return english_?en:zh;}
    HWND add(HWND parent,const wchar_t* type,const wchar_t* text,int id,int x,int y,int w,int h,DWORD style=0){
        HWND view=CreateWindowExW(0,type,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,parent,
            reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
        if(!view)throw std::runtime_error("Cannot create Retro extras control");
        SendMessageW(view,WM_SETFONT,reinterpret_cast<WPARAM>(font_),FALSE);return view;
    }
    static std::wstring read(HWND view){
        int n=GetWindowTextLengthW(view);
        std::wstring value(size_t(n)+1,L'\0');GetWindowTextW(view,value.data(),n+1);value.resize(n);return value;
    }
    void write(const wchar_t* key,int n){
        WritePrivateProfileStringW(L"Retro",key,std::to_wstring(n).c_str(),settings_.c_str());
    }
    std::wstring totals()const{
        return lang(L"已保存回放  ",L"Saved replays  ")+std::to_wstring(counts_[Replays])+
            lang(L"\r\n已保存截图  ",L"\r\nSaved screenshots  ")+std::to_wstring(counts_[Screenshots])+
            lang(L"\r\n已完成导出  ",L"\r\nCompleted exports  ")+std::to_wstring(counts_[Exports]);
    }
    std::wstring achievements()const{
        std::wstring value=lang(L"成就",L"Achievements");
        auto add=[&](bool done,const wchar_t* zh,const wchar_t* en){
            value+=std::wstring(done?L"\r\n[x] ":L"\r\n[ ] ")+lang(zh,en);
        };
        add(counts_[Replays]>0,L"第一段回放",L"First replay");
        add(counts_[Screenshots]>0,L"桌面留影",L"First screenshot");
        add(counts_[Exports]>0,L"剪辑完成",L"First export");
        if(unlocked_)add(true,L"找到 DOS",L"DOS discovered");
        if(played_)add(true,L"像素飞船",L"Pixel ship");
        return value;
    }
    void updateStats(){
        auto value=totals()+L"\r\n\r\n"+achievements();
        SetWindowTextW(stats_,value.c_str());
    }
    void out(const std::wstring& text){
        auto all=read(output_)+text;
        if(all.size()>18000)all.erase(0,all.size()-16000);
        SetWindowTextW(output_,all.c_str());
        SendMessageW(output_,EM_SETSEL,all.size(),all.size());
        SendMessageW(output_,EM_SCROLLCARET,0,0);
    }
    static LRESULT CALLBACK pixelProc(HWND h,UINT m,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data){
        auto& self=*reinterpret_cast<RetroExtras*>(data);
        if(m==WM_TIMER&&w==AnimationTimer){
            if(++self.frame_>=70)KillTimer(h,AnimationTimer);
            InvalidateRect(h,nullptr,FALSE);return 0;
        }
        if(m==WM_PAINT||m==WM_PRINTCLIENT){
            PAINTSTRUCT ps{};HDC dc=m==WM_PAINT?BeginPaint(h,&ps):reinterpret_cast<HDC>(w);
            RECT r{};GetClientRect(h,&r);lr_ui::fill(dc,r,RGB(5,13,27));
            int mid=r.bottom/2;const int span=std::max(1,int(r.right)-35);
            for(int i=0;i<39;++i){
                int x=(i*59+11)%span, y=(i*23+7)%std::max(1,int(r.bottom)-5);
                lr_ui::fill(dc,{x,y,x+2,y+2},i%3?RGB(107,145,171):RGB(232,242,255));
            }
            int x=8+std::min(69,self.frame_)*std::max(0,int(r.right)-50)/69;
            auto part=[&](int a,int b,int c,int d,COLORREF color){lr_ui::fill(dc,{x+a,mid+b,x+c,mid+d},color);};
            part(4,-4,33,5,RGB(193,229,243));part(12,-10,23,11,RGB(111,183,224));
            part(20,-3,26,3,RGB(19,79,153));part(31,-2,36,3,RGB(242,249,252));
            if(self.frame_<70){part(0,-2,7,3,RGB(255,139,61));if(self.frame_%2)part(-5,0,1,3,RGB(255,229,106));}
            if(m==WM_PAINT)EndPaint(h,&ps);return 0;
        }
        if(m==WM_NCDESTROY){KillTimer(h,AnimationTimer);RemoveWindowSubclass(h,pixelProc,1);}
        return DefSubclassProc(h,m,w,l);
    }
    void commandLine(){
        auto typed=read(input_);SetWindowTextW(input_,L"");
        auto first=typed.find_first_not_of(L" \t"),last=typed.find_last_not_of(L" \t");
        if(first==std::wstring::npos)return;
        typed=typed.substr(first,last-first+1);
        auto cmd=typed;CharUpperBuffW(cmd.data(),DWORD(cmd.size()));
        if(cmd==L"CLS"){SetWindowTextW(output_,L"");return;}
        out(L"C:\\LITEREPLAY>"+typed+L"\r\n");
        if(cmd==L"HELP")out(L"HELP  DIR  VER  STATS  ACHIEVE  CLS  EXIT\r\n");
        else if(cmd==L"DIR")out(L"ABOUT.TXT     STATS.TXT     PIXEL.EXE\r\n");
        else if(cmd==L"VER")out(L"LiteReplay Win98 Edition 1.6 · Yuzi_Lj\r\n");
        else if(cmd==L"STATS")out(totals()+L"\r\n");
        else if(cmd==L"ACHIEVE")out(achievements()+L"\r\n");
        else if(cmd==L"TYPE ABOUT.TXT")out(L"LiteReplay GameBox 98 · Yuzi_Lj\r\n");
        else if(cmd==L"PIXEL"||cmd==L"PIXEL.EXE"||cmd==L"RUN PIXEL"){
            frame_=0;played_=true;write(L"PixelAnimation",1);
            ShowWindow(animation_,SW_SHOWNA);SetTimer(animation_,AnimationTimer,42,nullptr);
            InvalidateRect(animation_,nullptr,FALSE);updateStats();
            out(lang(L"像素飞船已启动。\r\n",L"Pixel ship launched.\r\n"));
        }else if(cmd==L"EXIT"){closePrompt_();return;}
        else out(lang(L"未知命令。输入 HELP 查看命令。\r\n",L"Unknown command. Type HELP for commands.\r\n"));
        SetFocus(input_);
    }
public:
    RetroExtras(HWND about,HWND prompt,HFONT font,const std::filesystem::path& settings,
                std::function<void(int)> openPage,std::function<void()> closePrompt)
        :about_(about),prompt_(prompt),font_(font),settings_(settings),openPage_(std::move(openPage)),closePrompt_(std::move(closePrompt)){
        constexpr const wchar_t* keys[]={L"Replays",L"Screenshots",L"Exports"};
        for(int i=0;i<3;++i)counts_[i]=std::clamp(int(GetPrivateProfileIntW(L"Retro",keys[i],0,settings_.c_str())),0,INT_MAX);
        unlocked_=GetPrivateProfileIntW(L"Retro",L"DosUnlocked",0,settings_.c_str())!=0;
        played_=GetPrivateProfileIntW(L"Retro",L"PixelAnimation",0,settings_.c_str())!=0;
        details_=add(about_,L"STATIC",L"",0,22,20,431,49);
        SendMessageW(details_,WM_SETFONT,reinterpret_cast<WPARAM>(lr_ui::classicHeading),FALSE);
        version_=add(about_,L"BUTTON",L"Version 1.0",Version,22,81,135,29,WS_TABSTOP);
        open_=add(about_,L"BUTTON",L"DOS",OpenPrompt,173,81,106,29,WS_TABSTOP);
        avatarView_=add(about_,L"STATIC",L"",0,295,133,165,118,SS_BITMAP|SS_CENTERIMAGE|SS_SUNKEN);
        avatar_=reinterpret_cast<HBITMAP>(LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(IDB_GITHUB_AVATAR),
            IMAGE_BITMAP,0,0,LR_CREATEDIBSECTION));
        if(avatar_)SendMessageW(avatarView_,STM_SETIMAGE,IMAGE_BITMAP,reinterpret_cast<LPARAM>(avatar_));
        add(about_,L"BUTTON",L"",GitHubProfile,295,260,165,29,WS_TABSTOP);
        add(about_,L"BUTTON",L"",GitHubDownloads,295,296,165,29,WS_TABSTOP);
        stats_=add(about_,L"STATIC",L"",0,24,133,255,207);
        lr_ui::sections[about_]={{11,9,475,118},{11,122,475,355}};
        mono_=lr_ui::makeFont(15,FW_NORMAL,L"Consolas");
        output_=add(prompt_,L"EDIT",L"",0,13,13,616,215,ES_MULTILINE|ES_READONLY|WS_VSCROLL|WS_BORDER|WS_TABSTOP);
        input_=add(prompt_,L"EDIT",L"",0,13,238,503,28,ES_AUTOHSCROLL|WS_BORDER|WS_TABSTOP);
        SendMessageW(input_,EM_SETLIMITTEXT,95,0);
        SendMessageW(output_,WM_SETFONT,reinterpret_cast<WPARAM>(mono_),FALSE);
        SendMessageW(input_,WM_SETFONT,reinterpret_cast<WPARAM>(mono_),FALSE);
        run_=add(prompt_,L"BUTTON",L"",Run,526,238,103,28,WS_TABSTOP);
        animation_=add(prompt_,L"STATIC",L"",0,13,279,616,92,SS_SUNKEN);
        SetWindowSubclass(animation_,pixelProc,1,reinterpret_cast<DWORD_PTR>(this));
        ShowWindow(animation_,SW_HIDE);
        SetWindowTextW(output_,L"LiteReplay DOS 1.0\r\nType HELP to list internal commands.\r\n\r\n");
    }
    ~RetroExtras(){
        if(IsWindow(animation_)){KillTimer(animation_,AnimationTimer);RemoveWindowSubclass(animation_,pixelProc,1);}
        if(avatar_){
            if(IsWindow(avatarView_))SendMessageW(avatarView_,STM_SETIMAGE,IMAGE_BITMAP,0);
            DeleteObject(avatar_);
        }
        if(mono_)DeleteObject(mono_);
    }
    void localize(bool english){
        english_=english;
        SetWindowTextW(details_,L"LiteReplay Win98 Edition\r\nYuzi_Lj");
        SetWindowTextW(version_,lang(L"版本 1.6",L"Version 1.6"));
        SetDlgItemTextW(about_,GitHubProfile,L"GitHub · @MikaW21");
        SetDlgItemTextW(about_,GitHubDownloads,lang(L"GitHub 版本下载",L"GitHub downloads"));
        SetWindowTextW(run_,lang(L"运行",L"Run"));
        ShowWindow(open_,unlocked_?SW_SHOWNA:SW_HIDE);
        updateStats();
    }
    void addEvent(Event event){
        constexpr const wchar_t* keys[]={L"Replays",L"Screenshots",L"Exports"};
        if(counts_[event]<INT_MAX)++counts_[event];
        write(keys[event],counts_[event]);updateStats();
    }
    bool command(int id,int notification){
        if(notification!=BN_CLICKED)return false;
        if(id==GitHubProfile||id==GitHubDownloads){
            const wchar_t* url=id==GitHubProfile?L"https://github.com/MikaW21":L"https://github.com/MikaW21/LiteReplay/releases";
            if(reinterpret_cast<INT_PTR>(ShellExecuteW(about_,L"open",url,nullptr,nullptr,SW_SHOWNORMAL))<=32)
                MessageBoxW(about_,lang(L"无法打开 GitHub 链接。",L"Could not open the GitHub link."),L"GitHub",MB_ICONWARNING);
            return true;
        }
        if(id==Version){
            const auto now=GetTickCount64();
            if(now-lastClick_>1800)clicks_=0;
            lastClick_=now;
            if(++clicks_>=5){
                clicks_=0;unlocked_=true;write(L"DosUnlocked",1);
                localize(english_);openPage_(RetroDesktop::Prompt);
            }
            return true;
        }
        if(id==OpenPrompt&&unlocked_){openPage_(RetroDesktop::Prompt);return true;}
        if(id==Run){commandLine();return true;}
        return false;
    }
    bool key(const MSG& msg){
        if(msg.hwnd==input_&&msg.message==WM_KEYDOWN&&msg.wParam==VK_RETURN){commandLine();return true;}
        return false;
    }
    void promptVisibility(bool show){
        if(show)SetFocus(input_);
        else KillTimer(animation_,AnimationTimer);
    }
    bool unlocked()const{return unlocked_;}
    HBRUSH terminalColor(UINT msg,WPARAM w,LPARAM l)const{
        if((msg!=WM_CTLCOLOREDIT&&msg!=WM_CTLCOLORSTATIC)||
            (reinterpret_cast<HWND>(l)!=output_&&reinterpret_cast<HWND>(l)!=input_))return nullptr;
        auto dc=reinterpret_cast<HDC>(w);SetTextColor(dc,RGB(195,241,193));SetBkColor(dc,RGB(0,0,0));
        return reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    }
};
}
