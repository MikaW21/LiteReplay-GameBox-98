#pragma once
#include "ui_theme.hpp"
#include <windowsx.h>
#include <functional>
#include <stdexcept>

namespace lr {
class RetroDesktop {
public:
    static constexpr int Count=9,About=7,Prompt=8;
    static constexpr int OpenBase=800,ShortcutBase=1500,TaskBase=1520,Start=1540,ShowDesktop=1541,
        CaptureTray=1542,OverlayTray=1543,ClockId=1544;
    static constexpr UINT_PTR ClockTimer=81;
private:
    struct Pane {
        RetroDesktop* desktop{};
        int index{};
        HWND window{},body{},title{},minimize{},maximize{},close{};
        RECT normal{};
        POINT start{};
        RECT startBox{};
        bool open=false,hidden=false,maximized=false,sizing=false,dragging=false,positioned=false;
    };
    static constexpr int Bar=37,Caption=26,Border=4;
    static constexpr COLORREF wallpaper=RGB(0,112,112);
    HWND root_{},start_{},showDesk_{},capture_{},overlay_{},clock_{};
    HFONT font_{},bold_{};
    HBITMAP background_{},crtBackground_{};
    bool crt_=false;
    static std::wstring assetPath(const wchar_t* name){
        wchar_t path[MAX_PATH]{};GetModuleFileNameW(nullptr,path,MAX_PATH);
        std::wstring folder(path);folder.resize(folder.find_last_of(L"\\/")+1);
        return folder+L"assets\\"+name;
    }
    void paintBackground(HDC dc,int offsetX=0,int offsetY=0)const{
        auto r=area();HBITMAP bitmap=crt_?crtBackground_:background_;
        if(!bitmap){RECT target{-offsetX,-offsetY,r.right-offsetX,r.bottom-offsetY};lr_ui::fill(dc,target,wallpaper);return;}
        BITMAP info{};GetObjectW(bitmap,sizeof(info),&info);
        HDC source=CreateCompatibleDC(dc);auto old=SelectObject(source,bitmap);
        int saved=SaveDC(dc);SetStretchBltMode(dc,HALFTONE);SetBrushOrgEx(dc,-offsetX,-offsetY,nullptr);
        int cropW=info.bmWidth,cropH=info.bmHeight; if(double(cropW)/cropH>double(r.right)/r.bottom)cropW=int(double(cropH)*r.right/r.bottom);else cropH=int(double(cropW)*r.bottom/r.right); StretchBlt(dc,-offsetX,-offsetY,r.right,r.bottom,source,(info.bmWidth-cropW)/2,(info.bmHeight-cropH)/2,cropW,cropH,SRCCOPY);
        RestoreDC(dc,saved);SelectObject(source,old);DeleteDC(source);
    }
    std::array<Pane,Count> panes_{};
    std::array<HWND,Count> shortcut_{},task_{};
    std::array<std::wstring,Count> names_;
    std::array<int,Count> order_{0,1,2,3,4,5,6,7,8};
    std::function<void(int,bool)> visibility_;
    std::function<void(int,int,int)> resize_;
    int active_=-1;
    bool english_=false,recording_=false,overlayOn_=false;
    const wchar_t* lang(const wchar_t* zh,const wchar_t* en)const{return english_?en:zh;}
    const wchar_t* captureLabel()const{
        return recording_?lang(L"■ 停止回放",L"■ Stop replay"):lang(L"● 开始回放",L"● Start replay");
    }
    static HWND child(HWND parent,const wchar_t* type,const wchar_t* text,int id,DWORD style,HFONT font){
        HWND h=CreateWindowExW(0,type,text,WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|style,0,0,1,1,parent,
            reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
        if(!h)throw std::runtime_error("Cannot create desktop control");
        SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),FALSE);
        return h;
    }
    RECT area()const{
        RECT r{};GetClientRect(root_,&r);r.bottom=std::max(1L,r.bottom-Bar);
        r.right=std::max(1L,r.right);return r;
    }
    RECT bounds(Pane& p)const{
        auto r=area();
        if(p.maximized)return {0,0,r.right,r.bottom};
        int w=std::clamp(int(p.normal.right-p.normal.left),std::min(390,int(r.right)),int(r.right));
        int h=std::clamp(int(p.normal.bottom-p.normal.top),std::min(210,int(r.bottom)),int(r.bottom));
        int x=std::clamp(int(p.normal.left),0,int(r.right)-w);
        int y=std::clamp(int(p.normal.top),0,int(r.bottom)-h);
        return {x,y,x+w,y+h};
    }
    void place(Pane& p){
        auto box=bounds(p);
        SetWindowPos(p.window,nullptr,box.left,box.top,box.right-box.left,box.bottom-box.top,
            SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOOWNERZORDER);
    }
    void reflow(Pane& p){
        RECT client{};GetClientRect(p.window,&client);
        const int w=client.right;
        MoveWindow(p.title,4,4,std::max(1,w-8),Caption,TRUE);
        const int buttonWidth=24;
        const int x=w-4;
        MoveWindow(p.minimize,x-buttonWidth*3-8,7,buttonWidth,20,TRUE);
        MoveWindow(p.maximize,x-buttonWidth*2-7,7,buttonWidth,20,TRUE);
        MoveWindow(p.close,x-buttonWidth-4,7,buttonWidth,20,TRUE);
        if(!p.body)return;
        int height=std::max(1,int(client.bottom)-Caption-Border-10);
        int width=std::max(1,w-Border*2);
        MoveWindow(p.body,Border,Caption+Border,width,height,TRUE);
        resize_(p.index,width,height);
    }
    void refreshCaption(Pane& p){
        SetWindowTextW(p.title,names_[p.index].c_str());
        SetWindowTextW(p.window,names_[p.index].c_str());
        SetWindowTextW(p.maximize,p.maximized?L"❐":L"□");
        InvalidateRect(p.title,nullptr,TRUE);
    }
    void activate(int i){
        if(!visible(i))return; SetWindowPos(panes_[i].window,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE); if(active_==i)return;
        int previous=active_;active_=i;
        auto it=std::find(order_.begin(),order_.end(),i);
        std::rotate(it,it+1,order_.end());
        SetWindowPos(panes_[i].window,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        if(previous>=0){refreshCaption(panes_[previous]);InvalidateRect(task_[previous],nullptr,FALSE);}
        refreshCaption(panes_[i]);InvalidateRect(task_[i],nullptr,FALSE);
    }
    void hide(int index,bool close){
        auto& p=panes_[index];
        const bool wasVisible=visible(index);
        if(close){p.open=false;p.hidden=false;}
        else if(p.open)p.hidden=true;
        if(wasVisible){
            ShowWindow(p.window,SW_HIDE);
            if(active_==index){
                active_=-1;
                for(auto i=order_.rbegin();i!=order_.rend();++i)if(visible(*i)){activate(*i);break;}
            }
            visibility_(index,false);
        }
        layoutBar();
    }
    void maximize(Pane& p){
        p.maximized=!p.maximized;
        place(p);refreshCaption(p);
    }
    static LRESULT CALLBACK frameProc(HWND h,UINT msg,WPARAM w,LPARAM l){
        auto p=reinterpret_cast<Pane*>(GetWindowLongPtrW(h,GWLP_USERDATA));
        if(msg==WM_NCCREATE){
            p=static_cast<Pane*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
            SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(p));
        }
        if(!p)return DefWindowProcW(h,msg,w,l);
        auto& self=*p->desktop;
        switch(msg){
        case WM_ERASEBKGND:{
            RECT r{};GetClientRect(h,&r);auto dc=reinterpret_cast<HDC>(w);
            FillRect(dc,&r,lr_ui::paintBrushes().canvas);lr_ui::bevel(dc,r,false);
            if(!p->maximized){
                for(int i=3;i<=7;i+=2)
                    lr_ui::fill(dc,{r.right-i-2,r.bottom-6,r.right-i,r.bottom-4},RGB(95,95,95));
            }
            return 1;}
        case WM_SIZE:if(p->title)self.reflow(*p);return 0;
        case WM_CTLCOLORSTATIC:case WM_CTLCOLORBTN:return lr_ui::color(h,msg,w,l);
        case WM_DRAWITEM:
            if(w==4){
                auto& info=*reinterpret_cast<DRAWITEMSTRUCT*>(l);RECT r=info.rcItem;
                const bool selected=self.active_==p->index;
                if(selected)lr_ui::classicCaption(info.hDC,r);
                else lr_ui::fill(info.hDC,r,RGB(95,99,109));
                pixelIcon(info.hDC,p->index,r.left+5,r.top+5,1);
                r.left+=25;r.right-=90;
                lr_ui::label(info.hDC,info.hwndItem,r,RGB(255,255,255),self.names_[p->index].c_str(),DT_LEFT|DT_NOPREFIX);
                return TRUE;
            }
            break;
        case WM_COMMAND:
            if(LOWORD(w)==1||LOWORD(w)==3){
                PostMessageW(self.root_,WM_COMMAND,1600+p->index+(LOWORD(w)==3?Count:0),0);return 0;
            }
            if(LOWORD(w)==2){self.maximize(*p);return 0;}
            break;
        case WM_LBUTTONDOWN:{
            self.activate(p->index);
            RECT r{};GetClientRect(h,&r);
            int x=GET_X_LPARAM(l),y=GET_Y_LPARAM(l);
            p->sizing=!p->maximized&&x>=r.right-12&&y>=r.bottom-12;
            if(p->sizing){p->start={x,y};ClientToScreen(h,&p->start);p->startBox=self.bounds(*p);SetCapture(h);}
            return 0;}
        case WM_SETCURSOR:{
            POINT pt{};GetCursorPos(&pt);ScreenToClient(h,&pt);
            RECT r{};GetClientRect(h,&r);
            if(!p->maximized&&pt.x>=r.right-12&&pt.y>=r.bottom-12){SetCursor(LoadCursorW(nullptr,IDC_SIZENWSE));return TRUE;}
            break;}
        case WM_MOUSEMOVE:
            if(p->sizing&&GetCapture()==h){
                POINT pt{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ClientToScreen(h,&pt);
                p->normal=p->startBox;
                p->normal.right+=pt.x-p->start.x;
                p->normal.bottom+=pt.y-p->start.y;
                auto area=self.area();
                p->normal.right=std::clamp(p->normal.right,std::min(area.right,p->normal.left+390),area.right);
                p->normal.bottom=std::clamp(p->normal.bottom,std::min(area.bottom,p->normal.top+210),area.bottom);
                self.place(*p);
            }
            return 0;
        case WM_LBUTTONUP:if(p->sizing){p->sizing=false;ReleaseCapture();}return 0;
        case WM_CAPTURECHANGED:p->sizing=false;return 0;
        }
        return DefWindowProcW(h,msg,w,l);
    }
    static LRESULT CALLBACK titleProc(HWND h,UINT msg,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data){
        auto& p=*reinterpret_cast<Pane*>(data);auto& desktop=*p.desktop;
        switch(msg){
        case WM_LBUTTONDOWN:
            desktop.activate(p.index);
            if(!p.maximized){
                p.start={GET_X_LPARAM(l),GET_Y_LPARAM(l)};ClientToScreen(h,&p.start);
                p.startBox=desktop.bounds(p);SetCapture(h);p.dragging=GetCapture()==h;
            }
            return 0;
        case WM_LBUTTONDBLCLK:
            p.dragging=false;if(GetCapture()==h)ReleaseCapture();desktop.maximize(p);return 0;
        case WM_MOUSEMOVE:
            if(p.dragging&&GetCapture()==h){
                POINT pt{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ClientToScreen(h,&pt);p.normal=p.startBox;
                OffsetRect(&p.normal,pt.x-p.start.x,pt.y-p.start.y);
                p.normal=desktop.bounds(p);desktop.place(p);
            }
            return 0;
        case WM_LBUTTONUP:if(p.dragging){p.dragging=false;ReleaseCapture();}return 0;
        case WM_CAPTURECHANGED:p.dragging=false;return 0;
        case WM_NCDESTROY:RemoveWindowSubclass(h,titleProc,1);break;
        }
        return DefSubclassProc(h,msg,w,l);
    }
    void layoutBar(){
        auto r=area();const int y=r.bottom;
        MoveWindow(start_,5,y+5,100,27,TRUE);
        MoveWindow(showDesk_,110,y+5,88,27,TRUE);
        MoveWindow(capture_,203,y+5,114,27,TRUE);
        const int trayX=std::max(322,int(r.right)-166);
        MoveWindow(overlay_,trayX,y+7,72,23,TRUE);
        MoveWindow(clock_,trayX+75,y+8,85,21,TRUE);
        int count=0;for(const auto& p:panes_)if(p.open)++count;
        const int tabWidth=count?std::clamp((trayX-322)/count,4,158):0;
        int x=322;
        for(int i=0;i<Count;++i){
            if(panes_[i].open){MoveWindow(task_[i],x,y+5,tabWidth-3,27,TRUE);x+=tabWidth;}
            ShowWindow(task_[i],panes_[i].open?SW_SHOWNA:SW_HIDE);
        }
        InvalidateRect(root_,nullptr,TRUE);
    }
    void menu(){
        HMENU menu=CreatePopupMenu();if(!menu)return;
        for(int i:{0,1,3,2,5,4,6,7})
            AppendMenuW(menu,MF_OWNERDRAW,OpenBase+i,reinterpret_cast<LPCWSTR>(INT_PTR(i+1)));
        AppendMenuW(menu,MF_STRING|(crt_?MF_CHECKED:MF_UNCHECKED),1591,lang(L"CRT 桌面扫描线",L"CRT desktop scanlines")); AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
        AppendMenuW(menu,MF_OWNERDRAW,1590,reinterpret_cast<LPCWSTR>(INT_PTR(20)));
        RECT r{};GetWindowRect(start_,&r);
        UINT choice=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_LEFTALIGN|TPM_BOTTOMALIGN|TPM_RIGHTBUTTON,
            r.left,r.top,0,root_,nullptr);
        DestroyMenu(menu);
        if(choice)SendMessageW(root_,WM_COMMAND,choice,0);
    }
public:
    ~RetroDesktop(){if(bold_)DeleteObject(bold_);if(background_)DeleteObject(background_);if(crtBackground_)DeleteObject(crtBackground_);}
    static void pixelIcon(HDC dc,int id,int x,int y,int scale){
        static const auto icons=[](){std::array<HICON,Count> result{};for(int i=0;i<Count;++i){auto file=std::to_wstring(i)+L".ico";result[i]=reinterpret_cast<HICON>(LoadImageW(nullptr,assetPath(file.c_str()).c_str(),IMAGE_ICON,32,32,LR_LOADFROMFILE));}return result;}();
        if(id>=0&&id<Count&&icons[id]){DrawIconEx(dc,x,y,icons[id],16*scale,16*scale,0,nullptr,DI_NORMAL);return;}
        int saved=SaveDC(dc);
        SetMapMode(dc,MM_ANISOTROPIC);SetWindowOrgEx(dc,0,0,nullptr);
        SetViewportOrgEx(dc,x,y,nullptr);SetWindowExtEx(dc,32,32,nullptr);SetViewportExtEx(dc,16*scale,16*scale,nullptr);
        constexpr COLORREF colors[]={RGB(30,151,166),RGB(220,70,64),RGB(103,101,194),RGB(224,165,49),RGB(46,139,199),RGB(44,159,109),RGB(111,126,147),RGB(63,126,207),RGB(79,145,116)};
        auto brush=CreateSolidBrush(colors[std::clamp(id,0,8)]);auto pen=CreatePen(PS_SOLID,1,RGB(235,245,255));
        SelectObject(dc,brush);SelectObject(dc,pen);RoundRect(dc,2,2,30,30,8,8);
        auto line=[&](int a,int b,int c,int d){MoveToEx(dc,a,b,nullptr);LineTo(dc,c,d);};
        SelectObject(dc,GetStockObject(NULL_BRUSH));
        if(id==1){SelectObject(dc,GetStockObject(WHITE_BRUSH));Ellipse(dc,10,10,22,22);}
        else if(id==2){POINT v[]={{12,8},{24,16},{12,24}};SelectObject(dc,GetStockObject(WHITE_BRUSH));Polygon(dc,v,3);}
        else if(id==3){Rectangle(dc,6,12,26,24);line(6,12,6,8);line(6,8,15,8);line(15,8,18,12);}
        else if(id==5){line(8,24,8,16);line(16,24,16,8);line(24,24,24,12);}
        else if(id==4||id==6){for(int j=8;j<=24;j+=8){line(j,8,j,24);line(j-3,j==16?18:12,j+4,j==16?18:12);}}
        else if(id==7){Ellipse(dc,14,6,18,10);line(16,14,16,24);line(12,24,20,24);}
        else if(id==8){line(8,10,14,16);line(14,16,8,22);line(16,22,24,22);}
        else{RoundRect(dc,6,8,26,22,3,3);line(12,26,20,26);}
        RestoreDC(dc,saved);DeleteObject(brush);DeleteObject(pen);
    }
    static HICON appIcon(){
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=16;info.bmiHeader.biHeight=-16;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        void* pixels=nullptr;
        HBITMAP color=CreateDIBSection(nullptr,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
        WORD maskBits[16]{};
        HBITMAP mask=CreateBitmap(16,16,1,1,maskBits);
        HDC dc=CreateCompatibleDC(nullptr);
        HICON result=nullptr;
        if(color&&mask&&dc&&pixels){
            auto rgba=static_cast<DWORD*>(pixels);
            std::fill_n(rgba,256,0u);
            auto old=SelectObject(dc,color);pixelIcon(dc,0,0,0,1);GdiFlush();SelectObject(dc,old);
            for(int i=0;i<256;++i)if(rgba[i])rgba[i]|=0xFF000000u;
            ICONINFO icon{};icon.fIcon=TRUE;icon.hbmMask=mask;icon.hbmColor=color;
            result=CreateIconIndirect(&icon);
        }
        if(dc)DeleteDC(dc);
        if(mask)DeleteObject(mask);
        if(color)DeleteObject(color);
        return result;
    }
    void init(HWND root,HFONT font,std::function<void(int,bool)> visibility,std::function<void(int,int,int)> resize){
        background_=reinterpret_cast<HBITMAP>(LoadImageW(nullptr,assetPath(L"wallpaper.bmp").c_str(),IMAGE_BITMAP,0,0,LR_LOADFROMFILE|LR_CREATEDIBSECTION)); crtBackground_=reinterpret_cast<HBITMAP>(LoadImageW(nullptr,assetPath(L"wallpaper-crt.bmp").c_str(),IMAGE_BITMAP,0,0,LR_LOADFROMFILE|LR_CREATEDIBSECTION)); root_=root;font_=font;visibility_=std::move(visibility);resize_=std::move(resize);
        bold_=lr_ui::makeFont(15,FW_BOLD,L"Tahoma");
        WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=frameProc;
        cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.lpszClassName=L"LiteReplayRetroFrame";
        if(!RegisterClassW(&cls))throw std::runtime_error("Cannot register desktop window frame");
        for(int i=0;i<Count;++i){
            auto& p=panes_[i];p.desktop=this;p.index=i;
            p.window=CreateWindowExW(WS_EX_CONTROLPARENT,cls.lpszClassName,L"",WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
                0,0,1,1,root,nullptr,cls.hInstance,&p);
            if(!p.window)throw std::runtime_error("Cannot create desktop window frame");
            p.title=child(p.window,L"STATIC",L"",4,SS_OWNERDRAW|SS_NOTIFY|WS_CLIPSIBLINGS,bold_);
            SetWindowSubclass(p.title,titleProc,1,reinterpret_cast<DWORD_PTR>(&p));
            p.minimize=child(p.window,L"BUTTON",L"−",1,WS_TABSTOP|WS_CLIPSIBLINGS,bold_);
            p.maximize=child(p.window,L"BUTTON",L"□",2,WS_TABSTOP|WS_CLIPSIBLINGS,bold_);
            p.close=child(p.window,L"BUTTON",L"×",3,WS_TABSTOP|WS_CLIPSIBLINGS,bold_);
            SetWindowPos(p.title,HWND_BOTTOM,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
            if(i!=Prompt)shortcut_[i]=child(root,L"BUTTON",L"",ShortcutBase+i,BS_OWNERDRAW|WS_TABSTOP,font_);
            task_[i]=child(root,L"BUTTON",L"",TaskBase+i,BS_OWNERDRAW|WS_TABSTOP,font_);
            ShowWindow(task_[i],SW_HIDE);
        }
        start_=child(root,L"BUTTON",L"",Start,BS_OWNERDRAW|WS_TABSTOP,bold_);
        showDesk_=child(root,L"BUTTON",L"",ShowDesktop,WS_TABSTOP,font_);
        capture_=child(root,L"BUTTON",L"REC",CaptureTray,BS_OWNERDRAW|WS_TABSTOP,bold_);
        overlay_=child(root,L"BUTTON",L"HUD",OverlayTray,BS_OWNERDRAW|WS_TABSTOP,bold_);
        clock_=child(root,L"STATIC",L"",ClockId,SS_CENTER|SS_CENTERIMAGE|SS_SUNKEN,font_);
        tick();
    }
    HWND frame(int i)const{return panes_[i].window;}
    void attach(int i,HWND body){panes_[i].body=body;}
    bool visible(int i)const{return panes_[i].open&&!panes_[i].hidden;}
    void localize(bool english,const std::array<std::wstring,Count>& names){
        english_=english;names_=names;
        SetWindowTextW(start_,lang(L"开始",L"Start"));
        SetWindowTextW(showDesk_,lang(L"显示桌面",L"Desktop"));
        SetWindowTextW(capture_,captureLabel());
        for(int i=0;i<Count;++i){
            refreshCaption(panes_[i]);
            if(shortcut_[i])SetWindowTextW(shortcut_[i],names_[i].c_str());
            SetWindowTextW(task_[i],names_[i].c_str());
        }
        lr_ui::redraw(root_);
    }
    void show(int i){
        auto& p=panes_[i];
        const bool opening=!visible(i);
        if(!p.positioned){
            const int widths[]={720,690,1135,864,814,838,718,500,665};
            const int heights[]={555,785,730,556,783,775,681,442,470};
            const int offset=(i%5)*23;
            const auto r=area();
            int x=std::min(127+offset,std::max(0,int(r.right)-widths[i]));
            int y=std::min(28+offset,std::max(0,int(r.bottom)-heights[i]));
            p.normal={x,y,x+widths[i],y+heights[i]};p.positioned=true;
        }
        place(p);p.open=true;p.hidden=false;
        ShowWindow(p.window,SW_SHOWNA);
        activate(i);layoutBar();
        if(opening)visibility_(i,true);
        if(i!=Prompt&&IsWindowVisible(root_)&&!(GetFocus()==p.window||IsChild(p.window,GetFocus())))SetFocus(p.window);
    }
    void layout(){
        if(!start_)return;
        auto r=area();
        const int columns=std::max(1,int(r.bottom-20)/66);
        int n=0;
        for(int i:{0,3,2,5,1,4,6,7}){
            MoveWindow(shortcut_[i],12+(n/columns)*93,12+(n%columns)*66,89,62,TRUE);++n;
        }
        for(auto& p:panes_)if(p.positioned)place(p);
        layoutBar();
    }
    void activateFrom(HWND h){
        for(auto& p:panes_)if(visible(p.index)&&(h==p.window||IsChild(p.window,h))){activate(p.index);return;}
    }
    bool command(int id){
        if(id==1591){crt_=!crt_;lr_ui::redraw(root_);return true;} if(id==Start){menu();return true;}
        if(id==ShowDesktop){
            for(int i=0;i<Count;++i)if(visible(i))hide(i,false);
            SetFocus(showDesk_);return true;
        }
        if(id>=ShortcutBase&&id<ShortcutBase+Count){show(id-ShortcutBase);return true;}
        if(id>=TaskBase&&id<TaskBase+Count){
            int i=id-TaskBase;
            if(visible(i)&&active_==i)hide(i,false);else show(i);
            return true;
        }
        if(id>=1600&&id<1600+Count*2){
            int n=id-1600;hide(n%Count,n>=Count);return true;
        }
        return false;
    }
    bool measure(MEASUREITEMSTRUCT& entry)const{
        if(entry.CtlType!=ODT_MENU||entry.itemData==0)return false;
        const int n=int(entry.itemData);
        if(n!=20&&(n<1||n>Count))return false;
        entry.itemWidth=225;entry.itemHeight=30;return true;
    }
    bool draw(DRAWITEMSTRUCT& d){
        RECT r=d.rcItem;auto dc=d.hDC;
        if(d.CtlType==ODT_MENU&&d.itemData){
            const int i=int(d.itemData)-1;
            if(i!=19&&(i<0||i>=Count))return false;
            const bool selected=(d.itemState&ODS_SELECTED)!=0;
            lr_ui::fill(dc,r,selected?RGB(0,0,128):RGB(192,192,192));
            lr_ui::fill(dc,{r.left,r.top,r.left+8,r.bottom},RGB(49,73,126));
            pixelIcon(dc,i==19?1:i,r.left+15,r.top+7,1);
            r.left+=39;
            auto old=SelectObject(dc,font_);
            SetTextColor(dc,selected?RGB(255,255,255):RGB(20,20,20));SetBkMode(dc,TRANSPARENT);
            DrawTextW(dc,i==19?lang(L"退出 LiteReplay",L"Exit LiteReplay"):names_[i].c_str(),-1,&r,DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            SelectObject(dc,old);return true;
        }
        const int id=int(d.CtlID);
        const bool focus=(d.itemState&ODS_FOCUS)!=0,down=(d.itemState&ODS_SELECTED)!=0;
        if(id>=ShortcutBase&&id<ShortcutBase+Count){
            const int i=id-ShortcutBase;
            POINT origin{};MapWindowPoints(d.hwndItem,root_,&origin,1);paintBackground(dc,origin.x,origin.y);
            pixelIcon(dc,i,r.left+(r.right-r.left-32)/2,r.top+3,2);
            RECT text{r.left+2,r.top+39,r.right-2,r.bottom-2};
            if(focus||down)lr_ui::fill(dc,text,RGB(0,0,128));
            lr_ui::label(dc,d.hwndItem,text,RGB(255,255,255),names_[i].c_str(),DT_CENTER|DT_NOPREFIX);
            if(focus)DrawFocusRect(dc,&text);
            return true;
        }
        const bool task=id>=TaskBase&&id<TaskBase+Count;
        if(!task&&id!=Start&&id!=CaptureTray&&id!=OverlayTray)return false;
        const int index=task?id-TaskBase:0;
        const bool current=task&&active_==index;
        lr_ui::fill(dc,r,current?RGB(222,222,222):RGB(192,192,192));
        lr_ui::bevel(dc,r,current||down);
        if(id==CaptureTray||id==OverlayTray){
            const bool on=id==CaptureTray?recording_:overlayOn_;
            auto color=on?(id==CaptureTray?RGB(172,24,18):RGB(0,92,59)):RGB(88,88,88);
            InflateRect(&r,-3,-3);
            lr_ui::label(dc,d.hwndItem,r,color,id==CaptureTray?captureLabel():L"● HUD",DT_CENTER);
        }else{
            pixelIcon(dc,index,r.left+5,r.top+6,1);
            r.left+=24;r.right-=4;
            lr_ui::label(dc,d.hwndItem,r,RGB(15,15,15),task?names_[index].c_str():lang(L"开始",L"Start"),DT_LEFT|DT_NOPREFIX);
        }
        if(focus&&!IsRectEmpty(&r))DrawFocusRect(dc,&r);
        return true;
    }
    void paint(HDC dc)const{
        auto r=area();
        paintBackground(dc);
        RECT bar{0,r.bottom,r.right,r.bottom+Bar};
        lr_ui::fill(dc,bar,RGB(192,192,192));lr_ui::bevel(dc,bar,false);
        lr_ui::fill(dc,{107,r.bottom+6,108,r.bottom+31},RGB(110,110,110));
        RECT tray{std::max(320L,r.right-168),r.bottom+4,r.right-4,r.bottom+33};
        lr_ui::bevel(dc,tray,true);
    }
    void indicators(bool capture,bool overlay){
        if(capture!=recording_){recording_=capture;SetWindowTextW(capture_,captureLabel());InvalidateRect(capture_,nullptr,FALSE);}
        if(overlay!=overlayOn_){overlayOn_=overlay;InvalidateRect(overlay_,nullptr,FALSE);}
    }
    void tick(){
        wchar_t time[32]{};
        GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT,TIME_NOSECONDS,nullptr,nullptr,time,32);
        SetWindowTextW(clock_,time);
    }
};
}
