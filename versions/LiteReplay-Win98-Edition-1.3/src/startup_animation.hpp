#pragma once
#include "retro_desktop.hpp"
#include <mmsystem.h>
#include <cstdint>

namespace lr {
class StartupAnimation {
    static constexpr int ViewId=1545;
    static constexpr UINT Duration=2400;
    ULONGLONG start_{};
    MCIDEVICEID audio_{};
    bool english_{};

    static RECT lightBand(RECT view){
        return {0,view.bottom-22,view.right,view.bottom};
    }
    static double noise(int x,int y,int size){
        auto sample=[](int column,int row){
            uint32_t bits=uint32_t(column)*374761393u+uint32_t(row)*668265263u;
            bits=(bits^(bits>>13))*1274126177u;
            return double((bits^(bits>>16))&255)/255;
        };
        auto fade=[](double t){return t*t*(3-2*t);};
        auto mix=[](double a,double b,double t){return a+(b-a)*t;};
        const int column=x/size,row=y/size;
        const double u=fade(double(x%size)/size),v=fade(double(y%size)/size);
        return mix(mix(sample(column,row),sample(column+1,row),u),
            mix(sample(column,row+1),sample(column+1,row+1),u),v);
    }
    static void paintSky(HDC dc,RECT r){
        constexpr int Width=384,Height=240;
        static const auto pixels=[]{
            std::vector<uint32_t> out(Width*Height);
            for(int y=0;y<Height;++y)for(int x=0;x<Width;++x){
                const double haze=double(y)/Height;
                const double cloud=std::clamp((noise(x+117,(y+32)*2,92)*.75+
                    noise(x+31,y*2,48)*.18+noise(x,y,19)*.07-.28)*2.2,0.0,1.0);
                const auto tint=[&](double top,double bottom){
                    const double base=top+(bottom-top)*haze;
                    return uint32_t(base+(250-base)*cloud);
                };
                out[y*Width+x]=(tint(90,182)<<16)|(tint(160,216)<<8)|tint(216,240);
            }
            return out;
        }();
        BITMAPINFO info{};
        info.bmiHeader={sizeof(BITMAPINFOHEADER),Width,-Height,1,32,BI_RGB};
        SetStretchBltMode(dc,HALFTONE);SetBrushOrgEx(dc,0,0,nullptr);
        StretchDIBits(dc,0,0,r.right,r.bottom,0,0,Width,Height,pixels.data(),&info,DIB_RGB_COLORS,SRCCOPY);
    }
    void paint(HWND view){
        PAINTSTRUCT ps{};
        HDC dc=BeginPaint(view,&ps);
        RECT r{};GetClientRect(view,&r);
        const auto band=lightBand(r);
        if(ps.rcPaint.top<band.top){
            paintSky(dc,r);
            const int x=r.right/2,y=r.bottom/2;
            RetroDesktop::pixelIcon(dc,0,x-40,y-112,5);
            HFONT title=lr_ui::makeFont(56,FW_BOLD,L"Tahoma");
            auto previous=SelectObject(dc,title?title:lr_ui::classicHeading);
            SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(14,36,69));
            RECT wordmark{0,y-22,r.right,y+47};
            DrawTextW(dc,L"LiteReplay",-1,&wordmark,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            SelectObject(dc,previous);if(title)DeleteObject(title);
            const auto oldFont=reinterpret_cast<HFONT>(SendMessageW(view,WM_GETFONT,0,0));
            SetWindowFont(view,lr_ui::classicHeading,FALSE);
            lr_ui::label(dc,view,{0,y+49,r.right,y+83},RGB(29,62,98),L"Win98 Edition 1.3",DT_CENTER);
            SetWindowFont(view,lr_ui::classicSmall,FALSE);
            lr_ui::label(dc,view,{0,band.top-35,r.right,band.top-7},RGB(29,62,98),
                english_?L"Esc or click to skip":L"按 Esc 或单击跳过",DT_CENTER);
            SetWindowFont(view,oldFont,FALSE);
        }
        lr_ui::fill(dc,band,RGB(12,30,57));
        const int phase=int((GetTickCount64()-start_)/9%480);
        for(int x=0;x<r.right;x+=5){
            const int offset=(x+480-phase)%480;
            const int light=std::max(0,160-std::abs(offset-240));
            lr_ui::fill(dc,{x,band.top+5,std::min(x+5,int(r.right)),band.bottom-5},
                RGB(24+light/2,57+light,96+light*9/10));
        }
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
            RECT r{};GetClientRect(h,&r);auto band=lightBand(r);
            InvalidateRect(h,&band,FALSE);return 0;
        }
        case WM_LBUTTONUP:animation.skip(h);return 0;
        case WM_NCDESTROY:KillTimer(h,1);RemoveWindowSubclass(h,proc,1);break;
        }
        return DefSubclassProc(h,msg,w,l);
    }
    void playSound(HWND parent,const wchar_t* path){
        if(!path)return;
        const DWORD attributes=GetFileAttributesW(path);
        if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_DIRECTORY))return;
        stopAudio();
        MCI_OPEN_PARMSW open{};
        open.lpstrDeviceType=L"waveaudio";open.lpstrElementName=path;
        if(mciSendCommandW(0,MCI_OPEN,MCI_OPEN_TYPE|MCI_OPEN_ELEMENT|MCI_WAIT,reinterpret_cast<DWORD_PTR>(&open)))return;
        audio_=open.wDeviceID;
        MCI_PLAY_PARMS play{};play.dwCallback=reinterpret_cast<DWORD_PTR>(parent);
        if(mciSendCommandW(audio_,MCI_PLAY,MCI_NOTIFY,reinterpret_cast<DWORD_PTR>(&play)))stopAudio();
    }
    void skip(HWND view){stopAudio();DestroyWindow(view);}
