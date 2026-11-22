#pragma once
#include "retro_desktop.hpp"

namespace lr {
class StartupAnimation {
    static constexpr int ViewId=1545;
    static constexpr UINT Duration=1600;
    ULONGLONG start_{};
    bool english_{};

    static RECT progressBar(RECT view){
        const int width=std::clamp(int(view.right)-60,20,330);
        const int center=view.right/2,y=view.bottom/2+65;
        return {center-width/2,y,center+width/2,y+17};
    }
    void paint(HWND view){
        PAINTSTRUCT ps{};
        HDC dc=BeginPaint(view,&ps);
        RECT r{};GetClientRect(view,&r);
        for(int i=0;i<24;++i){
            RECT strip{0,r.bottom*i/24,r.right,r.bottom*(i+1)/24};
            lr_ui::fill(dc,strip,RGB(6+2*i/3,21+2*i,56+3*i));
        }
        const int x=r.right/2,y=r.bottom/2;
        RetroDesktop::pixelIcon(dc,0,x-24,y-105,3);
        SetWindowFont(view,lr_ui::classicHeading,FALSE);
        lr_ui::label(dc,view,{0,y-46,r.right,y-11},RGB(255,255,255),L"LiteReplay",DT_CENTER);
        SetWindowFont(view,lr_ui::classicSmall,FALSE);
        lr_ui::label(dc,view,{0,y-10,r.right,y+15},RGB(179,227,242),L"WIN98 EDITION 1.2",DT_CENTER);
        lr_ui::label(dc,view,{0,y+27,r.right,y+54},RGB(221,239,252),
            english_?L"Opening the desktop…":L"正在打开桌面…",DT_CENTER);
        auto bar=progressBar(r);
        lr_ui::bevel(dc,bar,true);
        InflateRect(&bar,-3,-3);
        lr_ui::fill(dc,bar,RGB(5,18,48));
        const auto elapsed=std::min<ULONGLONG>(GetTickCount64()-start_,Duration);
        const int end=bar.left+int((bar.right-bar.left)*elapsed/Duration);
        for(int left=bar.left;left<end;left+=12)
            lr_ui::fill(dc,{left,bar.top,std::min(end,left+9),bar.bottom},RGB(97,214,222));
        lr_ui::label(dc,view,{0,y+91,r.right,y+113},RGB(178,214,235),
            english_?L"Esc or click to skip":L"按 Esc 或单击跳过",DT_CENTER);
        EndPaint(view,&ps);
    }
    static LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data){
        auto& animation=*reinterpret_cast<StartupAnimation*>(data);
        switch(msg){
        case WM_NCHITTEST:return HTCLIENT;
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:animation.paint(h);return 0;
        case WM_TIMER:{
            if(GetTickCount64()-animation.start_>=Duration){DestroyWindow(h);return 0;}
            RECT r{};GetClientRect(h,&r);auto bar=progressBar(r);
            InvalidateRect(h,&bar,FALSE);return 0;
        }
        case WM_LBUTTONUP:DestroyWindow(h);return 0;
        case WM_NCDESTROY:KillTimer(h,1);RemoveWindowSubclass(h,proc,1);break;
        }
        return DefSubclassProc(h,msg,w,l);
    }
public:
    bool handleKey(HWND parent,const MSG& msg){
        HWND view=GetDlgItem(parent,ViewId);
        if(!view||msg.message!=WM_KEYDOWN)return false;
        if(msg.wParam==VK_ESCAPE||msg.wParam==VK_RETURN)DestroyWindow(view);
        return true;
    }
    void resize(HWND parent){
        if(HWND view=GetDlgItem(parent,ViewId)){
            RECT r{};GetClientRect(parent,&r);
            SetWindowPos(view,HWND_TOP,0,0,r.right,r.bottom,SWP_NOACTIVATE);
            InvalidateRect(view,nullptr,FALSE);
        }
    }
    void show(HWND parent,bool english){
        if(GetDlgItem(parent,ViewId))return;
        HWND view=CreateWindowExW(0,L"STATIC",L"LiteReplay",WS_CHILD|WS_CLIPSIBLINGS,
            0,0,0,0,parent,reinterpret_cast<HMENU>(INT_PTR(ViewId)),GetModuleHandleW(nullptr),nullptr);
        if(!view)return;
        start_=GetTickCount64();english_=english;
        if(!SetWindowSubclass(view,proc,1,reinterpret_cast<DWORD_PTR>(this))||!SetTimer(view,1,40,nullptr)){
            DestroyWindow(view);return;
        }
        resize(parent);ShowWindow(view,SW_SHOW);SetFocus(view);
    }
};
}
