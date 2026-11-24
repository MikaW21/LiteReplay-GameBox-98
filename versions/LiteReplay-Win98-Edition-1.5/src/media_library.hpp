#pragma once
#include "common.hpp"
#include "media_catalog.hpp"
#include "thumbnail_worker.hpp"
#include <commdlg.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <mfplay.h>
#include <fstream>
#include <deque>
namespace lr {
// The index never scans directories. Only explicit imports and committed saves enter it.
class MediaLibrary {
    struct Event {MFP_EVENT_TYPE type;HRESULT hr;ComPtr<IMFPMediaItem> item;};
    struct Events {std::mutex mutex;std::deque<Event> queue;};
    class Callback:public IMFPMediaPlayerCallback {
        std::atomic<ULONG> refs_{1};std::shared_ptr<Events> events_;
    public:
        explicit Callback(std::shared_ptr<Events> q):events_(std::move(q)){}
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** p)override{if(!p)return E_POINTER;*p=nullptr;if(id==__uuidof(IUnknown)||id==__uuidof(IMFPMediaPlayerCallback)){*p=this;AddRef();return S_OK;}return E_NOINTERFACE;}
        ULONG STDMETHODCALLTYPE AddRef()override{return ++refs_;}
        ULONG STDMETHODCALLTYPE Release()override{auto n=--refs_;if(!n)delete this;return n;}
        void STDMETHODCALLTYPE OnMediaPlayerEvent(MFP_EVENT_HEADER* e)override{
            Event out{e->eEventType,e->hrEvent,{}};
            if(e->eEventType==MFP_EVENT_TYPE_MEDIAITEM_CREATED)out.item=reinterpret_cast<MFP_MEDIAITEM_CREATED_EVENT*>(e)->pMediaItem;
            std::lock_guard<std::mutex> lock(events_->mutex);if(events_->queue.size()<32)events_->queue.push_back(std::move(out));
        }
    };
    HWND page_,list_,query_,types_,dates_,info_,name_,preview_,seek_;
    HFONT font_;std::function<const wchar_t*(const char*)> tr_;std::function<void(const std::wstring&)> edit_;
    std::filesystem::path index_;
    MediaCatalog catalog_;bool healthy_=true,dragging_=false,playing_=false,rebuilding_=false;
    uint64_t selected_=0;std::vector<uint64_t> visible_;
    ComPtr<IMFPMediaPlayer> player_;ComPtr<Callback> callback_;
    std::shared_ptr<Events> events_;HMODULE mf_=nullptr;LONGLONG duration_=0;
    HBITMAP bitmap_=nullptr;std::wstring thumbKey_,thumbPath_;
    ThumbnailCache thumbnails_;ThumbnailJob thumbnailJob_;
    enum {Import=1100,Search,Types,Dates,List,Play,Pause,Stop,Reveal,Rename,Remove,Delete,Refresh,Name,Seek,Edit,
        Actions,FileMenu,PlaybackMenu};
    const wchar_t* t(const char* key)const{return tr_(key);}
    static std::wstring text(HWND h){int n=GetWindowTextLengthW(h);std::wstring s(size_t(n)+1,0);GetWindowTextW(h,s.data(),n+1);s.resize(n);return s;}
    void status(const std::wstring& s){SetWindowTextW(info_,s.c_str());}
    HWND add(const wchar_t* cls,const wchar_t* title,int id,int x,int y,int w,int h,DWORD style=0){
        HWND c=CreateWindowExW(0,cls,title,WS_CHILD|WS_VISIBLE|style,x,y,w,h,page_,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
        if(!c)throw std::runtime_error("Cannot create library control");SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(font_),TRUE);return c;
    }
    static std::wstring lower(std::wstring s){if(!s.empty())CharLowerBuffW(s.data(),DWORD(s.size()));return s;}
    static MediaAsset inspect(const std::filesystem::path& input){
        auto path=std::filesystem::absolute(input).lexically_normal();auto type=lower(path.extension().wstring());
        if(type!=L".mp4"&&type!=L".mkv"&&type!=L".mov"&&type!=L".png")throw std::runtime_error("Only MP4, MKV, MOV and PNG are supported");
        WIN32_FILE_ATTRIBUTE_DATA d{};if(!GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&d)||d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)throw std::runtime_error("Media file missing or inaccessible");
        SYSTEMTIME st{};FILETIME local{};FileTimeToLocalFileTime(&d.ftLastWriteTime,&local);FileTimeToSystemTime(&local,&st);wchar_t date[32];swprintf_s(date,L"%04u-%02u-%02u",st.wYear,st.wMonth,st.wDay);
        MediaAsset a;a.path=utf8(path.wstring());a.key=utf8(lower(path.wstring()));a.date=utf8(date);a.type=utf8(type.substr(1));a.bytes=(uint64_t(d.nFileSizeHigh)<<32)|d.nFileSizeLow;a.stamp=(uint64_t(d.ftLastWriteTime.dwHighDateTime)<<32)|d.ftLastWriteTime.dwLowDateTime;return a;
    }
    void commit(MediaCatalog next){
        if(!healthy_)throw std::runtime_error("Library index is damaged/read-only; preserve media.library and repair or rename it before restarting");
        std::string data=next.serialize();if(data.size()>32*1024*1024)throw std::runtime_error("Library index limit reached (32 MiB)");auto temp=index_;temp+=L".tmp";
        HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot write library index");
        DWORD written=0;BOOL ok=WriteFile(file,data.data(),DWORD(data.size()),&written,nullptr);if(ok)ok=FlushFileBuffers(file);CloseHandle(file);
        if(!ok||written!=data.size()){DeleteFileW(temp.c_str());throw std::runtime_error("Library index write failed");}
        if(!MoveFileExW(temp.c_str(),index_.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){DeleteFileW(temp.c_str());throw std::runtime_error("Cannot commit library index");}
        catalog_=std::move(next);
    }
    MediaAsset current()const{for(auto& a:catalog_.rows())if(a.id==selected_)return a;throw std::runtime_error("Select a media item first");}
    void rebuildDates(){
        auto old=text(dates_);SendMessageW(dates_,CB_RESETCONTENT,0,0);SendMessageW(dates_,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(t("lib.all_dates")));
        std::set<std::string,std::greater<std::string>> days;for(auto& a:catalog_.rows())days.insert(a.date);
        int chosen=0,n=1;for(auto& day:days){auto w=wide(day);SendMessageW(dates_,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(w.c_str()));if(w==old)chosen=n;++n;}SendMessageW(dates_,CB_SETCURSEL,chosen,0);
    }
    void rebuild(){
        auto q=lower(text(query_));int kind=int(SendMessageW(types_,CB_GETCURSEL,0,0));auto date=text(dates_);bool all=SendMessageW(dates_,CB_GETCURSEL,0,0)<=0;
        visible_.clear();std::vector<MediaAsset> rows=catalog_.rows();std::sort(rows.begin(),rows.end(),[](auto& a,auto& b){return a.date!=b.date?a.date>b.date:a.id>b.id;});
        struct Repaint {
            HWND view;bool& flag;
            ~Repaint(){flag=false;SendMessageW(view,WM_SETREDRAW,TRUE,0);InvalidateRect(view,nullptr,TRUE);}
        } repaint{list_,rebuilding_};
        rebuilding_=true;
        SendMessageW(list_,WM_SETREDRAW,FALSE,0);ListView_DeleteAllItems(list_);
        for(auto& a:rows){if(!q.empty()&&lower(wide(a.path)).find(q)==std::wstring::npos)continue;
            if(!all&&wide(a.date)!=date)continue;const char* extensions[]={"","mp4","mkv","mov","png"};if(kind>0&&kind<5&&a.type!=extensions[kind])continue;
            auto title=std::filesystem::path(wide(a.path)).filename().wstring();
            LVITEMW entry{};entry.mask=LVIF_TEXT;entry.iItem=int(visible_.size());entry.pszText=title.data();
            int row=ListView_InsertItem(list_,&entry);
            if(row<0)continue;
            visible_.push_back(a.id);
            std::array<std::wstring,3> data={wide(a.type),std::to_wstring(a.bytes/1024)+L" KiB",wide(a.date)};
            for(int i=0;i<3;++i)ListView_SetItemText(list_,row,i+1,data[i].data());
            if(a.id==selected_)ListView_SetItemState(list_,row,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
        }
        if(std::find(visible_.begin(),visible_.end(),selected_)==visible_.end()){
            selected_=0;SetWindowTextW(name_,L"");clearThumbnail();
            if(healthy_)status(L"");
        }
    }
    void clearThumbnail(){KillTimer(page_,74);KillTimer(page_,75);thumbnailJob_.cancel();if(bitmap_)DeleteObject(bitmap_);bitmap_=nullptr;thumbKey_.clear();InvalidateRect(preview_,nullptr,FALSE);}
    void select(){
        stop();int i=ListView_GetNextItem(list_,-1,LVNI_SELECTED);
        selected_=i>=0&&size_t(i)<visible_.size()?visible_[size_t(i)]:0;
        if(!selected_){SetWindowTextW(name_,L"");clearThumbnail();status(L"");return;}
        auto a=current();auto path=std::filesystem::path(wide(a.path));SetWindowTextW(name_,path.filename().c_str());
        status(wide(a.path)+L"\r\n"+wide(a.date)+L"   "+std::to_wstring(a.bytes/1024)+L" KiB   "+wide(a.type));
        auto key=wide(a.path)+L"|"+std::to_wstring(a.bytes)+L"|"+std::to_wstring(a.stamp);
        if(key!=thumbKey_){clearThumbnail();thumbKey_=key;thumbPath_=path.wstring();
            if(auto cached=thumbnails_.get(key))showThumbnail(*cached);
            else SetTimer(page_,74,400,nullptr); // Selection debounce; only one requested item.
        }
    }
    void showThumbnail(const ThumbnailPixels& pixels){
        if(pixels.pixels.empty())return;
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=pixels.width;info.bmiHeader.biHeight=-pixels.height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        void* bits=nullptr;bitmap_=CreateDIBSection(nullptr,&info,DIB_RGB_COLORS,&bits,nullptr,0);
        if(bitmap_){memcpy(bits,pixels.pixels.data(),pixels.pixels.size()*4);InvalidateRect(preview_,nullptr,FALSE);}
    }

    static LRESULT CALLBACK paintPreview(HWND h,UINT msg,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data){
        auto self=reinterpret_cast<MediaLibrary*>(data);
        if(msg==WM_PAINT){
            PAINTSTRUCT ps{};auto dc=BeginPaint(h,&ps);RECT r{};GetClientRect(h,&r);
            FillRect(dc,&r,reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
            if(!self->player_&&self->bitmap_){
                BITMAP image{};GetObjectW(self->bitmap_,sizeof(image),&image);
                double scale=std::min(double(r.right)/image.bmWidth,double(r.bottom)/image.bmHeight);
                int width=int(image.bmWidth*scale),height=int(image.bmHeight*scale);
                HDC source=CreateCompatibleDC(dc);
                if(source){
                    auto old=SelectObject(source,self->bitmap_);
                    SetStretchBltMode(dc,HALFTONE);SetBrushOrgEx(dc,0,0,nullptr);
                    StretchBlt(dc,(r.right-width)/2,(r.bottom-height)/2,width,height,source,0,0,image.bmWidth,image.bmHeight,SRCCOPY);
                    SelectObject(source,old);DeleteDC(source);
                }
            }
            EndPaint(h,&ps);if(self->player_)self->player_->UpdateVideo();return 0;
        }
        return DefSubclassProc(h,msg,w,l);
    }
    void play(){
        KillTimer(page_,74);KillTimer(page_,75);thumbnailJob_.cancel();
        stop();auto a=current();
        if(a.type=="png"){if(INT_PTR(ShellExecuteW(page_,L"open",wide(a.path).c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)throw std::runtime_error("Cannot open image viewer / 无法打开图片查看器");status(t("lib.image_opened"));return;}
        if(!mf_)mf_=LoadLibraryW(L"mfplay.dll");if(!mf_)throw std::runtime_error("Windows Media Foundation is unavailable; replay recording is unaffected");
        using Create=HRESULT(WINAPI*)(LPCWSTR,BOOL,MFP_CREATION_OPTIONS,IMFPMediaPlayerCallback*,HWND,IMFPMediaPlayer**);
        auto create=reinterpret_cast<Create>(GetProcAddress(mf_,"MFPCreateMediaPlayer"));if(!create)throw std::runtime_error("MFPlay is unavailable");
        events_=std::make_shared<Events>();callback_.Attach(new Callback(events_));
        check(create(nullptr,FALSE,0,callback_.Get(),preview_,&player_),"Create preview");
        check(player_->CreateMediaItemFromURL(wide(a.path).c_str(),FALSE,0,nullptr),"Open preview");SetTimer(page_,72,100,nullptr);status(t("lib.loading"));
    }
    void pump(){
        if(!events_||!player_)return;std::deque<Event> events;
        {std::lock_guard<std::mutex> lock(events_->mutex);events.swap(events_->queue);}
        for(auto& e:events){if(FAILED(e.hr)){stop();check(e.hr,"Preview unavailable (check installed Windows codecs)");}
            if(e.type==MFP_EVENT_TYPE_MEDIAITEM_CREATED){check(player_->SetMediaItem(e.item.Get()),"Set preview media");}
            if(e.type==MFP_EVENT_TYPE_MEDIAITEM_SET){PROPVARIANT v{};if(SUCCEEDED(player_->GetDuration(MFP_POSITIONTYPE_100NS,&v))&&v.vt==VT_I8)duration_=v.hVal.QuadPart;PropVariantClear(&v);
                SIZE size{};auto hr=player_->GetNativeVideoSize(&size,nullptr);status((SUCCEEDED(hr)&&size.cx>0?std::to_wstring(size.cx)+L" x "+std::to_wstring(size.cy):L"N/A")+std::wstring(L"   ")+(duration_>0?std::to_wstring(duration_/10000000)+L" s":L"N/A"));check(player_->Play(),"Play preview");playing_=true;SetTimer(page_,72,500,nullptr);}
            if(e.type==MFP_EVENT_TYPE_PLAYBACK_ENDED){stop();return;}
        }
        if(player_&&duration_>0&&!dragging_){PROPVARIANT p{};if(SUCCEEDED(player_->GetPosition(MFP_POSITIONTYPE_100NS,&p))&&p.vt==VT_I8)SendMessageW(seek_,TBM_SETPOS,TRUE,LPARAM(10000.0*p.hVal.QuadPart/duration_));PropVariantClear(&p);}
    }
    void importFiles(){
        std::vector<wchar_t> buffer(65536);OPENFILENAMEW of{sizeof of};of.hwndOwner=page_;of.lpstrFilter=L"Media\0*.mp4;*.mkv;*.mov;*.png\0\0";of.lpstrFile=buffer.data();of.nMaxFile=DWORD(buffer.size());of.Flags=OFN_EXPLORER|OFN_ALLOWMULTISELECT|OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR;
        if(!GetOpenFileNameW(&of)){if(CommDlgExtendedError())throw std::runtime_error("File selection failed / too many paths");return;}
        std::filesystem::path first=buffer.data();const wchar_t* p=buffer.data()+wcslen(buffer.data())+1;auto next=catalog_;
        if(*p){while(*p){next.add(inspect(first/p));p+=wcslen(p)+1;}}else next.add(inspect(first));
        commit(std::move(next));rebuildDates();rebuild();status(t("lib.imported"));
    }
    void rename(){
        if(!healthy_)throw std::runtime_error("Library index is read-only");
        auto a=current();auto source=std::filesystem::path(wide(a.path));std::wstring n=text(name_);
        if(n.empty()||n.size()>240||n.find_first_of(L"<>:\"/\\|?*")!=std::wstring::npos||n.back()==L'.'||n.back()==L' '||std::any_of(n.begin(),n.end(),[](wchar_t c){return c<32;})||lower(std::filesystem::path(n).extension().wstring())!=lower(source.extension().wstring()))throw std::runtime_error("Invalid filename; keep the original extension");
        auto dest=source.parent_path()/n;if(dest==source)return;
        if(MessageBoxW(page_,t("lib.rename_warn"),t("lib.rename"),MB_OKCANCEL|MB_ICONWARNING)!=IDOK)return;
        stop();clearThumbnail();if(!MoveFileW(source.c_str(),dest.c_str()))throw std::runtime_error("Cannot rename (target exists, locked, or inaccessible)");
        try{auto next=catalog_;auto b=inspect(dest);for(auto& entry:next.rows())if(entry.id!=a.id&&entry.key==b.key)throw std::runtime_error("Destination already exists in library index");b.id=a.id;*next.find(a.id)=b;commit(std::move(next));}
        catch(...){if(!MoveFileW(dest.c_str(),source.c_str()))status(L"Index update and rename rollback failed. Check both filenames.");throw;}
        clearThumbnail();rebuildDates();rebuild();select();
    }
    void openMenu(int anchor,const POINT* point=nullptr){
        HMENU menu=CreatePopupMenu();if(!menu)return;
        auto append=[&](int id,const char* label,bool enabled){
            AppendMenuW(menu,MF_STRING|(enabled?MF_ENABLED:MF_GRAYED),id,t(label));
        };
        if(point){
            append(Import,"lib.import",true);
            if(selected_){
                AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
                append(Play,"lib.transport_play",true);
                append(Edit,"lib.edit",current().type!="png");
                append(Reveal,"lib.reveal",true);
                append(Remove,"lib.remove",true);
                append(Delete,"lib.delete",true);
            }
        }else if(anchor==FileMenu){
            append(Import,"lib.import",true);
            append(Reveal,"lib.reveal",selected_!=0);
        }else if(anchor==PlaybackMenu){
            append(Play,"lib.transport_play",selected_!=0);
            append(Pause,"lib.transport_pause",player_.Get()!=nullptr);
            append(Stop,"lib.transport_stop",player_.Get()!=nullptr);
        }else{
            append(Refresh,"lib.refresh",selected_!=0);
            AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
            append(Remove,"lib.remove",selected_!=0);
            append(Delete,"lib.delete",selected_!=0);
        }
        RECT button{};GetWindowRect(GetDlgItem(page_,anchor),&button);
        UINT choice=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point?point->x:button.left,point?point->y:button.bottom,0,page_,nullptr);
        DestroyMenu(menu);
        if(choice)SendMessageW(page_,WM_COMMAND,choice,0);
    }
public:
    MediaLibrary(HWND page,HFONT font,std::filesystem::path index,std::function<const wchar_t*(const char*)> translate,std::function<void(const std::wstring&)> edit={}):page_(page),font_(font),tr_(std::move(translate)),index_(std::move(index)),edit_(std::move(edit)){
        add(L"BUTTON",L"",FileMenu,0,0,1,1,WS_TABSTOP);
        add(L"BUTTON",L"",PlaybackMenu,0,0,1,1,WS_TABSTOP);
        add(L"BUTTON",L"",Actions,0,0,1,1,WS_TABSTOP);
        query_=add(L"EDIT",L"",Search,165,70,260,28,WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP);
        add(L"STATIC",t("lib.search"),1190,165,47,260,20);
        types_=add(WC_COMBOBOXW,L"",Types,435,70,110,180,CBS_DROPDOWNLIST|WS_TABSTOP);
        dates_=add(WC_COMBOBOXW,L"",Dates,555,70,160,240,CBS_DROPDOWNLIST|WS_TABSTOP);
        list_=add(WC_LISTVIEWW,L"",List,20,115,695,185,LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS|WS_BORDER|WS_TABSTOP);
        ListView_SetExtendedListViewStyle(list_,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_LABELTIP);
        const int widths[]={352,74,101,139};
        for(int i=0;i<4;++i){
            LVCOLUMNW column{};column.mask=LVCF_WIDTH;column.cx=widths[i];
            ListView_InsertColumn(list_,i,&column);
        }
        name_=add(L"EDIT",L"",Name,20,315,335,28,WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP);
        add(L"BUTTON",t("lib.rename"),Rename,365,315,105,28,WS_TABSTOP);
        preview_=add(L"STATIC",L"",0,20,400,480,270,SS_BLACKRECT|WS_BORDER);SetWindowSubclass(preview_,paintPreview,1,reinterpret_cast<DWORD_PTR>(this));
        seek_=add(TRACKBAR_CLASSW,L"",Seek,20,680,480,30,WS_TABSTOP);SendMessageW(seek_,TBM_SETRANGE,TRUE,MAKELPARAM(0,10000));
        add(L"BUTTON",t("lib.play"),Play,20,720,150,28,WS_TABSTOP);add(L"BUTTON",t("lib.pause"),Pause,180,720,150,28,WS_TABSTOP);add(L"BUTTON",t("lib.stop"),Stop,340,720,160,28,WS_TABSTOP);
        add(L"BUTTON",t("lib.edit"),Edit,520,720,195,28,WS_TABSTOP);
        info_=add(L"STATIC",L"",0,20,765,695,95);
        try{std::error_code ec;if(std::filesystem::exists(index_,ec)){if(std::filesystem::file_size(index_)>32*1024*1024)throw std::runtime_error("Library index too large");std::ifstream f(index_,std::ios::binary);if(!f)throw std::runtime_error("Cannot read library");std::ostringstream data;data<<f.rdbuf();catalog_.load(data.str());}if(ec)throw std::runtime_error("Library index inaccessible");}
        catch(const std::exception& e){healthy_=false;status(wide(e.what()));}
        localize();
    }
    ~MediaLibrary(){stop();clearThumbnail();RemoveWindowSubclass(preview_,paintPreview,1);if(mf_)FreeLibrary(mf_);}
    void stop(){KillTimer(page_,72);if(player_)player_->Shutdown();player_.Reset();callback_.Reset();events_.reset();duration_=0;playing_=false;dragging_=false;SendMessageW(seek_,TBM_SETPOS,TRUE,0);if(preview_)InvalidateRect(preview_,nullptr,TRUE);}
    void layout(int width,int height,int offsetX,int offsetY){
        const int contentWidth=width-24;
        const int transportY=height*56/100;
        const int listY=transportY+90;
        auto place=[&](HWND h,int x,int y,int w,int hgt){MoveWindow(h,x-offsetX,y-offsetY,w,hgt,TRUE);};
        auto item=[&](int id,int x,int y,int w,int hgt){place(GetDlgItem(page_,id),x,y,w,hgt);};
        item(FileMenu,12,8,80,24);item(PlaybackMenu,97,8,95,24);item(Actions,197,8,120,24);
        place(preview_,12,39,contentWidth,transportY-43);
        place(seek_,12,transportY,contentWidth,22);
        item(Play,12,transportY+24,116,25);
        item(Pause,133,transportY+24,134,25);
        item(Stop,272,transportY+24,80,25);
        item(Edit,width-162,transportY+24,150,25);
        item(1190,12,transportY+62,172,22);
        place(query_,190,transportY+60,width-430,24);
        place(types_,width-234,transportY+60,106,180);
        place(dates_,width-122,transportY+60,110,240);
        place(list_,12,listY,contentWidth,height-listY-78);
        place(name_,12,height-72,contentWidth-88,24);item(Rename,width-94,height-72,82,24);
        place(info_,12,height-44,contentWidth,40);
        ListView_SetColumnWidth(list_,0,contentWidth-284);
        ListView_SetColumnWidth(list_,1,70);
        ListView_SetColumnWidth(list_,2,81);
        ListView_SetColumnWidth(list_,3,114);
        if(player_)player_->UpdateVideo();
    }
    void hide(){KillTimer(page_,73);stop();clearThumbnail();thumbnails_.clear();rebuild();}
    void show(){if(selected_&&thumbKey_.empty())select();}
    void thumbnailBudget(size_t bytes){thumbnails_.limit(bytes);}
    void addSaved(const std::wstring& path){auto next=catalog_;next.add(inspect(path));commit(std::move(next));rebuildDates();rebuild();}
    void localize(){
        SetDlgItemTextW(page_,1190,t("lib.search"));
        SetDlgItemTextW(page_,Actions,t("lib.actions"));
        SetDlgItemTextW(page_,FileMenu,t("lib.file_menu"));
        SetDlgItemTextW(page_,PlaybackMenu,t("lib.playback_menu"));
        const char* keys[]={"retro.file_name","retro.file_type","retro.file_size","retro.file_date"};
        for(int i=0;i<4;++i){
            std::wstring title=t(keys[i]);
            LVCOLUMNW column{};column.mask=LVCF_TEXT;column.pszText=title.data();
            ListView_SetColumn(list_,i,&column);
        }
        for(auto entry:{std::pair<int,const char*>{Import,"lib.import"},{Rename,"lib.rename"},{Play,"lib.transport_play"},{Pause,"lib.transport_pause"},{Stop,"lib.transport_stop"},{Edit,"lib.edit"}})SetDlgItemTextW(page_,entry.first,t(entry.second));
        int type=int(SendMessageW(types_,CB_GETCURSEL,0,0));SendMessageW(types_,CB_RESETCONTENT,0,0);for(auto value:{t("lib.all_types"),L"MP4",L"MKV",L"MOV",L"PNG"})SendMessageW(types_,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(value));SendMessageW(types_,CB_SETCURSEL,std::max(0,type),0);rebuildDates();rebuild();
    }
    bool message(UINT msg,WPARAM w,LPARAM l){
        if(msg==WM_CONTEXTMENU&&reinterpret_cast<HWND>(w)==list_){
            POINT pt{GET_X_LPARAM(l),GET_Y_LPARAM(l)};
            if(l==-1){
                RECT box{};
                int row=ListView_GetNextItem(list_,-1,LVNI_SELECTED);
                if(row>=0)ListView_GetItemRect(list_,row,&box,LVIR_BOUNDS);
                pt={box.left+8,box.bottom+2};ClientToScreen(list_,&pt);
            }else{
                LVHITTESTINFO hit{};hit.pt=pt;ScreenToClient(list_,&hit.pt);
                int row=ListView_HitTest(list_,&hit);
                ListView_SetItemState(list_,row,row>=0?LVIS_SELECTED|LVIS_FOCUSED:0,LVIS_SELECTED|LVIS_FOCUSED);
            }
            openMenu(Actions,&pt);return true;
        }
        if(msg==WM_NOTIFY&&reinterpret_cast<NMHDR*>(l)->hwndFrom==list_){
            const auto* note=reinterpret_cast<NMHDR*>(l);
            if(note->code==LVN_ITEMCHANGED&&!rebuilding_){
                auto* change=reinterpret_cast<NMLISTVIEW*>(l);
                if((change->uChanged&LVIF_STATE)&&((change->uNewState^change->uOldState)&LVIS_SELECTED))select();
            }
            if(note->code==NM_DBLCLK&&reinterpret_cast<NMITEMACTIVATE*>(l)->iItem>=0&&selected_)play();
            return true;
        }
        if(msg==WM_TIMER&&w==74){KillTimer(page_,74);
            if(thumbnailJob_.start(thumbPath_))SetTimer(page_,75,200,nullptr);else thumbnails_.put(thumbKey_,{});return true;}
        if(msg==WM_TIMER&&w==75){ThumbnailPixels pixels;if(thumbnailJob_.poll(pixels)){KillTimer(page_,75);showThumbnail(pixels);thumbnails_.put(thumbKey_,std::move(pixels));}return true;}
        if(msg==WM_TIMER&&w==72){try{pump();}catch(...){stop();throw;}return true;}
        if(msg==WM_HSCROLL&&reinterpret_cast<HWND>(l)==seek_){if(player_&&duration_>0){dragging_=LOWORD(w)==TB_THUMBTRACK;if(!dragging_){PROPVARIANT p{};p.vt=VT_I8;p.hVal.QuadPart=duration_/10000*SendMessageW(seek_,TBM_GETPOS,0,0);check(player_->SetPosition(MFP_POSITIONTYPE_100NS,&p),"Seek preview");}}return true;}
        if(msg!=WM_COMMAND||LOWORD(w)<Import||LOWORD(w)>PlaybackMenu)return false;
        int id=LOWORD(w),code=HIWORD(w);
        if(id==Actions||id==FileMenu||id==PlaybackMenu){openMenu(id);return true;}
        if(id==Search&&code==EN_CHANGE){SetTimer(page_,73,200,nullptr);return true;}
        if((id==Types||id==Dates)&&code==CBN_SELCHANGE){stop();rebuild();return true;}
        if(id==Edit){auto a=current();if(a.type=="png")throw std::runtime_error("Select a video / 请选择视频");stop();if(edit_)edit_(wide(a.path));return true;}
        if(id==Import)importFiles();if(id==Play)play();if(id==Stop)stop();
        if(id==Pause&&player_){if(playing_){check(player_->Pause(),"Pause");playing_=false;KillTimer(page_,72);}else{check(player_->Play(),"Resume");playing_=true;SetTimer(page_,72,500,nullptr);}}
        if(id==Reveal){auto a=current();auto path=std::filesystem::path(wide(a.path));auto arg=L"/select,\""+path.wstring()+L"\"";if(INT_PTR(ShellExecuteW(page_,L"open",L"explorer.exe",arg.c_str(),nullptr,SW_SHOWNORMAL))<=32)throw std::runtime_error("Cannot open Explorer");}
        if(id==Rename)rename();
        if(id==Remove||id==Delete){auto a=current();if(MessageBoxW(page_,t(id==Remove?"lib.remove_warn":"lib.delete_warn"),t(id==Remove?"lib.remove":"lib.delete"),MB_YESNO|MB_ICONWARNING)!=IDYES)return true;stop();clearThumbnail();
            if(id==Delete){auto path=wide(a.path);path.push_back(0);path.push_back(0);SHFILEOPSTRUCTW op{};op.hwnd=page_;op.wFunc=FO_DELETE;op.pFrom=path.c_str();op.fFlags=FOF_ALLOWUNDO;if(SHFileOperationW(&op)||op.fAnyOperationsAborted)throw std::runtime_error("File deletion canceled or failed");}
            auto next=catalog_;next.remove(a.id);commit(std::move(next));selected_=0;clearThumbnail();rebuildDates();rebuild();status(t("lib.done"));}
        if(id==Refresh){thumbnails_.erase(thumbKey_);auto a=current();auto next=catalog_;auto updated=inspect(wide(a.path));updated.id=a.id;*next.find(a.id)=updated;commit(std::move(next));clearThumbnail();rebuildDates();rebuild();select();}
        return true;
    }
    bool debounce(UINT msg,WPARAM w){if(msg==WM_TIMER&&w==73){KillTimer(page_,73);stop();rebuild();return true;}return false;}
};
}