public:
    void stopAudio(){
        if(!audio_)return;
        const auto device=audio_;audio_=0;
        mciSendCommandW(device,MCI_CLOSE,MCI_WAIT,0);
    }
    bool audioNotification(WPARAM status,LPARAM device){
        if(!audio_||MCIDEVICEID(device)!=audio_)return false;
        if(status==MCI_NOTIFY_SUCCESSFUL||status==MCI_NOTIFY_FAILURE)stopAudio();
        return true;
    }
    bool handleKey(HWND parent,const MSG& msg){
        HWND view=GetDlgItem(parent,ViewId);
        if(!view||msg.message!=WM_KEYDOWN)return false;
        if(msg.wParam==VK_ESCAPE||msg.wParam==VK_RETURN)skip(view);
        return true;
    }
    void resize(HWND parent){
        if(HWND view=GetDlgItem(parent,ViewId)){
            RECT r{};GetClientRect(parent,&r);
            SetWindowPos(view,HWND_TOP,0,0,r.right,r.bottom,SWP_NOACTIVATE);
            InvalidateRect(view,nullptr,FALSE);
        }
    }
    void show(HWND parent,bool english,const wchar_t* sound=nullptr){
        if(GetDlgItem(parent,ViewId))return;
        HWND view=CreateWindowExW(0,L"STATIC",L"LiteReplay",WS_CHILD|WS_CLIPSIBLINGS,
            0,0,0,0,parent,reinterpret_cast<HMENU>(INT_PTR(ViewId)),GetModuleHandleW(nullptr),nullptr);
        if(!view)return;
        start_=GetTickCount64();english_=english;
        if(!SetWindowSubclass(view,proc,1,reinterpret_cast<DWORD_PTR>(this))||!SetTimer(view,1,40,nullptr)){
            DestroyWindow(view);return;
        }
        resize(parent);ShowWindow(view,SW_SHOW);SetFocus(view);
        playSound(parent,sound);
    }
};
}
