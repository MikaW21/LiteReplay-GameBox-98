#pragma once
#include "performance.hpp"
#include "frame_sampler.hpp"
#include "process_target.hpp"
#include <windowsx.h>
#include <cwchar>

namespace lr {
inline constexpr UINT OverlayChanged=WM_APP+10;
class DesktopPerformance {
    enum {Toggle=1100,Slot,Items,Include,Up,Down,FontSize,Opacity,Columns,Lock,Save,Reset,
          Processes,Reload,Priority,Cores,Apply,Restore,Frames,EditLayout,GroupedLayout,LayoutHint,VisualStyle};
    static constexpr std::array<const wchar_t*,9> zh_={
        L"CPU 使用率／温度／频率",L"逻辑核心使用率",L"GPU 使用率／温度／频率",
        L"物理内存",L"显存",L"FPS／帧时间／1% low",L"磁盘读写",L"网络收发",L"所选进程"};
    static constexpr std::array<const wchar_t*,9> en_={
        L"CPU usage / temperature / clock",L"Logical CPUs",L"GPU usage / temperature / clock",
        L"Physical memory",L"VRAM",L"FPS / frame time / 1% low",L"Disk read / write",L"Network down / up",L"Selected process"};
    static constexpr std::array<DWORD,6> priorities_={
        IDLE_PRIORITY_CLASS,BELOW_NORMAL_PRIORITY_CLASS,NORMAL_PRIORITY_CLASS,
        ABOVE_NORMAL_PRIORITY_CLASS,HIGH_PRIORITY_CLASS,REALTIME_PRIORITY_CLASS};
    struct Preset {
        struct Position {int x=INT_MIN,y=INT_MIN;bool set()const{return x!=INT_MIN&&y!=INT_MIN;}};
        std::array<int,9> order{0,2,3,5,4,1,6,7,8};
        std::array<Position,9> positions{};
        unsigned mask=61;
        int font=16,alpha=90,columns=1,x=INT_MIN,y=INT_MIN,style=0;
        bool locked=true,freeLayout=false;
    };
    struct Tile {
        DesktopPerformance* monitor=nullptr;
        int index=0;
        HWND window=nullptr;
        POINT grip{};
        bool dragging=false;
        int alpha=-1;
    };
    HWND owner_,page_,overlay_{};
    HFONT uiFont_,overlayFont_{};
    std::filesystem::path settings_,frameExe_;
    Preset preset_;
    int slot_=0,frameHeight_=0,frameWidth_=0;
    bool english_=false,frameWanted_=false,moving_=false,shown_=false,editing_=false;
    std::array<Tile,9> tiles_{};
    std::wstring processName_;
    ProcessTarget process_;
    FrameSampler frames_; FrameStats frameStats_;
    std::vector<ProcessTarget::Entry> processes_;
    std::vector<unsigned> cpuBits_;
    std::array<std::wstring,9> values_;
    std::vector<std::pair<HWND,std::pair<const wchar_t*,const wchar_t*>>> labels_;
    const wchar_t* lang(const wchar_t* zh,const wchar_t* en)const{return english_?en:zh;}
    HWND item(int id)const{return GetDlgItem(page_,id);}
    LRESULT send(int id,UINT message,WPARAM w=0,LPARAM l=0)const{return SendMessageW(item(id),message,w,l);}
    void checked(int id,bool yes){send(id,BM_SETCHECK,yes?BST_CHECKED:BST_UNCHECKED);}
    HWND add(const wchar_t* cls,int id,const wchar_t* zh,const wchar_t* en,int x,int y,int w,int h,DWORD style=0){
        auto control=CreateWindowExW(0,cls,lang(zh,en),WS_CHILD|WS_VISIBLE|style,x,y,w,h,page_,
            reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
        if(!control)throw std::runtime_error("Cannot create performance control");
        SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(uiFont_),TRUE);
        if(*zh)labels_.push_back({control,{zh,en}});
        return control;
    }
    void label(const wchar_t* zh,const wchar_t* en,int x,int y,int w){add(L"STATIC",0,zh,en,x,y,w,22);}
    void button(int id,const wchar_t* zh,const wchar_t* en,int x,int y,int w){
        add(L"BUTTON",id,zh,en,x,y,w,30,WS_TABSTOP);
    }
    void combo(int id,int x,int y,int w){
        add(WC_COMBOBOXW,id,L"",L"",x,y,w,300,CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP);
    }
    void flag(int id,const wchar_t* zh,const wchar_t* en,int x,int y,int w){
        add(L"BUTTON",id,zh,en,x,y,w,26,BS_AUTOCHECKBOX|WS_TABSTOP);
    }
    void option(int id,const std::wstring& value){send(id,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(value.c_str()));}
    std::wstring section()const{return L"PerformanceOverlay"+std::to_wstring(slot_+1);}
    void save(){
        const auto name=section();
        auto put=[&](const wchar_t* key,int n){
            if(!WritePrivateProfileStringW(name.c_str(),key,std::to_wstring(n).c_str(),settings_.c_str()))
                throw std::runtime_error("Cannot save overlay preset");
        };
        put(L"Style",preset_.style);put(L"Mask",preset_.mask);put(L"Font",preset_.font);put(L"Opacity",preset_.alpha);
        put(L"Columns",preset_.columns);put(L"Locked",preset_.locked);
        put(L"X",preset_.x);put(L"Y",preset_.y);
        put(L"FreeLayout",preset_.freeLayout);
        for(int i=0;i<9;++i){
            const auto index=std::to_wstring(i);
            put((L"Row"+index).c_str(),preset_.order[i]);
            put((L"ItemX"+index).c_str(),preset_.positions[i].x);
            put((L"ItemY"+index).c_str(),preset_.positions[i].y);
        }
    }
    void load(){
        preset_={};
        if(slot_==3){preset_.style=1;preset_.mask=45;}
        if(slot_==4){preset_.style=2;preset_.mask=32;preset_.font=24;}
        if(slot_==5){preset_.style=3;preset_.mask=511;preset_.columns=2;preset_.font=14;}
        if(slot_==6){preset_.style=4;preset_.mask=285;preset_.alpha=100;}
        if(slot_==7){preset_.style=0;preset_.mask=224;preset_.font=18;}
        auto name=section();
        auto read=[&](const wchar_t* key,int fallback){return int(GetPrivateProfileIntW(name.c_str(),key,fallback,settings_.c_str()));};
        auto position=[&](const wchar_t* key){
            wchar_t raw[32]{};
            GetPrivateProfileStringW(name.c_str(),key,L"",raw,32,settings_.c_str());
            wchar_t* end=nullptr;
            auto value=std::wcstoll(raw,&end,10);
            return end!=raw&&!*end&&value>=INT_MIN&&value<=INT_MAX?int(value):INT_MIN;
        };
        preset_.style=std::clamp(read(L"Style",preset_.style),0,4); preset_.mask=unsigned(read(L"Mask",preset_.mask))&511;
        preset_.font=std::clamp(read(L"Font",preset_.font),12,32);
        preset_.alpha=std::clamp(read(L"Opacity",preset_.alpha),20,100);
        preset_.columns=std::clamp(read(L"Columns",preset_.columns),1,2);
        preset_.locked=read(L"Locked",1)!=0;
        preset_.x=position(L"X");preset_.y=position(L"Y");
        preset_.freeLayout=read(L"FreeLayout",0)!=0;
        for(int i=0;i<9;++i){
            const auto index=std::to_wstring(i);
            preset_.positions[i]={position((L"ItemX"+index).c_str()),position((L"ItemY"+index).c_str())};
        }
        auto order=preset_.order;
        unsigned found=0;
        for(int i=0;i<9;++i){
            order[i]=read((L"Row"+std::to_wstring(i)).c_str(),order[i]);
            if(order[i]<0||order[i]>8||(found&(1u<<order[i])))return;
            found|=1u<<order[i];
        }
        preset_.order=order;
    }
    void syncItems(int row=0){
        send(Items,LB_RESETCONTENT);
        for(auto index:preset_.order){
            auto title=std::wstring(preset_.mask&(1u<<index)?L"✓  ":L"     ")+(english_?en_[index]:zh_[index]);
            send(Items,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(title.c_str()));
        }
        send(Items,LB_SETCURSEL,row);
        checked(Include,(preset_.mask&(1u<<preset_.order[row]))!=0);
    }
    void syncPreset(){
        send(Slot,CB_SETCURSEL,slot_);send(VisualStyle,CB_SETCURSEL,preset_.style);
        send(FontSize,CB_SETCURSEL,(preset_.font-12)/2);
        send(Opacity,CB_SETCURSEL,(preset_.alpha-20)/10);
        send(Columns,CB_SETCURSEL,preset_.columns-1);checked(Lock,preset_.locked);
        syncItems();
        syncLayoutControls();
        updateStyle();
    }
    void syncLayoutControls(){
        SetDlgItemTextW(page_,EditLayout,editing_?lang(L"完成并保存",L"Finish and save"):
            lang(L"编辑信息位置",L"Edit metric positions"));
        SetDlgItemTextW(page_,LayoutHint,editing_?lang(L"拖动桌面上的任意信息块，然后点「完成并保存」。",
            L"Drag each desktop card, then choose Finish and save."):preset_.freeLayout?
            lang(L"自由布局已启用。编辑时可跨屏拖动。",L"Free layout. Edit to drag cards across displays."):
            lang(L"解锁可移动整块面板。",L"Unlock to move the whole panel."));
        for(int id:{Up,Down,Columns,Lock})EnableWindow(item(id),!preset_.freeLayout);
        EnableWindow(item(GroupedLayout),preset_.freeLayout);
    }
    void syncScheduling(){
        const auto value=process_.read();
        int priority=-1;
        for(int i=0;i<int(priorities_.size());++i)if(priorities_[i]==value.priority)priority=i;
        send(Priority,CB_SETCURSEL,priority);
        send(Cores,LB_RESETCONTENT);cpuBits_.clear();
        for(unsigned i=0;i<sizeof(DWORD_PTR)*8;++i){
            if(!(value.allowed&(DWORD_PTR(1)<<i)))continue;
            auto name=L"CPU "+std::to_wstring(i);
            send(Cores,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));
            send(Cores,LB_SETSEL,(value.mask&(DWORD_PTR(1)<<i))!=0,cpuBits_.size());
            cpuBits_.push_back(i);
        }
        EnableWindow(item(Cores),!cpuBits_.empty());
        const bool editable=process_.pid()!=GetCurrentProcessId()&&value.priority!=0;
        EnableWindow(item(Apply),editable);
        EnableWindow(item(Restore),editable&&process_.canRestore());
    }
    void reload(){
        const DWORD old=process_.pid();
        processes_=ProcessTarget::list();
        send(Processes,CB_RESETCONTENT);option(Processes,lang(L"系统总量（选择应用查看进程）",L"System totals (select an app to inspect)"));
        int row=0;
        for(size_t i=0;i<processes_.size();++i){
            const auto& entry=processes_[i];
            option(Processes,entry.name+L"  ·  "+std::to_wstring(entry.pid));
            if(entry.pid==old)row=int(i)+1;
        }
        send(Processes,CB_SETCURSEL,row);
        if(!row)selectProcess();
    }
    void selectProcess(){
        frames_.stop();frameStats_={};frameWanted_=false;checked(Frames,false);process_.clear();processName_.clear();
        int row=int(send(Processes,CB_GETCURSEL))-1;
        if(row>=0&&row<int(processes_.size())){
            const auto& entry=processes_[row];
            processName_=entry.name+L"  ·  PID "+std::to_wstring(entry.pid);
            if(!process_.select(entry.pid))SetDlgItemTextW(page_,1191,lang(L"进程不可访问或已退出。",L"Process is inaccessible or has exited."));
        }
        syncScheduling();
    }
    std::wstring n(double value,const wchar_t* unit,int precision=1)const{
        if(value<0)return lang(L"不可用",L"N/A");
        std::wostringstream text;text<<std::fixed<<std::setprecision(precision)<<value<<unit;return text.str();
    }
    std::wstring pair(double used,uint64_t total)const{
        if(used<0)return n(-1,L"");
        auto text=n(used/1073741824.0,L" GiB",2);
        if(total)text+=L" / "+n(total/1073741824.0,L" GiB",2)+L"  ("+n(100.0*used/total,L"%")+L")";
        return text;
    }
    std::wstring rate(double bytes)const{
        if(bytes<0)return n(-1,L"");
        return bytes>=1048576?n(bytes/1048576,L" MiB/s"):n(bytes/1024,L" KiB/s");
    }
    COLORREF background()const{const COLORREF c[]={RGB(12,17,24),RGB(192,192,192),RGB(4,20,10),RGB(24,13,36),RGB(247,248,250)};return c[preset_.style];}
    COLORREF ink()const{const COLORREF c[]={RGB(225,232,240),RGB(20,20,20),RGB(105,255,145),RGB(235,205,255),RGB(25,35,50)};return c[preset_.style];}
    COLORREF muted()const{return preset_.style==1||preset_.style==4?RGB(65,75,90):RGB(160,180,195);}
    COLORREF accent(COLORREF original)const{return preset_.style==0?original:preset_.style==2?RGB(105,255,145):preset_.style==3?RGB(225,145,255):preset_.style==1?RGB(0,0,128):RGB(25,75,130);}
    void updateStyle(){
        HFONT next=CreateFontW(-preset_.font,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,
            preset_.style==1?L"Tahoma":L"Consolas");
        if(next){if(overlayFont_)DeleteObject(overlayFont_);overlayFont_=next;}
        if(overlay_){
            auto style=GetWindowLongPtrW(overlay_,GWL_EXSTYLE);
            SetWindowLongPtrW(overlay_,GWL_EXSTYLE,preset_.locked?style|WS_EX_TRANSPARENT:style&~WS_EX_TRANSPARENT);
            SetLayeredWindowAttributes(overlay_,0,BYTE(preset_.alpha*255/100),LWA_ALPHA);
        }
        repaint(true);
    }
    std::vector<std::wstring> rows()const{
        std::vector<std::wstring> result;
        for(int index:preset_.order)if(preset_.mask&(1u<<index))result.push_back(values_[index]);
        if(result.empty())result.emplace_back(lang(L"请在性能页选择显示项目",L"Choose metrics on the Performance page"));
        return result;
    }
    int render(HDC dc,int width,bool paint){
        auto old=SelectObject(dc,overlayFont_?overlayFont_:GetStockObject(DEFAULT_GUI_FONT));
        SetBkMode(dc,TRANSPARENT);
        constexpr COLORREF accents[]={RGB(95,210,255),RGB(95,210,255),RGB(120,235,137),RGB(230,191,100),RGB(180,155,255),RGB(255,158,75),RGB(140,210,215),RGB(140,210,215),RGB(220,225,235)};
        int y=12;
        RECT title{12,y,width-12,y+preset_.font+8};
        if(paint){SetTextColor(dc,muted());DrawTextW(dc,L"LiteReplay / PERFORMANCE",-1,&title,DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);}
        y+=preset_.font+16;
        int column=0,rowHeight=0;
        int colWidth=(width-24-(preset_.columns-1)*12)/preset_.columns;
        for(int index:preset_.order){
            if(!(preset_.mask&(1u<<index)))continue;
            int x=12+column*(colWidth+12);RECT text{x,y,x+colWidth,y};
            DrawTextW(dc,values_[index].c_str(),-1,&text,DT_WORDBREAK|DT_NOPREFIX|DT_CALCRECT);
            if(paint){SetTextColor(dc,accent(accents[index]));DrawTextW(dc,values_[index].c_str(),-1,&text,DT_WORDBREAK|DT_NOPREFIX);}
            rowHeight=std::max(rowHeight,int(text.bottom-text.top)+10);
            if(++column==preset_.columns){y+=rowHeight;column=0;rowHeight=0;}
        }
        if(column)y+=rowHeight;
        if(preset_.mask&(1u<<5)){
            RECT label{12,y,width-12,y+preset_.font+6};
            if(paint){SetTextColor(dc,muted());DrawTextW(dc,lang(L"逐帧时间 / 最近 240 帧",L"Frame time / latest 240 frames"),-1,&label,DT_SINGLELINE|DT_NOPREFIX);}
            y+=preset_.font+8;
            RECT graph{12,y,width-12,y+72};
            if(paint){
                lr_ui::fill(dc,graph,background());
                double ceiling=33.34;for(double ms:frameStats_.times)ceiling=std::max(ceiling,ms);
                auto grid=CreatePen(PS_DOT,1,RGB(75,85,101));auto prior=SelectObject(dc,grid);
                int baseline=graph.bottom-int(16.667/ceiling*70);MoveToEx(dc,graph.left,baseline,nullptr);LineTo(dc,graph.right,baseline);
                SelectObject(dc,prior);DeleteObject(grid);
                auto line=CreatePen(PS_SOLID,1,accent(RGB(255,158,75)));prior=SelectObject(dc,line);
                for(size_t i=0;i<frameStats_.times.size();++i){int x=graph.left+int(i)*(graph.right-graph.left-1)/239;
                    int py=graph.bottom-1-int(frameStats_.times[i]/ceiling*70);if(i)LineTo(dc,x,py);else MoveToEx(dc,x,py,nullptr);}
                SelectObject(dc,prior);DeleteObject(line);
                RECT note{graph.left+4,graph.top+2,graph.right-4,graph.bottom};SetTextColor(dc,muted());
                auto caption=frameStats_.times.empty()?std::wstring(lang(L"选择应用并启用 FPS 采集",L"Select an app and enable FPS capture")):n(ceiling,L" ms",1)+L" / 16.7 ms = 60 FPS";
                DrawTextW(dc,caption.c_str(),-1,&note,DT_SINGLELINE|DT_NOPREFIX);
            }
            y+=80;
        }
        SelectObject(dc,old);return y+6;
    }    RECT workArea(Preset::Position p)const{
        auto monitor=p.set()?MonitorFromPoint({p.x,p.y},MONITOR_DEFAULTTONEAREST):
            MonitorFromWindow(owner_,MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO info{sizeof info};
        if(GetMonitorInfoW(monitor,&info))return info.rcWork;
        return {0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN)};
    }
    int drawTile(HDC dc,int i,int width,int paintHeight=0){
        const auto& palette=lr_ui::colors();
        const int titleHeight=editing_||preset_.style==1?24:0;
        auto previous=SelectObject(dc,overlayFont_?overlayFont_:GetStockObject(DEFAULT_GUI_FONT));
        SetBkMode(dc,TRANSPARENT);SetTextColor(dc,ink());
        const auto title=english_?en_[i]:zh_[i];
        const auto value=values_[i].empty()?title:values_[i].c_str();
        RECT text{11,8+titleHeight,width-11,8+titleHeight};
        DrawTextW(dc,value,-1,&text,DT_WORDBREAK|DT_NOPREFIX|DT_CALCRECT);
        if(paintHeight){
            RECT frame{0,0,width,paintHeight};
            lr_ui::fill(dc,frame,background());
            if((preset_.style==1))lr_ui::bevel(dc,frame,false);
            else lr_ui::fill(dc,{0,0,width,2},palette.accent);
            if(titleHeight){
                RECT bar{4,4,width-4,4+titleHeight};
                if((preset_.style==1))lr_ui::classicCaption(dc,bar);
                else lr_ui::fill(dc,bar,palette.accent);
                auto old=SelectObject(dc,(preset_.style==1)?lr_ui::classicSmall:lr_ui::small);
                SetTextColor(dc,(preset_.style==1)?palette.onCaption:palette.onAccent);
                InflateRect(&bar,-7,0);
                DrawTextW(dc,title,-1,&bar,DT_SINGLELINE|DT_END_ELLIPSIS|DT_VCENTER|DT_NOPREFIX);
                SelectObject(dc,old);SetTextColor(dc,ink());
            }
            text.right=width-11;text.bottom=paintHeight-4;
            DrawTextW(dc,value,-1,&text,DT_WORDBREAK|DT_NOPREFIX);
        }
        SelectObject(dc,previous);
        return std::max(int(text.bottom)+8,titleHeight+preset_.font+16);
    }
    void finishDrag(Tile& tile){
        if(!tile.dragging)return;
        tile.dragging=false;
        RECT r{};GetWindowRect(tile.window,&r);
        MONITORINFO info{sizeof info};
        if(GetMonitorInfoW(MonitorFromWindow(tile.window,MONITOR_DEFAULTTONEAREST),&info)){
            const auto area=info.rcWork;
            r.left=std::clamp(r.left,area.left,std::max(area.left,area.right-(r.right-r.left)));
            r.top=std::clamp(r.top,area.top,std::max(area.top,area.bottom-(r.bottom-r.top)));
        }
        preset_.positions[tile.index]={int(r.left),int(r.top)};
        if(GetCapture()==tile.window)ReleaseCapture();
    }
    static LRESULT CALLBACK tileProc(HWND h,UINT msg,WPARAM w,LPARAM l){
        auto tile=reinterpret_cast<Tile*>(GetWindowLongPtrW(h,GWLP_USERDATA));
        if(msg==WM_NCCREATE){
            tile=static_cast<Tile*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
            SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(tile));
        }
        if(tile){
            auto& self=*tile->monitor;
            switch(msg){
            case WM_NCHITTEST:return self.editing_?HTCLIENT:HTTRANSPARENT;
            case WM_MOUSEACTIVATE:return self.editing_?MA_ACTIVATE:MA_NOACTIVATE;
            case WM_LBUTTONDOWN:
                if(self.editing_){
                    tile->grip={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
                    SetCapture(h);tile->dragging=GetCapture()==h;
                }
                return 0;
            case WM_MOUSEMOVE:
                if(tile->dragging&&GetCapture()==h){
                    POINT cursor{GET_X_LPARAM(l),GET_Y_LPARAM(l)};
                    if(ClientToScreen(h,&cursor))
                        SetWindowPos(h,HWND_TOPMOST,cursor.x-tile->grip.x,cursor.y-tile->grip.y,0,0,SWP_NOSIZE|SWP_NOACTIVATE);
                }
                return 0;
            case WM_LBUTTONUP:case WM_CAPTURECHANGED:case WM_CANCELMODE:
                if(tile->dragging){self.finishDrag(*tile);PostMessageW(self.owner_,OverlayChanged,0,0);}
                return 0;
            case WM_CLOSE:self.toggle();PostMessageW(self.owner_,OverlayChanged,0,0);return 0;
            case WM_DISPLAYCHANGE:PostMessageW(self.owner_,OverlayChanged,0,0);return 0;
            case WM_PAINT:{
                PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r{};GetClientRect(h,&r);
                self.drawTile(dc,tile->index,r.right,r.bottom);EndPaint(h,&ps);return 0;}
            }
        }
        return DefWindowProcW(h,msg,w,l);
    }
    HWND tileWindow(int i){
        auto& tile=tiles_[i];
        if(tile.window)return tile.window;
        WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=tileProc;
        wc.hCursor=LoadCursorW(nullptr,IDC_SIZEALL);wc.lpszClassName=L"LiteReplayMetricTile";
        if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)
            throw std::runtime_error("Cannot register metric tile");
        tile.monitor=this;tile.index=i;
        tile.window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_LAYERED|WS_EX_NOACTIVATE,
            wc.lpszClassName,english_?en_[i]:zh_[i],WS_POPUP,0,0,1,1,nullptr,nullptr,wc.hInstance,&tile);
        if(!tile.window)throw std::runtime_error("Cannot create metric tile");
        return tile.window;
    }
    void updateTiles(){
        const auto area=workArea({preset_.x,preset_.y});
        const int span=std::max(1,int(area.right-area.left)),gap=10;
        const int target=std::max(250,preset_.font*21);
        const int columns=std::clamp((span-gap)/(target+gap),1,3);
        const int gridWidth=std::min(target,std::max(1,(span-(columns+1)*gap)/columns));
        const int start=std::max(int(area.left),int(area.right)-columns*(gridWidth+gap));
        int count=0,y=area.top+gap,rowHeight=0;
        for(int i:preset_.order){
            auto& tile=tiles_[i];
            if(!(preset_.mask&(1u<<i))){
                if(tile.window){finishDrag(tile);ShowWindow(tile.window,SW_HIDE);}
                continue;
            }
            HWND window=tileWindow(i);
            const auto position=preset_.positions[i];
            const auto bounds=position.set()?workArea(position):area;
            const int width=position.set()?std::min(target,std::max(1,int(bounds.right-bounds.left))):gridWidth;
            HDC dc=GetDC(window);
            const int measured=dc?drawTile(dc,i,width):preset_.font+20;
            if(dc)ReleaseDC(window,dc);
            const int height=std::min(measured,std::max(1,int(bounds.bottom-bounds.top)));
            int x=position.set()?position.x:start+(count%columns)*(gridWidth+gap);
            int top=position.set()?position.y:y;
            x=std::clamp(x,int(bounds.left),int(bounds.right)-width);
            top=std::clamp(top,int(bounds.top),int(bounds.bottom)-height);
            rowHeight=std::max(rowHeight,height);
            if(++count%columns==0){y+=rowHeight+gap;rowHeight=0;}
            LONG_PTR style=GetWindowLongPtrW(window,GWL_EXSTYLE);
            const LONG_PTR inputFlags=WS_EX_TRANSPARENT|WS_EX_NOACTIVATE;
            const LONG_PTR next=editing_?style&~inputFlags:style|inputFlags;
            if(next!=style)SetWindowLongPtrW(window,GWL_EXSTYLE,next);
            const int alpha=editing_?100:preset_.alpha;
            if(tile.alpha!=alpha&&SetLayeredWindowAttributes(window,0,BYTE(alpha*255/100),LWA_ALPHA))
                tile.alpha=alpha;
            RECT old{};GetWindowRect(window,&old);
            if(!tile.dragging&&(!IsWindowVisible(window)||old.left!=x||old.top!=top||old.right-x!=width||old.bottom-top!=height))
                SetWindowPos(window,nullptr,x,top,width,height,SWP_NOACTIVATE|SWP_NOZORDER|SWP_SHOWWINDOW);
            InvalidateRect(window,nullptr,FALSE);
        }
    }
    void repaint(bool place=false){
        if(shown_&&preset_.freeLayout){
            if(overlay_&&IsWindowVisible(overlay_))ShowWindow(overlay_,SW_HIDE);
            updateTiles();
            return;
        }
        for(auto& tile:tiles_)if(tile.window&&IsWindowVisible(tile.window)){
            finishDrag(tile);ShowWindow(tile.window,SW_HIDE);
        }
        if(!shown_){if(overlay_)ShowWindow(overlay_,SW_HIDE);return;}
        ensureGroupedWindow();
        if(!IsWindowVisible(overlay_)){ShowWindow(overlay_,SW_SHOWNOACTIVATE);place=true;}
        if(moving_){InvalidateRect(overlay_,nullptr,FALSE);return;}
        POINT point{preset_.x,preset_.y};
        MONITORINFO monitor{sizeof monitor};
        HMONITOR screen=preset_.x==INT_MIN||preset_.y==INT_MIN?
            MonitorFromWindow(owner_,MONITOR_DEFAULTTOPRIMARY):MonitorFromPoint(point,MONITOR_DEFAULTTONEAREST);
        if(!GetMonitorInfoW(screen,&monitor))return;
        auto area=monitor.rcWork;
        int width=std::min<int>(area.right-area.left,std::max(300,preset_.font*24)*preset_.columns);
        HDC dc=GetDC(overlay_);if(!dc)return;
        int height=std::min<int>(area.bottom-area.top,render(dc,width,false));ReleaseDC(overlay_,dc);
        if(place||width!=frameWidth_||height!=frameHeight_){
            int x=preset_.x==INT_MIN?area.right-width-20:preset_.x;
            int y=preset_.y==INT_MIN?area.top+20:preset_.y;
            x=std::clamp(x,int(area.left),int(area.right)-width);
            y=std::clamp(y,int(area.top),int(area.bottom)-height);
            SetWindowPos(overlay_,HWND_TOPMOST,x,y,width,height,SWP_NOACTIVATE);frameWidth_=width;frameHeight_=height;
        }
        InvalidateRect(overlay_,nullptr,FALSE);
    }
    static LRESULT CALLBACK overlayProc(HWND h,UINT msg,WPARAM w,LPARAM l){
        auto self=reinterpret_cast<DesktopPerformance*>(GetWindowLongPtrW(h,GWLP_USERDATA));
        if(msg==WM_NCCREATE){
            self=static_cast<DesktopPerformance*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
            SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
        }
        if(self)switch(msg){
        case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
        case WM_CLOSE:self->toggle();PostMessageW(self->owner_,OverlayChanged,0,0);return 0;
        case WM_NCHITTEST:return self->preset_.locked?HTTRANSPARENT:HTCAPTION;
        case WM_ENTERSIZEMOVE:self->moving_=true;return 0;
        case WM_EXITSIZEMOVE:{
            self->moving_=false;
            RECT r{};GetWindowRect(h,&r);self->preset_.x=r.left;self->preset_.y=r.top;
            self->repaint(true);return 0;}
        case WM_DISPLAYCHANGE:self->repaint(true);return 0;
        case WM_PAINT:{
            PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r{};GetClientRect(h,&r);
            const auto& palette=lr_ui::colors();
            lr_ui::fill(dc,r,self->background());
            if(self->preset_.style==1)lr_ui::bevel(dc,r,false);
            else lr_ui::fill(dc,{0,0,r.right,2},palette.accent);
            self->render(dc,r.right,true);EndPaint(h,&ps);return 0;}
        }
        return DefWindowProcW(h,msg,w,l);
    }
    void ensureGroupedWindow(){
        if(overlay_)return;
        WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=overlayProc;
        wc.hCursor=LoadCursorW(nullptr,IDC_SIZEALL);wc.lpszClassName=L"LiteReplayPerformanceOverlay";
        if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)
            throw std::runtime_error("Cannot register performance overlay");
        DWORD style=WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_LAYERED|WS_EX_NOACTIVATE;
        if(preset_.locked)style|=WS_EX_TRANSPARENT;
        overlay_=CreateWindowExW(style,wc.lpszClassName,L"LiteReplay Performance",WS_POPUP,
            0,0,1,1,nullptr,nullptr,wc.hInstance,this);
        if(!overlay_)throw std::runtime_error("Cannot create performance overlay");
        SetLayeredWindowAttributes(overlay_,0,BYTE(preset_.alpha*255/100),LWA_ALPHA);
    }
    void captureLayout(){
        for(auto& tile:tiles_)if(tile.window&&IsWindowVisible(tile.window)){
            if(tile.dragging)finishDrag(tile);
            else{
                RECT r{};GetWindowRect(tile.window,&r);
                preset_.positions[tile.index]={int(r.left),int(r.top)};
            }
        }
    }
    void editLayout(){
        if(editing_){
            captureLayout();
            save();
            editing_=false;
            SetDlgItemTextW(page_,1192,lang(L"已保存信息块位置。",L"Metric positions saved."));
        }else{
            if(!preset_.mask){
                SetDlgItemTextW(page_,1192,lang(L"请先启用一个显示项目。",L"Enable a metric to edit its position."));
                return;
            }
            editing_=true;shown_=true;preset_.freeLayout=true;
            SetDlgItemTextW(page_,1192,L"");
            repaint(true);
            captureLayout();
        }
        syncLayoutControls();repaint(true);
    }
