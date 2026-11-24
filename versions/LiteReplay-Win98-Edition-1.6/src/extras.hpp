#pragma once
#include "performance.hpp"
#include "ui_theme.hpp"
#include <commdlg.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <deque>
namespace lr {
// A normal layered desktop window: no hooks, injection, or game FPS claims.
class Extras {
    HWND overlay_=nullptr,panel_=nullptr;
    HFONT font_=nullptr;
    std::filesystem::path ini_,sound_;
    PerformanceSnapshot data_;
    std::deque<PerformanceSnapshot> history_;
    bool recording_=false;
    static constexpr const wchar_t* names_[10]={L"01 极简横条",L"02 经典绿字",L"03 冰蓝卡片",L"04 琥珀大字",L"05 紫色详情",L"06 资源进度条",L"07 三列仪表",L"08 60 秒趋势",L"09 录制诊断",L"10 透明白字"};
    int style_=0,corner_=0,alpha_=220;
    bool visible_=false,audible_=true;
    std::wstring custom_;
    static Extras* self(HWND h){return reinterpret_cast<Extras*>(GetWindowLongPtrW(h,GWLP_USERDATA));}
    static std::wstring pct(double n){if(n<0)return L"--";return std::to_wstring(int(n+0.5))+L"%";}
    double ram()const{return data_.ramValid&&data_.totalRam?100.0*data_.usedRam/data_.totalRam:-1;}
    void persist(){
        auto put=[&](const wchar_t* k,const std::wstring& v){if(!WritePrivateProfileStringW(L"Extras",k,v.c_str(),ini_.c_str()))throw std::runtime_error("无法保存悬浮窗设置");};
        put(L"Style",std::to_wstring(style_));put(L"Corner",std::to_wstring(corner_));put(L"Alpha",std::to_wstring(alpha_));
        put(L"Visible",std::to_wstring(visible_));put(L"Sound",std::to_wstring(audible_));put(L"Wav",custom_);
    }
    void place(){
        if(!visible_){ShowWindow(overlay_,SW_HIDE);return;}
        const int width=style_==0?430:style_==7?410:350;
        const int height=style_==0?48:style_==4?230:style_==7?210:style_==8?210:150;
        MONITORINFO mi{sizeof mi};GetMonitorInfoW(MonitorFromWindow(GetForegroundWindow(),MONITOR_DEFAULTTOPRIMARY),&mi);
        const RECT r=mi.rcMonitor;
        int x=(corner_%2)?r.right-width-18:r.left+18;
        int y=(corner_/2)?r.bottom-height-18:r.top+18;
        SetLayeredWindowAttributes(overlay_,RGB(1,2,3),BYTE(alpha_),LWA_ALPHA|(style_==9?LWA_COLORKEY:0));
        SetWindowPos(overlay_,HWND_TOPMOST,x,y,width,height,SWP_NOACTIVATE|SWP_SHOWWINDOW);
        InvalidateRect(overlay_,nullptr,FALSE);
    }
    void paint(HWND h){
        PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);
        HDC mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,r.right,r.bottom);auto oldBmp=SelectObject(mem,bmp);
        const COLORREF colors[]={RGB(240,240,240),RGB(80,255,120),RGB(90,215,255),RGB(255,195,65),RGB(205,155,255),RGB(100,220,180),RGB(160,210,255),RGB(100,230,210),RGB(255,150,110),RGB(255,255,255)};
        HBRUSH brush=CreateSolidBrush(style_==9?RGB(1,2,3):RGB(20,23,30));FillRect(mem,&r,brush);DeleteObject(brush);
        HFONT f=CreateFontW(style_==3?26:18,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,DEFAULT_QUALITY,0,L"Microsoft YaHei UI");
        auto oldFont=SelectObject(mem,f);SetBkMode(mem,TRANSPARENT);SetTextColor(mem,colors[style_]);
        auto line=[&](int x,int y,const std::wstring& s){TextOutW(mem,x,y,s.c_str(),int(s.size()));};
        const std::wstring cpu=L"CPU "+pct(data_.systemCpu),gpu=L"GPU "+pct(data_.systemGpu),memory=L"RAM "+pct(ram());
        if(style_==0)line(12,13,cpu+L"   |   "+gpu+L"   |   "+memory);
        else {
            line(12,10,recording_?L"LiteReplay  ·  正在缓存":L"LiteReplay  ·  回放关闭");
            if(style_==6){line(12,55,L"CPU");line(125,55,L"GPU");line(240,55,L"RAM");line(12,90,pct(data_.systemCpu));line(125,90,pct(data_.systemGpu));line(240,90,pct(ram()));}
            else if(style_==5){
                double values[]={data_.systemCpu,data_.systemGpu,ram()};const wchar_t* labels[]={L"CPU",L"GPU",L"RAM"};
                for(int i=0;i<3;++i){int y=42+i*32;line(12,y,std::wstring(labels[i])+L" "+pct(values[i]));RECT bar{130,y+5,325,y+19};HBRUSH b=CreateSolidBrush(RGB(50,55,65));FillRect(mem,&bar,b);DeleteObject(b);if(values[i]>=0){bar.right=bar.left+int(195*std::clamp(values[i],0.0,100.0)/100);b=CreateSolidBrush(colors[style_]);FillRect(mem,&bar,b);DeleteObject(b);}}
            }else if(style_==7){
                line(12,40,cpu+L"   "+gpu+L"   "+memory);line(12,180,L"CPU 白 / GPU 青 / RAM 黄 · 60 秒");
                const COLORREF c[]={RGB(240,240,240),RGB(80,220,210),RGB(255,210,90)};
                for(int channel=0;channel<3;++channel){HPEN pen=CreatePen(PS_SOLID,1,c[channel]);auto old=SelectObject(mem,pen);bool started=false;int i=0;for(const auto& v:history_){double n=channel==0?v.systemCpu:channel==1?v.systemGpu:(v.ramValid&&v.totalRam?100.0*v.usedRam/v.totalRam:-1);int x=12+i++*6;if(n<0){started=false;continue;}int y=165-int(n*0.9);if(started)LineTo(mem,x,y);else MoveToEx(mem,x,y,nullptr);started=true;}SelectObject(mem,old);DeleteObject(pen);}
            }else{
                int step=style_==3?32:27;line(12,42,cpu);line(12,42+step,gpu);line(12,42+step*2,memory);
                if(style_==4||style_==8){line(12,130,L"本程序 CPU "+pct(data_.processCpu));line(12,157,L"本程序编码 GPU "+pct(data_.encodeGpu));line(12,184,L"本程序内存 "+(data_.processRamValid?std::to_wstring(data_.privateRam/1048576)+L" MiB":L"--"));}
            }
        }
        BitBlt(dc,0,0,r.right,r.bottom,mem,0,0,SRCCOPY);SelectObject(mem,oldFont);DeleteObject(f);SelectObject(mem,oldBmp);DeleteObject(bmp);DeleteDC(mem);EndPaint(h,&ps);
    }
    static LRESULT CALLBACK overlayProc(HWND h,UINT m,WPARAM w,LPARAM l){
        if(m==WM_NCCREATE)SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams));
        auto p=self(h);if(p&&m==WM_PAINT){p->paint(h);return 0;}if(m==WM_ERASEBKGND)return 1;if(m==WM_NCHITTEST)return HTTRANSPARENT;if(m==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
        return DefWindowProcW(h,m,w,l);
    }
    static LRESULT CALLBACK panelProc(HWND h,UINT m,WPARAM w,LPARAM l){
        if(m==WM_NCCREATE)SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams));
        auto p=self(h);if(!p)return DefWindowProcW(h,m,w,l);
        if(m==WM_ERASEBKGND){lr_ui::paint(h,reinterpret_cast<HDC>(w));return 1;}
        if(m==WM_CTLCOLORSTATIC||m==WM_CTLCOLORLISTBOX||m==WM_CTLCOLORBTN)return lr_ui::color(h,m,w,l);
        if(m==WM_CLOSE){ShowWindow(h,SW_HIDE);return 0;}
        if(m==WM_COMMAND){try{
            int id=LOWORD(w);
            if(id==1)p->visible_=SendDlgItemMessageW(h,1,BM_GETCHECK,0,0)==BST_CHECKED;
            if(id==2&&HIWORD(w)==CBN_SELCHANGE)p->style_=int(SendDlgItemMessageW(h,2,CB_GETCURSEL,0,0));
            if(id==3&&HIWORD(w)==CBN_SELCHANGE)p->corner_=int(SendDlgItemMessageW(h,3,CB_GETCURSEL,0,0));
            if(id==4)p->audible_=SendDlgItemMessageW(h,4,BM_GETCHECK,0,0)==BST_CHECKED;
            if(id==5){wchar_t path[32768]{};OPENFILENAMEW of{sizeof of};of.hwndOwner=h;of.lpstrFilter=L"WAV 提示音\0*.wav\0\0";of.lpstrFile=path;of.nMaxFile=32768;of.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;if(GetOpenFileNameW(&of))p->custom_=path;}
            if(id==6)p->play(true);
            if(id==7)p->custom_.clear();
            if(id==8&&HIWORD(w)==CBN_SELCHANGE)p->alpha_=int(SendDlgItemMessageW(h,8,CB_GETCURSEL,0,0))==0?160:int(SendDlgItemMessageW(h,8,CB_GETCURSEL,0,0))==1?220:255;
            p->persist();SetDlgItemTextW(h,9,p->custom_.empty()?L"当前：内置原创双音":p->custom_.c_str());p->place();PostMessageW(FindWindowW(L"LiteReplayWindow",nullptr),WM_APP+3,0,0);
        }catch(const std::exception& e){MessageBoxW(h,wide(e.what()).c_str(),L"LiteReplay",MB_ICONERROR);}return 0;}
        return DefWindowProcW(h,m,w,l);
    }
