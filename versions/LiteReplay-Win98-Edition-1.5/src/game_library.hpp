// Included in the shell translation unit to reuse its recording configuration and device catalog.
namespace games {
struct Card {
    int id=0;
    std::wstring name,categories,launcher,process,last;
    Config recording;
    int replays=0;
};
std::vector<Card> cards;
int active=0,session=0,sortColumn=0;
bool descending=false;
HWND view{};
std::filesystem::path directory(){return settings.parent_path()/L"GameBox98";}
Card* find(int id){for(auto& c:cards)if(c.id==id)return &c;return nullptr;}
void write(const Card& c){
    auto dir=directory();std::filesystem::create_directories(dir);
    auto file=dir/(std::to_wstring(c.id)+L".game");auto temp=file;temp+=L".tmp";
    std::ofstream out(temp,std::ios::binary|std::ios::trunc);
    auto& r=c.recording;
    for(auto& s:{c.name,c.categories,c.launcher,c.process,c.last,r.systemDeviceId,r.micDeviceId})out<<std::quoted(utf8(s))<<'\n';
    out<<r.seconds<<' '<<r.width<<' '<<r.fps<<' '<<r.mbps<<' '<<r.systemAudio<<' '<<r.microphone<<' '<<c.replays;
    out.close();if(!out||!MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot save game profile");
}
void load(){
    if(!std::filesystem::exists(directory()))return;
    for(auto& e:std::filesystem::directory_iterator(directory())){
        if(e.path().extension()!=L".game")continue;
        Card c;c.recording=config;auto& r=c.recording;
        std::ifstream in(e.path(),std::ios::binary);
        for(auto p:{&c.name,&c.categories,&c.launcher,&c.process,&c.last,&r.systemDeviceId,&r.micDeviceId}){
            std::string s;in>>std::quoted(s);*p=wide(s);
        }
        if(!(in>>r.seconds>>r.width>>r.fps>>r.mbps>>r.systemAudio>>r.microphone>>c.replays))continue;
        c.id=_wtoi(e.path().stem().c_str());if(c.id>0)cards.push_back(c);
    }
}
Config effective(){
    Config next=config;if(auto* c=find(active)){
        auto& r=c->recording;next.seconds=r.seconds;next.width=r.width;next.height=resolutions[resolutionIndex(r.width)].height;
        next.fps=r.fps;next.mbps=r.mbps;next.systemAudio=r.systemAudio;next.microphone=r.microphone;
        next.systemDeviceId=r.systemDeviceId;next.micDeviceId=r.micDeviceId;
        next.folder=(std::filesystem::path(next.folder)/L"GameBox98"/std::to_wstring(c->id)).wstring();
    }return next;
}
void start(){engine->start(effective());session=active;}
void countReplay(const std::wstring& path){
    auto parent=std::filesystem::path(path).parent_path();
    if(parent.parent_path().filename()!=L"GameBox98")return;
    if(auto c=find(_wtoi(parent.filename().c_str()))){++c->replays;write(*c);}
}
std::vector<std::wstring> groups(std::wstring text){
    std::replace(text.begin(),text.end(),L'；',L';');std::wistringstream input(text);std::vector<std::wstring> out;
    for(std::wstring s;std::getline(input,s,L';');){auto a=s.find_first_not_of(L" \t");if(a!=s.npos)out.push_back(s.substr(a,s.find_last_not_of(L" \t")-a+1));}return out;
}
int selected(){
    auto list=GetDlgItem(view,15);LVITEMW item{};item.mask=LVIF_PARAM;item.iItem=ListView_GetNextItem(list,-1,LVNI_SELECTED);
    return item.iItem>=0&&ListView_GetItem(list,&item)?int(item.lParam):0;
}
void render(bool categories=false){
    if(!view)return;
    HWND list=GetDlgItem(view,15),box=GetDlgItem(view,14);
    int filter=int(SendMessageW(box,LB_GETCURSEL,0,0));std::wstring group;
    if(filter>=2){auto n=SendMessageW(box,LB_GETTEXTLEN,filter,0);group.resize(n+1);SendMessageW(box,LB_GETTEXT,filter,(LPARAM)group.data());group.resize(n);}
    if(categories){
        SendMessageW(box,LB_RESETCONTENT,0,0);
        for(auto s:{L"全部游戏",L"最近游玩"})SendMessageW(box,LB_ADDSTRING,0,(LPARAM)s);
        std::set<std::wstring> names;for(auto& c:cards)for(auto& s:groups(c.categories))names.insert(s);
        for(auto& s:names)SendMessageW(box,LB_ADDSTRING,0,(LPARAM)s.c_str());
        if(filter>=2)filter=int(SendMessageW(box,LB_FINDSTRINGEXACT,-1,(LPARAM)group.c_str()));
        filter=std::max(filter,0);SendMessageW(box,LB_SETCURSEL,filter,0);
    }
    int keep=selected();ListView_DeleteAllItems(list);
    std::vector<Card*> rows;for(auto& c:cards){auto tags=groups(c.categories);
        if(filter==1&&c.last.empty())continue;
        if(filter>=2&&std::find(tags.begin(),tags.end(),group)==tags.end())continue;
        rows.push_back(&c);}
    std::sort(rows.begin(),rows.end(),[](Card* a,Card* b){
        int n=0;switch(sortColumn){case 0:n=lstrcmpiW(a->name.c_str(),b->name.c_str());break;
        case 1:n=lstrcmpiW(a->categories.c_str(),b->categories.c_str());break;
        case 2:n=(a->id==active)-(b->id==active);break;case 3:n=a->last.compare(b->last);break;
        case 4:n=a->replays-b->replays;break;}return n?(descending?n>0:n<0):a->id<b->id;});
    for(auto c:rows){LVITEMW item{};item.mask=LVIF_TEXT|LVIF_PARAM;item.iItem=ListView_GetItemCount(list);item.pszText=c->name.data();item.lParam=c->id;
        int i=ListView_InsertItem(list,&item),j=1;for(auto s:{c->categories,std::wstring(active==c->id?L"已识别":L"未运行"),c->last,std::to_wstring(c->replays)}){ListView_SetItemText(list,i,j,s.data());++j;}
        if(c->id==keep)ListView_SetItemState(list,i,LVIS_SELECTED,LVIS_SELECTED);}
    auto c=find(active);SetDlgItemTextW(view,16,c?(L"当前游戏："+c->name).c_str():L"当前未识别到游戏 · 使用全局录制设置");
}
bool restarting=false;
void scan(){
    int match=0,rank=0;DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);
    if(!cards.empty())for(auto& p:ProcessTarget::list()){
        for(auto& c:cards)for(auto target:{&c.process,&c.launcher}){
            if(target->empty()||lstrcmpiW(std::filesystem::path(*target).filename().c_str(),p.name.c_str()))continue;
            HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,p.pid);if(!h)continue;
            wchar_t path[32768];DWORD size=32768;bool ok=QueryFullProcessImageNameW(h,0,path,&size);CloseHandle(h);
            int score=1+(target==&c.process?4:0)+(foreground==p.pid?2:0)+(active==c.id?1:0);
            if(ok&&!lstrcmpiW(path,target->c_str())&&score>rank){rank=score;match=c.id;}
        }}
    if(match!=active){active=match;if(auto c=find(active)){SYSTEMTIME t;GetLocalTime(&t);wchar_t stamp[32];swprintf_s(stamp,L"%04d-%02d-%02d %02d:%02d",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute);c->last=stamp;write(*c);}render();}
    if(engine&&engine->running()&&session!=active&&!engine->saving()){engine->stop();restarting=true;}
    if(restarting&&engine&&!engine->running()){restarting=false;start();}
}

