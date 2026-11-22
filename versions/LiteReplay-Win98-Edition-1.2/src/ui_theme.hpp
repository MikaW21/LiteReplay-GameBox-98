#pragma once
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <array>
#include <map>
#include <vector>
#include <string>
#include <algorithm>

namespace lr_ui {
enum class Theme { Graphite, Classic98, Mist, Carbon, Phosphor, Arcade };
inline constexpr std::array<const char*,6> themeKeys={"theme.graphite","theme.classic","theme.mist","theme.carbon",
    "theme.phosphor","theme.arcade"};
inline constexpr Theme parseTheme(int value){
    return value>=0&&value<int(themeKeys.size())?static_cast<Theme>(value):Theme::Graphite;
}
struct Palette {
    COLORREF canvas,surface,input,sidebar,edge,ink,muted;
    COLORREF accent,onAccent,active,hover,pressed,caption,onCaption;
    COLORREF overlay,overlayInk,preview,previewInk;
    int corners;
    bool dark;
};
inline constexpr Palette palettes[]={
    {RGB(22,25,30),RGB(30,35,42),RGB(39,45,54),RGB(16,20,26),RGB(55,64,75),RGB(231,237,244),RGB(151,165,183),
     RGB(88,215,184),RGB(16,31,29),RGB(36,70,68),RGB(50,60,72),RGB(42,83,74),RGB(22,25,30),RGB(231,237,244),
     RGB(13,27,39),RGB(224,242,251),RGB(12,15,19),RGB(151,165,183),12,true},
    {RGB(192,192,192),RGB(212,208,200),RGB(255,255,255),RGB(192,192,192),RGB(103,103,103),RGB(21,21,21),RGB(77,76,73),
     RGB(0,0,128),RGB(255,255,255),RGB(0,0,128),RGB(230,227,219),RGB(172,168,160),RGB(0,0,128),RGB(255,255,255),
     RGB(212,208,200),RGB(20,20,20),RGB(49,49,49),RGB(227,227,227),0,false},
    {RGB(242,244,248),RGB(255,255,255),RGB(248,249,252),RGB(232,237,246),RGB(193,204,220),RGB(30,42,61),RGB(77,92,114),
     RGB(28,85,173),RGB(255,255,255),RGB(211,227,251),RGB(228,236,249),RGB(199,219,248),RGB(242,244,248),RGB(30,42,61),
     RGB(250,252,255),RGB(30,42,61),RGB(41,51,66),RGB(237,241,248),16,false},
    {RGB(19,19,20),RGB(37,37,40),RGB(47,47,51),RGB(25,25,27),RGB(91,91,98),RGB(245,245,247),RGB(174,174,185),
     RGB(120,169,255),RGB(13,21,38),RGB(35,57,92),RGB(56,56,63),RGB(53,79,120),RGB(26,39,63),RGB(245,245,247),
     RGB(20,20,22),RGB(237,241,249),RGB(12,12,14),RGB(177,180,192),0,true},
    {RGB(8,21,14),RGB(14,33,22),RGB(17,45,31),RGB(7,29,19),RGB(71,131,89),RGB(213,250,220),RGB(147,200,159),
     RGB(132,255,155),RGB(9,35,17),RGB(29,63,40),RGB(29,70,43),RGB(34,74,48),RGB(17,54,31),RGB(181,255,194),
     RGB(6,25,14),RGB(158,255,178),RGB(6,16,10),RGB(147,200,159),0,true},
    {RGB(27,18,49),RGB(48,30,77),RGB(57,38,86),RGB(35,20,62),RGB(121,84,159),RGB(252,241,255),RGB(213,185,231),
     RGB(255,143,215),RGB(61,12,54),RGB(92,43,105),RGB(81,46,112),RGB(109,42,105),RGB(58,28,106),RGB(255,233,253),
     RGB(42,20,68),RGB(254,231,255),RGB(26,11,46),RGB(231,202,249),24,true}
};
inline Theme current=Theme::Graphite;
inline const Palette& colors(){return palettes[static_cast<int>(current)];}
inline bool classic(){return current==Theme::Classic98;}
inline bool phosphor(){return current==Theme::Phosphor;}
inline bool arcade(){return current==Theme::Arcade;}
inline bool banner(){return classic()||current==Theme::Carbon||phosphor()||arcade();}
struct Brushes {
    HBRUSH canvas{},surface{},input{},sidebar{},caption{};
    void clear(){
        for(auto brush:{canvas,surface,input,sidebar,caption})if(brush)DeleteObject(brush);
        *this={};
    }
    bool create(const Palette& p){
        canvas=CreateSolidBrush(p.canvas);surface=CreateSolidBrush(p.surface);
        input=CreateSolidBrush(p.input);sidebar=CreateSolidBrush(p.sidebar);caption=CreateSolidBrush(p.caption);
        return canvas&&surface&&input&&sidebar&&caption;
    }
};
inline std::array<Brushes,themeKeys.size()> brushes;
inline const Brushes& paintBrushes(){return brushes[static_cast<int>(current)];}
inline HFONT heading{},small{},classicHeading{},classicSmall{},terminalHeading{};
inline std::map<HWND,std::vector<RECT>> sections;
inline HFONT makeFont(int size,int weight,const wchar_t* name){
    return CreateFontW(-size,0,0,0,weight,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,name);
}
inline bool init(){
    for(size_t i=0;i<brushes.size();++i)if(!brushes[i].create(palettes[i]))return false;
    heading=makeFont(26,FW_SEMIBOLD,L"Microsoft YaHei UI");
    small=makeFont(13,FW_NORMAL,L"Microsoft YaHei UI");
    classicHeading=makeFont(23,FW_BOLD,L"Tahoma");
    classicSmall=makeFont(13,FW_NORMAL,L"Tahoma");
    terminalHeading=makeFont(25,FW_BOLD,L"Consolas");
    return heading&&small&&classicHeading&&classicSmall&&terminalHeading;
}
inline void fill(HDC dc,RECT r,COLORREF color){
    HBRUSH b=CreateSolidBrush(color);
    if(b){FillRect(dc,&r,b);DeleteObject(b);}
}
inline void classicCaption(HDC dc,RECT r){
    const int width=r.right-r.left;
    if(width<=0)return;
    for(int step=0;step<24;++step){
        RECT strip{r.left+width*step/24,r.top,r.left+width*(step+1)/24,r.bottom};
        fill(dc,strip,RGB(8+51*step/23,20+73*step/23,111+62*step/23));
    }
}
inline void frame(HDC dc,RECT r,COLORREF bg,COLORREF edge,int radius){
    HBRUSH brush=CreateSolidBrush(bg);
    HPEN pen=CreatePen(PS_SOLID,1,edge);
    if(brush&&pen){
        auto oldBrush=SelectObject(dc,brush),oldPen=SelectObject(dc,pen);
        if(radius)RoundRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);
        else Rectangle(dc,r.left,r.top,r.right,r.bottom);
        SelectObject(dc,oldPen);SelectObject(dc,oldBrush);
    }
    if(brush)DeleteObject(brush);
    if(pen)DeleteObject(pen);
}
inline void bevel(HDC dc,RECT r,bool inset){
    auto hi=inset?RGB(64,64,64):RGB(255,255,255);
    auto lo=inset?RGB(255,255,255):RGB(64,64,64);
    fill(dc,{r.left,r.top,r.right,r.top+1},hi);
    fill(dc,{r.left,r.top+1,r.left+1,r.bottom},hi);
    fill(dc,{r.right-1,r.top+1,r.right,r.bottom},lo);
    fill(dc,{r.left+1,r.bottom-1,r.right,r.bottom},lo);
    InflateRect(&r,-1,-1);
    hi=inset?RGB(129,129,129):RGB(231,231,231);
    lo=inset?RGB(231,231,231):RGB(129,129,129);
    fill(dc,{r.left,r.top,r.right,r.top+1},hi);
    fill(dc,{r.left,r.top+1,r.left+1,r.bottom},hi);
    fill(dc,{r.right-1,r.top+1,r.right,r.bottom},lo);
    fill(dc,{r.left+1,r.bottom-1,r.right,r.bottom},lo);
}
inline RECT shifted(HWND h,RECT r){
    OffsetRect(&r,-GetScrollPos(h,SB_HORZ),-GetScrollPos(h,SB_VERT));return r;
}
inline void paint(HWND h,HDC dc){
    RECT r{};GetClientRect(h,&r);FillRect(dc,&r,paintBrushes().canvas);
    auto it=sections.find(h);
    if(it==sections.end())return;
    const auto& p=colors();
    for(auto card:it->second){
        card=shifted(h,card);
        if(classic()){FillRect(dc,&card,paintBrushes().surface);bevel(dc,card,true);}
        else frame(dc,card,p.surface,p.edge,p.corners);
    }
}
struct Surface {COLORREF color;HBRUSH brush;};
inline Surface behind(HWND parent,HWND child){
    auto& p=colors();auto& b=paintBrushes();
    RECT r{};GetWindowRect(child,&r);
    POINT point{r.left,r.top};ScreenToClient(parent,&point);
    if(auto width=GetPropW(parent,L"lrSidebarWidth");width&&point.x<reinterpret_cast<INT_PTR>(width))
        return {p.sidebar,b.sidebar};
    auto it=sections.find(parent);
    if(it!=sections.end())for(auto card:it->second){
        card=shifted(parent,card);
        if(PtInRect(&card,point))return {p.surface,b.surface};
    }
    return {p.canvas,b.canvas};
}
inline LRESULT color(HWND parent,UINT msg,WPARAM w,LPARAM l){
    HDC dc=reinterpret_cast<HDC>(w);HWND child=reinterpret_cast<HWND>(l);
    const auto& p=colors();const auto& b=paintBrushes();
    auto background=behind(parent,child);
    COLORREF text=IsWindowEnabled(child)?p.ink:p.muted;
    wchar_t cls[16]{};GetClassNameW(child,cls,16);
    if(banner()&&GetPropW(child,L"lrBanner")){background={p.caption,b.caption};text=p.onCaption;}
    else if(msg==WM_CTLCOLOREDIT||msg==WM_CTLCOLORLISTBOX||GetPropW(child,L"lrField")||lstrcmpiW(cls,L"Edit")==0)
        background={p.input,b.input};
    SetTextColor(dc,text);SetBkColor(dc,background.color);
    return reinterpret_cast<LRESULT>(background.brush);
}
inline void label(HDC dc,HWND control,RECT rect,COLORREF text,const wchar_t* title,UINT format){
    auto font=reinterpret_cast<HFONT>(SendMessageW(control,WM_GETFONT,0,0));
    auto previous=SelectObject(dc,font?font:GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,text);
    DrawTextW(dc,title,-1,&rect,format|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
    SelectObject(dc,previous);
}
inline void paintButton(HWND h,HDC dc){
    const auto& p=colors();
    RECT r{};GetClientRect(h,&r);
    auto under=behind(GetParent(h),h);
    FillRect(dc,&r,under.brush);
    const bool enabled=IsWindowEnabled(h)!=FALSE,focus=GetFocus()==h;
    const bool active=GetPropW(h,L"lrActive")!=nullptr,primary=GetPropW(h,L"lrPrimary")!=nullptr;
    const bool nav=GetPropW(h,L"lrNav")!=nullptr,hover=GetPropW(h,L"lrHover")!=nullptr;
    const bool down=(SendMessageW(h,BM_GETSTATE,0,0)&BST_PUSHED)!=0;
    auto type=GetWindowLongPtrW(h,GWL_STYLE)&BS_TYPEMASK;
    wchar_t title[512]{};GetWindowTextW(h,title,512);
    if(type==BS_AUTOCHECKBOX||type==BS_CHECKBOX){
        const LONG y=(r.bottom-15)/2;
        RECT mark{1,y,16,y+15};
        const auto state=SendMessageW(h,BM_GETCHECK,0,0);
        frame(dc,mark,p.input,focus||hover?p.accent:p.edge,classic()?0:4);
        if(classic())bevel(dc,mark,true);
        if(state==BST_CHECKED){
            HPEN pen=CreatePen(PS_SOLID,2,enabled?p.accent:p.muted);
            auto old=SelectObject(dc,pen);
            MoveToEx(dc,4,y+7,nullptr);LineTo(dc,7,y+11);LineTo(dc,13,y+4);
            SelectObject(dc,old);DeleteObject(pen);
        }
        r.left=22;
        label(dc,h,r,enabled?p.ink:p.muted,title,DT_LEFT);
        if(focus)DrawFocusRect(dc,&r);
        return;
    }
    COLORREF bg=nav?p.sidebar:p.surface,foreground=p.ink;
    if(enabled){
        if(down)bg=p.pressed;
        else if(primary){bg=p.accent;foreground=p.onAccent;}
        else if(active){bg=p.active;if(classic())foreground=p.onAccent;}
        else if(hover)bg=p.hover;
    }else foreground=p.muted;
    if(classic()){
        FillRect(dc,&r,under.brush);fill(dc,r,bg);bevel(dc,r,down||active);
        if(primary&&enabled&&!down){RECT inner=r;InflateRect(&inner,-2,-2);fill(dc,inner,p.accent);}
    }else{
        frame(dc,r,bg,(active||primary)&&enabled?p.accent:nav&&!hover?p.sidebar:p.edge,arcade()?18:p.corners?8:0);
        if(active){RECT stripe{1,5,4,r.bottom-5};fill(dc,stripe,p.accent);}
    }
    InflateRect(&r,nav?-16:-10,-3);
    if(down&&classic())OffsetRect(&r,1,1);
    label(dc,h,r,foreground,title,nav?DT_LEFT:DT_CENTER);
    if(focus)DrawFocusRect(dc,&r);
}
inline LRESULT CALLBACK buttonProc(HWND h,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR){
    switch(message){
    case WM_MOUSEMOVE:
        if(!GetPropW(h,L"lrHover")){
            SetPropW(h,L"lrHover",HANDLE(1));TRACKMOUSEEVENT track{sizeof track,TME_LEAVE,h,0};
            TrackMouseEvent(&track);InvalidateRect(h,nullptr,FALSE);
        }
        break;
    case WM_MOUSELEAVE:RemovePropW(h,L"lrHover");InvalidateRect(h,nullptr,FALSE);break;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps;auto dc=BeginPaint(h,&ps);paintButton(h,dc);EndPaint(h,&ps);return 0;}
    case WM_PRINTCLIENT:paintButton(h,reinterpret_cast<HDC>(w));return 0;
    case WM_NCDESTROY:
        for(auto name:{L"lrHover",L"lrPrimary",L"lrActive",L"lrNav"})RemovePropW(h,name);
        RemoveWindowSubclass(h,buttonProc,1);break;
    }
    auto result=DefSubclassProc(h,message,w,l);
    switch(message){
    case WM_ENABLE:case WM_SETTEXT:case WM_SETFOCUS:case WM_KILLFOCUS:case BM_SETSTATE:case BM_SETCHECK:
        InvalidateRect(h,nullptr,FALSE);break;
    }
    return result;
}
inline void primary(HWND h){SetPropW(h,L"lrPrimary",HANDLE(1));InvalidateRect(h,nullptr,FALSE);}
inline void active(HWND h,bool on){
    if(on)SetPropW(h,L"lrActive",HANDLE(1));else RemovePropW(h,L"lrActive");
    InvalidateRect(h,nullptr,FALSE);
}
inline void paintCombo(HWND h,HDC dc){
    RECT r{};GetClientRect(h,&r);
    const auto& p=colors();
    const bool enabled=IsWindowEnabled(h)!=FALSE,focus=GetFocus()==h;
    const bool open=SendMessageW(h,CB_GETDROPPEDSTATE,0,0)!=0;
    const auto foreground=enabled?p.ink:p.muted;
    FillRect(dc,&r,behind(GetParent(h),h).brush);
    frame(dc,r,p.input,focus?p.accent:p.edge,classic()?0:p.corners?5:0);
    if(classic())bevel(dc,r,true);
    COMBOBOXINFO info{sizeof info};
    RECT drop{r.right-21,r.top+2,r.right-2,r.bottom-2};
    if(GetComboBoxInfo(h,&info)&&!IsRectEmpty(&info.rcButton))drop=info.rcButton;
    fill(dc,drop,open?p.pressed:p.surface);
    if(classic())bevel(dc,drop,open);
    auto x=(drop.left+drop.right)/2,y=(drop.top+drop.bottom)/2;
    POINT arrow[]={{x-4,y-2},{x+4,y-2},{x,y+3}};
    auto brush=CreateSolidBrush(foreground);
    if(brush){
        auto oldBrush=SelectObject(dc,brush),oldPen=SelectObject(dc,GetStockObject(NULL_PEN));
        Polygon(dc,arrow,3);
        SelectObject(dc,oldPen);SelectObject(dc,oldBrush);DeleteObject(brush);
    }
    std::wstring title;
    auto selected=SendMessageW(h,CB_GETCURSEL,0,0);
    if(selected!=CB_ERR){
        auto length=SendMessageW(h,CB_GETLBTEXTLEN,selected,0);
        if(length>0){
            title.resize(size_t(length)+1);
            SendMessageW(h,CB_GETLBTEXT,selected,reinterpret_cast<LPARAM>(title.data()));
        }
    }
    RECT text{r.left+7,r.top+1,drop.left-4,r.bottom-1};
    label(dc,h,text,foreground,title.c_str(),DT_LEFT|DT_NOPREFIX);
    if(focus&&!open){InflateRect(&text,2,-2);DrawFocusRect(dc,&text);}
}
inline LRESULT CALLBACK comboProc(HWND h,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR){
    switch(message){
    case WM_PAINT:{PAINTSTRUCT ps;auto dc=BeginPaint(h,&ps);paintCombo(h,dc);EndPaint(h,&ps);return 0;}
    case WM_PRINTCLIENT:paintCombo(h,reinterpret_cast<HDC>(w));return 0;
    case WM_NCDESTROY:RemoveWindowSubclass(h,comboProc,1);break;
    }
    auto result=DefSubclassProc(h,message,w,l);
    switch(message){
    case CB_SETCURSEL:case CB_RESETCONTENT:case CB_SHOWDROPDOWN:
    case WM_ENABLE:case WM_SETFOCUS:case WM_KILLFOCUS:case WM_KEYDOWN:case WM_CHAR:
    case WM_LBUTTONDOWN:case WM_LBUTTONUP:
        InvalidateRect(h,nullptr,FALSE);break;
    }
    return result;
}
inline BOOL CALLBACK skin(HWND h,LPARAM){
    wchar_t cls[32]{};GetClassNameW(h,cls,32);
    if(lstrcmpiW(cls,L"Button")==0){
        auto type=GetWindowLongPtrW(h,GWL_STYLE)&BS_TYPEMASK;
        if(type==BS_PUSHBUTTON||type==BS_DEFPUSHBUTTON||type==BS_CHECKBOX||type==BS_AUTOCHECKBOX)
            SetWindowSubclass(h,buttonProc,1,0);
    }
    if(lstrcmpiW(cls,PROGRESS_CLASSW)==0){
        SendMessageW(h,PBM_SETBARCOLOR,0,colors().accent);
        SendMessageW(h,PBM_SETBKCOLOR,0,colors().input);
    }
    if(lstrcmpiW(cls,WC_COMBOBOXW)==0&&(GetWindowLongPtrW(h,GWL_STYLE)&3)==CBS_DROPDOWNLIST)
        SetWindowSubclass(h,comboProc,1,0);
    return TRUE;
}
inline void apply(HWND root){EnumChildWindows(root,skin,0);}
inline void caption(HWND window){
    // Windows 11 honors caption colors and corner preferences. Older hosts ignore unsupported attributes.
    const auto& p=colors();
    const BOOL dark=p.dark;
    const DWORD corner=classic()||current==Theme::Carbon||phosphor()?1:0;
    const COLORREF bg=classic()?p.caption:0xFFFFFFFFu,fg=classic()?p.onCaption:0xFFFFFFFFu;
    DwmSetWindowAttribute(window,20,&dark,sizeof dark);
    DwmSetWindowAttribute(window,33,&corner,sizeof corner);
    DwmSetWindowAttribute(window,35,&bg,sizeof bg);
    DwmSetWindowAttribute(window,36,&fg,sizeof fg);
}
inline void redraw(HWND root){RedrawWindow(root,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);}
inline void shutdown(){
    sections.clear();
    for(auto font:{heading,small,classicHeading,classicSmall,terminalHeading})if(font)DeleteObject(font);
    for(auto& group:brushes)group.clear();
}
}