public:
    Extras(const std::filesystem::path& ini,HFONT font):font_(font),ini_(ini){
        auto get=[&](const wchar_t* key,int value,int lo,int hi){return std::clamp(int(GetPrivateProfileIntW(L"Extras",key,value,ini_.c_str())),lo,hi);};
        style_=get(L"Style",0,0,9);corner_=get(L"Corner",0,0,3);alpha_=get(L"Alpha",220,100,255);visible_=get(L"Visible",0,0,1);audible_=get(L"Sound",1,0,1);
        wchar_t path[32768];GetPrivateProfileStringW(L"Extras",L"Wav",L"",path,32768,ini_.c_str());custom_=path;
        GetModuleFileNameW(nullptr,path,32768);sound_=std::filesystem::path(path).parent_path()/L"save.wav";
        WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=overlayProc;wc.lpszClassName=L"LiteReplayOverlay13";
        if(!RegisterClassW(&wc))throw std::runtime_error("悬浮窗注册失败");
        overlay_=CreateWindowExW(WS_EX_TOPMOST|WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,wc.lpszClassName,L"LiteReplay OSD",WS_POPUP,0,0,430,150,nullptr,nullptr,wc.hInstance,this);
        if(!overlay_)throw std::runtime_error("悬浮窗创建失败");
        place();
    }
    ~Extras(){PlaySoundW(nullptr,nullptr,0);if(panel_)DestroyWindow(panel_);if(overlay_)DestroyWindow(overlay_);}
    bool visible()const{return visible_;}
    void toggle(){visible_=!visible_;if(panel_)SendDlgItemMessageW(panel_,1,BM_SETCHECK,visible_?BST_CHECKED:BST_UNCHECKED,0);place();persist();}
    void update(PerformanceSnapshot s,bool active){data_=s;recording_=active;if(visible_){if(history_.empty()||history_.back().sampledAt!=s.sampledAt){if(!history_.empty()&&s.sampledAt-history_.back().sampledAt>2500)history_.clear();history_.push_back(s);if(history_.size()>60)history_.pop_front();}place();}}
    void play(bool preview=false){if(!audible_&&!preview)return;const auto path=custom_.empty()?sound_.wstring():custom_;if(!PlaySoundW(path.c_str(),nullptr,SND_FILENAME|SND_ASYNC|SND_NODEFAULT)){if(preview)MessageBoxW(panel_,L"无法播放该 WAV。请使用 PCM WAV 或恢复内置声音。",L"提示音",MB_ICONWARNING);else MessageBeep(MB_OK);}}
    void show(){
        if(!panel_){WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=panelProc;wc.lpszClassName=L"LiteReplayExtras13";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);RegisterClassW(&wc);
            panel_=CreateWindowExW(0,wc.lpszClassName,L"LiteReplay 1.3 · 游戏悬浮窗与提示音",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU, CW_USEDEFAULT,CW_USEDEFAULT,590,470,nullptr,nullptr,wc.hInstance,this);if(!panel_)throw std::runtime_error("设置窗口创建失败");
            auto item=[&](const wchar_t* cls,const wchar_t* title,DWORD flags,int x,int y,int width,int height,int id){auto h=CreateWindowExW(0,cls,title,WS_CHILD|WS_VISIBLE|flags,x,y,width,height,panel_,reinterpret_cast<HMENU>(INT_PTR(id)),wc.hInstance,nullptr);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font_),TRUE);return h;};
            item(L"BUTTON",L"显示游戏性能悬浮窗（Ctrl+Shift+F9 切换）",BS_AUTOCHECKBOX|WS_TABSTOP,18,16,530,28,1);SendDlgItemMessageW(panel_,1,BM_SETCHECK,visible_?BST_CHECKED:BST_UNCHECKED,0);
            auto combo=[&](int id,int x,int y,int width,const std::vector<std::wstring>& values,int selected){auto h=item(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,x,y,width,300,id);for(auto& v:values)SendMessageW(h,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(v.c_str()));SendMessageW(h,CB_SETCURSEL,selected,0);};
            std::vector<std::wstring> names;for(auto n:names_)names.emplace_back(n);combo(2,18,60,270,names,style_);combo(3,305,60,240,{L"左上",L"右上",L"左下",L"右下"},corner_);
            combo(8,18,105,270,{L"透明度：较透明",L"透明度：默认",L"透明度：不透明"},alpha_<190?0:alpha_<240?1:2);
            item(L"STATIC",L"跟随前台窗口所在屏幕；鼠标穿透，每秒刷新。\r\n用于窗口化 / 无边框游戏；独占全屏不保证显示。\r\nGPU 为全机最忙引擎；-- 表示没有有效读数。",0,18,150,540,72,0);
            item(L"BUTTON",L"录像保存成功后播放提示音",BS_AUTOCHECKBOX|WS_TABSTOP,18,240,530,28,4);SendDlgItemMessageW(panel_,4,BM_SETCHECK,audible_?BST_CHECKED:BST_UNCHECKED,0);
            item(L"BUTTON",L"导入 WAV…",WS_TABSTOP,18,280,155,30,5);item(L"BUTTON",L"试听",WS_TABSTOP,190,280,155,30,6);item(L"BUTTON",L"恢复内置声音",WS_TABSTOP,362,280,180,30,7);
            item(L"STATIC",custom_.empty()?L"当前：内置原创双音":custom_.c_str(),SS_PATHELLIPSIS,18,325,530,28,9);
            item(L"STATIC",L"可导入你已有的 Discord 提示音 WAV；设置即时保存。\r\n系统声音录制可能将提示音收进后续回放。",0,18,365,540,50,0);
            lr_ui::apply(panel_);lr_ui::caption(panel_);
        }ShowWindow(panel_,SW_SHOWNORMAL);SetForegroundWindow(panel_);
    }
};
}