public:
    DesktopPerformance(HWND owner,HWND page,HFONT font,const std::filesystem::path& settings,
                       const std::filesystem::path& root,bool english)
        :owner_(owner),page_(page),uiFont_(font),settings_(settings),frameExe_(root/L"performance-tools"/L"PresentMon.exe"),english_(english){
        button(Toggle,L"显示 / 隐藏覆盖层",L"Toggle overlay",24,26,194);
        button(EditLayout,L"",L"",228,26,192);
        button(GroupedLayout,L"整块布局",L"Group metrics",430,26,144);
        label(L"Ctrl+Shift+F7",L"Ctrl+Shift+F7",600,31,175);
        label(L"布局预设",L"Layout preset",24,80,130);combo(Slot,153,76,217);
        add(L"LISTBOX",Items,L"",L"",24,116,346,180,LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|WS_VSCROLL|WS_TABSTOP|WS_BORDER);
        flag(Include,L"显示所选项目",L"Show selected item",24,303,146);
        button(Up,L"上移",L"Move up",188,302,86);button(Down,L"下移",L"Move down",282,302,88);
        label(L"字号",L"Font size",24,352,100);combo(FontSize,115,348,88);
        label(L"不透明度",L"Opacity",217,352,90);combo(Opacity,304,348,66);
        label(L"列数",L"Columns",24,389,100);combo(Columns,115,386,88);
        flag(Lock,L"锁定并鼠标穿透",L"Lock / click through",212,384,158);
        button(Save,L"保存此预设",L"Save preset",24,429,160);button(Reset,L"重置位置",L"Reset position",198,429,172);
        add(L"STATIC",LayoutHint,L"",L"",24,469,352,42);
        label(L"选择应用",L"Select app",408,80,140);
        combo(Processes,408,116,283);button(Reload,L"刷新",L"Refresh",703,114,81);
        flag(Frames,L"采集此应用的 FPS",L"Capture FPS for this app",408,156,270);
        add(L"STATIC",1191,L"",L"",408,190,376,80);
        label(L"CPU 优先级",L"CPU priority",408,278,150);combo(Priority,560,274,224);
        label(L"可用逻辑核心",L"Allowed logical CPUs",408,315,200);
        add(L"LISTBOX",Cores,L"",L"",408,344,376,78,LBS_MULTIPLESEL|LBS_NOINTEGRALHEIGHT|WS_TABSTOP|WS_VSCROLL|WS_BORDER);
        button(Apply,L"应用调度设置",L"Apply scheduling",408,429,182);
        button(Restore,L"恢复修改前设置",L"Restore original",602,429,182);
        add(L"STATIC",1192,L"",L"",408,467,376,45);
        label(L"监视器画风",L"Monitor style",24,530,130);combo(VisualStyle,153,526,300);label(L"切换后自动保存",L"Saved automatically",470,530,300);
        label(L"实时读数",L"Live readings",24,586,200);
        auto live=add(L"EDIT",1195,L"",L"",24,616,759,145,
            WS_BORDER|WS_VSCROLL|WS_TABSTOP|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL);
        SetPropW(live,L"lrField",HANDLE(1));
        for(int i=12;i<=32;i+=2)option(FontSize,std::to_wstring(i));
        for(int i=20;i<=100;i+=10)option(Opacity,std::to_wstring(i)+L"%");
        option(Columns,L"1");option(Columns,L"2");
        slot_=std::clamp(int(GetPrivateProfileIntW(L"Suite",L"PerformancePreset",0,settings_.c_str())),0,7);
        load();localize(english_);syncPreset();reload();syncScheduling();
    }
    ~DesktopPerformance(){
        shown_=false;
        for(auto& tile:tiles_)if(tile.window){finishDrag(tile);DestroyWindow(tile.window);}
        if(overlay_)DestroyWindow(overlay_);
        if(overlayFont_)DeleteObject(overlayFont_);
    }
    void localize(bool english){
        english_=english;
        for(const auto& label:labels_)SetWindowTextW(label.first,lang(label.second.first,label.second.second));
        send(Slot,CB_RESETCONTENT);
                const wchar_t* zh[]={L"自定义 1",L"自定义 2",L"自定义 3",L"经典 Win98 / 游戏",L"绿色荧光 / 帧率",L"霓虹紫 / 全部指标",L"浅色 / 工作监控",L"深色 / 网络与帧率"};
        const wchar_t* en[]={L"Custom 1",L"Custom 2",L"Custom 3",L"Win98 / Gaming",L"Phosphor / FPS",L"Neon / All metrics",L"Light / Work",L"Dark / Network and FPS"};
        for(int i=0;i<8;++i)option(Slot,english_?en[i]:zh[i]);
        send(VisualStyle,CB_RESETCONTENT);
        const wchar_t* stylesZh[]={L"RTSS 深色彩色",L"经典 Win98",L"绿色荧光",L"霓虹紫",L"极简浅色"};
        const wchar_t* stylesEn[]={L"RTSS dark",L"Classic Win98",L"Green phosphor",L"Neon purple",L"Minimal light"};
        for(int i=0;i<5;++i)option(VisualStyle,english_?stylesEn[i]:stylesZh[i]);
        send(Slot,CB_SETCURSEL,slot_);send(VisualStyle,CB_SETCURSEL,preset_.style);
        send(Priority,CB_RESETCONTENT);
        if(english_)for(auto name:{L"Low",L"Below normal",L"Normal",L"Above normal",L"High",L"Realtime (current only)"})option(Priority,name);
        else for(auto name:{L"低",L"低于正常",L"正常",L"高于正常",L"高",L"实时（仅显示当前值）"})option(Priority,name);
        if(!processes_.empty())reload();
        syncItems();syncScheduling();syncLayoutControls();
        for(int i=0;i<int(tiles_.size());++i)
            if(tiles_[i].window)SetWindowTextW(tiles_[i].window,english_?en_[i]:zh_[i]);
        SetDlgItemTextW(page_,1193,lang(L"温度与 GPU 频率取自驱动或 LibreHardwareMonitor。没有有效读数时显示「不可用」。",
            L"Temperatures and GPU clocks come from the driver or LibreHardwareMonitor. Missing readings show N/A."));
    }
    DWORD target()const{return process_.pid();}
    bool visible()const{return shown_;}
    bool capturing()const{return frameWanted_&&frames_.running();}
    void refreshAppearance(){updateStyle();}
    void toggle(){
        shown_=!shown_;
        if(!shown_&&editing_){editing_=false;syncLayoutControls();}
        repaint(true);
    }
    bool command(int id,int notification){
        if(id<Toggle||id>VisualStyle)return false; if(id==VisualStyle&&notification==CBN_SELCHANGE){int style=int(send(VisualStyle,CB_GETCURSEL));if(style>=0&&style<5){preset_.style=style;updateStyle();save();}return true;}
        if(id==Toggle&&notification==BN_CLICKED)toggle();
        else if(id==EditLayout&&notification==BN_CLICKED)editLayout();
        else if(id==GroupedLayout&&notification==BN_CLICKED){
            editing_=false;preset_.freeLayout=false;syncLayoutControls();repaint(true);
        }
        else if(id==Slot&&notification==CBN_SELCHANGE){
            auto selected=int(send(Slot,CB_GETCURSEL));
            if(selected>=0&&selected<8){
                for(auto& tile:tiles_)finishDrag(tile);
                editing_=false;
                save();slot_=selected;load();syncPreset();
                WritePrivateProfileStringW(L"Suite",L"PerformancePreset",std::to_wstring(slot_).c_str(),settings_.c_str());
            }
        }else if((id==Items&&notification==LBN_SELCHANGE)||((id==Include||id==Up||id==Down)&&notification==BN_CLICKED)){
            int row=int(send(Items,LB_GETCURSEL));
            if(row<0||row>=9)return true;
            auto bit=1u<<preset_.order[row];
            if(id==Include)preset_.mask^=bit;
            if(id==Up&&row>0){std::swap(preset_.order[row],preset_.order[row-1]);--row;}
            if(id==Down&&row<8){std::swap(preset_.order[row],preset_.order[row+1]);++row;}
            syncItems(row);repaint(true);
        }else if((id==FontSize||id==Opacity||id==Columns)&&notification==CBN_SELCHANGE){
            int index=int(send(id,CB_GETCURSEL));
            if(index>=0){
                if(id==FontSize)preset_.font=12+2*index;
                if(id==Opacity)preset_.alpha=20+10*index;
                if(id==Columns)preset_.columns=1+index;
                updateStyle();
            }
        }else if(id==Lock&&notification==BN_CLICKED){preset_.locked=send(Lock,BM_GETCHECK)==BST_CHECKED;updateStyle();}
        else if(id==Save&&notification==BN_CLICKED){save();SetDlgItemTextW(page_,1192,lang(L"已保存布局预设。",L"Layout preset saved."));}
        else if(id==Reset&&notification==BN_CLICKED){
            if(preset_.freeLayout){
                for(auto& tile:tiles_)finishDrag(tile);
                preset_.positions={};
            }else preset_.x=preset_.y=INT_MIN;
            repaint(true);
        }
        else if(id==Reload&&notification==BN_CLICKED)reload();
        else if(id==Processes&&notification==CBN_SELCHANGE)selectProcess();
        else if(id==Frames&&notification==BN_CLICKED){
            frameWanted_=send(Frames,BM_GETCHECK)==BST_CHECKED;
            if(!frameWanted_)frames_.stop();
            else if(!frames_.start(frameExe_,target())){
                SetDlgItemTextW(page_,1192,lang(L"请先选择可访问的应用并检查 PresentMon。",L"Select an accessible app and check PresentMon."));
                frameWanted_=false;checked(Frames,false);
            }else SetDlgItemTextW(page_,1192,L"");
        }else if((id==Apply||id==Restore)&&notification==BN_CLICKED){
            int priority=int(send(Priority,CB_GETCURSEL));
            DWORD_PTR mask=0;
            for(size_t i=0;i<cpuBits_.size();++i)
                if(send(Cores,LB_GETSEL,i)>0)mask|=DWORD_PTR(1)<<cpuBits_[i];
            DWORD result=ERROR_INVALID_PARAMETER;
            if(id==Restore)result=process_.apply(0,0,true);
            else if(priority>=0&&priority<int(priorities_.size())&&
                    (priorities_[priority]!=REALTIME_PRIORITY_CLASS||process_.read().priority==REALTIME_PRIORITY_CLASS)&&
                    (mask||cpuBits_.empty()))result=process_.apply(priorities_[priority],mask,false);
            auto line=result==ERROR_SUCCESS?std::wstring(lang(L"已更新进程设置。",L"Process settings updated.")):
                lang(L"设置未完成。请选择可用核心并检查权限。错误 ",L"Check core selection and permissions. Error ")+std::to_wstring(result);
            SetDlgItemTextW(page_,1192,line.c_str());syncScheduling();
        }
        return true;
    }
    void update(const PerformanceSnapshot& s,bool showing){
        auto frame=frames_.sample();
        if(frameWanted_&&!frames_.running()){
            const bool accessDenied=frames_.accessDenied();
            frameWanted_=false;checked(Frames,false);frames_.stop();frame={};
            SetDlgItemTextW(page_,1192,accessDenied?
                lang(L"Windows 拒绝 ETW 跟踪。请检查当前账户的跟踪权限。",L"Windows denied ETW tracing. Check the account's trace privileges."):
                lang(L"帧率采集已结束。请检查目标应用与跟踪权限。",L"Frame capture ended. Check the app and trace permissions."));
        }
        frameStats_=frame; if(!showing)return;
        auto readings=[&](double load,double temp,double mhz){
            return n(load,L"%")+L"  |  "+n(temp,L" °C")+L"  |  "+n(mhz,L" MHz",0);
        };
        values_[0]=L"CPU  "+readings(s.systemCpu,s.sensors.cpuTemp,s.sensors.cpuMhz);
        values_[1]=lang(L"逻辑核心",L"Logical CPUs");
        if(s.cores.empty())values_[1]+=L"  "+n(-1,L"");
        else{
            std::vector<std::pair<std::wstring,double>> cores(s.cores.begin(),s.cores.end());
            std::sort(cores.begin(),cores.end(),[](const auto& a,const auto& b){
                return CompareStringEx(LOCALE_NAME_INVARIANT,SORT_DIGITSASNUMBERS,a.first.c_str(),-1,b.first.c_str(),-1,nullptr,nullptr,0)==CSTR_LESS_THAN;
            });
            int i=0;
            for(const auto& [name,value]:cores){
                values_[1]+=(i++%3==0?L"\n":L"   ")+name+L" "+n(value,L"%",0);
            }
        }
        values_[2]=L"GPU  "+readings(s.systemGpu,s.sensors.gpuTemp,s.sensors.gpuMhz);
        values_[3]=L"RAM  "+pair(s.ramValid?double(s.usedRam):-1,s.totalRam);
        values_[4]=L"VRAM  "+pair(s.vram,s.vramTotal);
        values_[5]=L"FPS  "+n(frame.fps,L"")+L"  |  "+n(frame.ms,L" ms")+L"\n1% low  "+n(frame.low,L" FPS");
        values_[6]=lang(L"磁盘  读 ",L"Disk  R ")+rate(s.diskRead)+lang(L"  写 ",L"  W ")+rate(s.diskWrite);
        values_[7]=lang(L"网络  下 ",L"Network  ↓ ")+rate(s.netIn)+lang(L"  上 ",L"  ↑ ")+rate(s.netOut);
        const DWORD selected=target();
        auto details=processName_.empty()?std::wstring(lang(L"请选择进程查看占用。",L"Select a process to see usage.")):processName_;
        if(selected&&s.target==selected)
            details+=L"\r\nCPU "+n(s.targetCpu,L"%")+L"  GPU "+n(s.targetGpu,L"%")+L"  RAM "+
                n(s.targetRamValid?double(s.targetRam)/1048576:-1,L" MiB",0);
        else if(!processName_.empty())details+=selected?lang(L"\r\n正在采样…",L"\r\nSampling…"):
            lang(L"\r\n进程不可访问或已退出。",L"\r\nProcess is inaccessible or has exited.");
        if(!selected&&!processName_.empty()){EnableWindow(item(Apply),FALSE);EnableWindow(item(Restore),FALSE);}
        values_[8]=details;
        if(frame.fps>=0)details+=L"\r\n"+n(frame.fps,L" FPS")+L"  |  "+n(frame.ms,L" ms")+L"  |  1% low "+n(frame.low,L" FPS");
        SetDlgItemTextW(page_,1191,details.c_str());
        std::wstring preview;
        for(int index:{0,2,3,4,5,6,7,1}){
            if(!preview.empty())preview+=L"\r\n";
            for(wchar_t c:values_[index]){
                if(c==L'\n')preview+=L'\r';
                preview+=c;
            }
        }
        auto live=item(1195);
        // Leave selection and scrolling alone while the user inspects the text.
        if(GetFocus()!=live)SetWindowTextW(live,preview.c_str());
        repaint();
    }
};
}