Card draft;
std::wstring field(HWND h,int id){int n=GetWindowTextLengthW(GetDlgItem(h,id));std::wstring v(n+1,0);GetDlgItemTextW(h,id,v.data(),n+1);v.resize(n);return v;}
void page(HWND h){int tab=TabCtrl_GetCurSel(GetDlgItem(h,20));for(int i=30;i<=61;++i)ShowWindow(GetDlgItem(h,i),i/10==3+tab||(tab==2&&i>=60)?SW_SHOW:SW_HIDE);}
void icon(HWND h){
    SHFILEINFOW info{};SHGetFileInfoW(field(h,41).c_str(),0,&info,sizeof(info),SHGFI_ICON|SHGFI_LARGEICON);
    auto old=(HICON)SendDlgItemMessageW(h,34,STM_SETICON,(WPARAM)info.hIcon,0);if(old)DestroyIcon(old);
}
INT_PTR CALLBACK properties(HWND h,UINT m,WPARAM w,LPARAM l){
    try{
    if(m==WM_INITDIALOG){
        COLORREF blue=RGB(0,0,128);DwmSetWindowAttribute(h,35,&blue,sizeof blue);
        int i=0;for(auto s:{L"基本信息",L"识别程序",L"录制设置"}){TCITEMW t{};t.mask=TCIF_TEXT;t.pszText=const_cast<wchar_t*>(s);TabCtrl_InsertItem(GetDlgItem(h,20),i++,&t);}
        auto& r=draft.recording;
        SetDlgItemTextW(h,31,draft.name.c_str());SetDlgItemTextW(h,33,draft.categories.c_str());SetDlgItemTextW(h,41,draft.launcher.c_str());SetDlgItemTextW(h,44,draft.process.c_str());
        SetDlgItemInt(h,51,r.seconds,FALSE);SetDlgItemInt(h,57,r.mbps,FALSE);
        auto add=[&](int id,const std::wstring& s){SendDlgItemMessageW(h,id,CB_ADDSTRING,0,(LPARAM)s.c_str());};
        for(auto f:frameRates)add(53,std::to_wstring(f));for(auto res:resolutions)add(55,res.label);
        SendDlgItemMessageW(h,53,CB_SETCURSEL,fpsIndex(r.fps),0);SendDlgItemMessageW(h,55,CB_SETCURSEL,resolutionIndex(r.width),0);
        for(int id:{59,61}){auto& choices=id==59?systemChoices:micChoices;auto& device=id==59?r.systemDeviceId:r.micDeviceId;
            int idx=-1;for(size_t n=0;n<choices.size();++n){add(id,choices[n].name);if(choices[n].id==device)idx=int(n);}
            if(idx<0){idx=int(choices.size());add(id,device.empty()?L"系统默认设备":L"已保存的设备（离线）");}
            SendDlgItemMessageW(h,id,CB_SETCURSEL,idx,0);}
        CheckDlgButton(h,58,r.systemAudio);CheckDlgButton(h,60,r.microphone);icon(h);page(h);return TRUE;
    }
    if(m==WM_NOTIFY&&((NMHDR*)l)->code==TCN_SELCHANGE){page(h);return TRUE;}
    if(m==WM_DESTROY){auto old=(HICON)SendDlgItemMessageW(h,34,STM_SETICON,0,0);if(old)DestroyIcon(old);}
    if(m!=WM_COMMAND)return FALSE;
    int cmd=LOWORD(w);
    if(cmd==42||cmd==45){wchar_t path[32768]{};OPENFILENAMEW of{sizeof(of)};of.hwndOwner=h;of.lpstrFile=path;of.nMaxFile=32768;of.lpstrFilter=L"应用程序 (*.exe)\0*.exe\0";of.Flags=OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR;
        if(GetOpenFileNameW(&of)){SetDlgItemTextW(h,cmd==42?41:44,path);if(cmd==42){icon(h);if(field(h,31).empty())SetDlgItemTextW(h,31,std::filesystem::path(path).stem().c_str());}}return TRUE;}
    if(cmd==IDCANCEL){EndDialog(h,0);return TRUE;}
    if(cmd!=IDOK)return FALSE;
    draft.name=field(h,31);draft.categories=field(h,33);draft.launcher=field(h,41);draft.process=field(h,44);
    auto valid=[](const std::wstring& s){std::filesystem::path p(s);return p.is_absolute()&&!lstrcmpiW(p.extension().c_str(),L".exe")&&std::filesystem::is_regular_file(p);};
    if(draft.name.empty()||!valid(draft.launcher)||(!draft.process.empty()&&!valid(draft.process)))throw std::runtime_error("请填写名称，并选择有效的 EXE 文件");
    auto& r=draft.recording;r.seconds=GetDlgItemInt(h,51,nullptr,FALSE);r.mbps=GetDlgItemInt(h,57,nullptr,FALSE);
    if(r.seconds<30||r.seconds>600||r.mbps<2||r.mbps>100)throw std::runtime_error("时长范围 30–600 秒，码率范围 2–100 Mbps");
    auto index=[&](int id){return size_t(SendDlgItemMessageW(h,id,CB_GETCURSEL,0,0));};
    r.fps=frameRates.at(index(53));r.width=resolutions.at(index(55)).width;r.systemAudio=IsDlgButtonChecked(h,58);r.microphone=IsDlgButtonChecked(h,60);
    if(index(59)<systemChoices.size())r.systemDeviceId=systemChoices[index(59)].id;
    if(index(61)<micChoices.size())r.micDeviceId=micChoices[index(61)].id;
    if(auto c=find(draft.id)){draft.last=c->last;draft.replays=c->replays;}
    write(draft);EndDialog(h,1);return TRUE;
    }catch(const std::exception& e){MessageBoxW(h,wide(e.what()).c_str(),L"游戏属性",MB_ICONWARNING);return TRUE;}
}
INT_PTR CALLBACK browser(HWND h,UINT m,WPARAM w,LPARAM l){
    try{
    if(m==WM_INITDIALOG){view=h;COLORREF blue=RGB(0,0,128);DwmSetWindowAttribute(h,35,&blue,sizeof blue);auto list=GetDlgItem(h,15);ListView_SetExtendedListViewStyle(list,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_DOUBLEBUFFER);
        int i=0;for(auto label:{L"游戏名称",L"分类",L"识别状态",L"最近游玩时间",L"回放数量"}){LVCOLUMNW col{};col.mask=LVCF_TEXT|LVCF_WIDTH;col.pszText=const_cast<wchar_t*>(label);int widths[]={150,110,88,150,80};col.cx=widths[i];ListView_InsertColumn(list,i,&col);++i;}
        HMENU menu=CreateMenu(),file=CreatePopupMenu();for(int id:{10,11,12,13}){auto s=field(h,id);AppendMenuW(file,MF_STRING,id,s.c_str());}AppendMenuW(menu,MF_POPUP,(UINT_PTR)file,L"游戏(&G)");SetMenu(h,menu);
        SendMessageW(h,WM_SETICON,ICON_SMALL,(LPARAM)retroIcon);render(true);return TRUE;}
    if(m==WM_SIZE){int width=LOWORD(l),height=HIWORD(l);int side=std::clamp(width/5,100,190);MoveWindow(GetDlgItem(h,14),6,45,side,std::max(30,height-80),TRUE);
        MoveWindow(GetDlgItem(h,15),side+12,45,std::max(60,width-side-18),std::max(30,height-80),TRUE);MoveWindow(GetDlgItem(h,16),8,height-28,width-16,23,TRUE);return TRUE;}
    if(m==WM_GETMINMAXINFO){((MINMAXINFO*)l)->ptMinTrackSize={530,320};return TRUE;}
    if(m==WM_NOTIFY){auto note=(NMHDR*)l;if(note->idFrom==15&&note->code==LVN_COLUMNCLICK){int col=((NMLISTVIEW*)l)->iSubItem;descending=sortColumn==col?!descending:col>=3;sortColumn=col;render();}
        if(note->idFrom==15&&note->code==NM_DBLCLK)SendMessageW(h,WM_COMMAND,11,0);}
    if(m==WM_CLOSE){DestroyWindow(h);return TRUE;}if(m==WM_DESTROY){view=nullptr;return TRUE;}
    if(m!=WM_COMMAND)return FALSE;int id=LOWORD(w);auto c=find(selected());
    if(id==IDCANCEL){DestroyWindow(h);return TRUE;}
    if(id==14&&HIWORD(w)==LBN_SELCHANGE){if(SendDlgItemMessageW(h,14,LB_GETCURSEL,0,0)==1){sortColumn=3;descending=true;}render();return TRUE;}
    if(id==13){scan();render();return TRUE;}
    if(id==11||id==12){if(!c){MessageBoxW(h,L"请先选择一个游戏。",L"游戏资料库",MB_OK);return TRUE;}}
    if(id==12){if(INT_PTR(ShellExecuteW(h,L"open",c->launcher.c_str(),nullptr,std::filesystem::path(c->launcher).parent_path().c_str(),SW_SHOWNORMAL))<=32)throw std::runtime_error("启动游戏失败");return TRUE;}
    if(id==10||id==11){draft=id==10?Card{}:*c;if(id==10){draft.recording=config;draft.id=1;for(auto& row:cards)draft.id=std::max(draft.id,row.id+1);}
        if(DialogBoxW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(7101),h,properties)==1){if(id==10)cards.push_back(draft);else { *find(draft.id)=draft;if(active==draft.id&&engine&&engine->running())session=-1; }scan();render(true);}return TRUE;}
    }catch(const std::exception& e){MessageBoxW(h,wide(e.what()).c_str(),L"游戏资料库",MB_ICONWARNING);}
    return FALSE;
}
void open(){if(!view)view=CreateDialogW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(7100),window,browser);if(!view)throw std::runtime_error("Cannot open game library");ShowWindow(view,SW_SHOWNORMAL);SetForegroundWindow(view);}
}

