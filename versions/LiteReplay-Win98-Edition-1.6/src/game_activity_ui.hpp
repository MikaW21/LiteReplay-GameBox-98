struct ActivityView { int mode,game; };
void selectGameMedia(int game){
    if(!mediaLibrary)return;
    std::function<bool(const std::wstring&)> filter;
    if(game>=0)filter=[game](const std::wstring& path){return classify(path).context.game==game;};
    mediaLibrary->filterByGame(std::move(filter),game<0?L"按游戏筛选":gameName(game)+L" ▾");
}
void showGameMedia(int game){
    if(view)ShowWindow(view,SW_HIDE);
    ShowWindow(window,SW_RESTORE);
    SendMessageW(window,WM_COMMAND,RetroDesktop::OpenBase+3,0);
    selectGameMedia(game);
    SetForegroundWindow(window);
}
void pickMediaGame(HWND anchor){
    HMENU menu=CreatePopupMenu();if(!menu)return;
    AppendMenuW(menu,MF_STRING,1,L"全部素材");AppendMenuW(menu,MF_STRING,2,L"未归类");
    for(size_t i=0;i<cards.size();++i)AppendMenuW(menu,MF_STRING,UINT_PTR(i+3),cards[i].name.c_str());
    RECT rect{};GetWindowRect(anchor,&rect);
    auto choice=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,rect.left,rect.bottom,0,window,nullptr);
    DestroyMenu(menu);
    if(choice)selectGameMedia(choice<=2?int(choice)-2:cards.at(choice-3).id);
}
void fillGames(HWND h,int id,int game,bool all){
    auto combo=GetDlgItem(h,id);
    auto add=[&](int value,const wchar_t* name){
        auto row=SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)name);
        if(row<0)return;
        SendMessageW(combo,CB_SETITEMDATA,row,value);
        if(value==game)SendMessageW(combo,CB_SETCURSEL,row,0);
    };
    if(all)add(-1,L"所有游戏");
    add(0,L"未归类");
    for(auto& c:cards)add(c.id,c.name.c_str());
}
int gameSelection(HWND h,int id){
    int selected=int(SendDlgItemMessageW(h,id,CB_GETCURSEL,0,0));
    return selected<0?-1:int(SendDlgItemMessageW(h,id,CB_GETITEMDATA,selected,0));
}
void columns(HWND list,std::initializer_list<std::pair<const wchar_t*,int>> definitions){
    int index=0;
    for(auto [title,width]:definitions){
        LVCOLUMNW col{};col.mask=LVCF_TEXT|LVCF_WIDTH;col.cx=width;col.pszText=const_cast<wchar_t*>(title);
        ListView_InsertColumn(list,index++,&col);
    }
    ListView_SetExtendedListViewStyle(list,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);
}
void row(HWND list,LPARAM id,std::initializer_list<std::wstring> values){
    auto it=values.begin();LVITEMW item{};item.mask=LVIF_TEXT|LVIF_PARAM;item.lParam=id;
    item.iItem=ListView_GetItemCount(list);item.pszText=const_cast<wchar_t*>(it->c_str());
    int index=ListView_InsertItem(list,&item),col=1;
    for(++it;it!=values.end();++it)ListView_SetItemText(list,index,col++,const_cast<wchar_t*>(it->c_str()));
}
LPARAM selectedRow(HWND list){
    LVITEMW item{};item.mask=LVIF_PARAM;item.iItem=ListView_GetNextItem(list,-1,LVNI_SELECTED);
    return item.iItem>=0&&ListView_GetItem(list,&item)?item.lParam:0;
}
const wchar_t* kindName(MediaKind kind){
    static const wchar_t* labels[]={L"导入素材",L"回放",L"截图",L"导出"};
    return labels[int(kind)];
}
std::wstring sizeText(uint64_t bytes){
    wchar_t text[64];
    if(bytes>=1024ull*1024*1024)swprintf_s(text,L"%.2f GiB",double(bytes)/(1024*1024*1024));
    else swprintf_s(text,L"%.2f MiB",double(bytes)/(1024*1024));
    return text;
}
Visit* selectedVisit(HWND h){
    auto id=uint64_t(selectedRow(GetDlgItem(h,81)));
    for(auto& v:visits)if(v.id==id)return &v;
    return nullptr;
}
void visitAssets(HWND h){
    HWND list=GetDlgItem(h,82);ListView_DeleteAllItems(list);
    if(auto v=selectedVisit(h))for(auto& [path,a]:associations)if(a.context.visit==v->id){
        std::error_code ec;
        row(list,0,{kindName(a.kind),std::filesystem::exists(path,ec)?path:path+L" [已移动或不可访问]"});
    }
}
void renderActivity(HWND h,const ActivityView& state){
    auto list=GetDlgItem(h,81);ListView_DeleteAllItems(list);
    int game=gameSelection(h,80);
    if(state.mode==0){
        auto filter=field(h,83);
        std::map<uint64_t,std::array<unsigned,4>> counts;
        for(auto& [path,a]:associations)++counts[a.context.visit][int(a.kind)];
        for(auto v=visits.rbegin();v!=visits.rend();++v){
            auto start=dateText(v->start);
            if((game>=0&&game!=v->game)||(!filter.empty()&&start.rfind(filter,0)!=0))continue;
            auto end=v->end<0?L"结束时间待修正":dateText(v->end);
            auto duration=v->end<0?L"—":std::to_wstring(std::max<int64_t>(0,(v->end?v->end:now())-v->start))+L" 秒";
            auto& n=counts[v->id];
            row(list,LPARAM(v->id),{gameName(v->game),start,end,duration,
                std::to_wstring(n[1])+L" / "+std::to_wstring(n[2])+L" / "+std::to_wstring(n[3])});
        }
        visitAssets(h);return;
    }
    uint64_t totals[4]{};
    unsigned unavailable=0;
    if(mediaLibrary)for(auto& asset:mediaLibrary->assets()){
        auto path=wide(asset.path);auto a=classify(path);
        if(game>=0&&a.context.game!=game)continue;
        std::error_code ec;auto bytes=std::filesystem::file_size(path,ec);
        if(ec)++unavailable;else totals[int(a.kind)]+=bytes;
        row(list,LPARAM(asset.id),{gameName(a.context.game),kindName(a.kind),ec?L"已移动或不可访问":sizeText(bytes),path});
    }
    auto total=totals[0]+totals[1]+totals[2]+totals[3];
    auto text=L"合计 "+sizeText(total)+L"    回放 "+sizeText(totals[1])+L"    截图 "+sizeText(totals[2])+
        L"    导出 "+sizeText(totals[3])+L"    不可访问 "+std::to_wstring(unavailable)+L" 项";
    SetDlgItemTextW(h,86,text.c_str());
    ULARGE_INTEGER free{};
    auto location=config.folder+L" · "+(GetDiskFreeSpaceExW(storageDirectory(config.folder).c_str(),&free,nullptr,nullptr)
        ?L"可用 "+sizeText(free.QuadPart):L"无法读取可用空间");
    SetDlgItemTextW(h,87,location.c_str());
}
void settingsValue(const wchar_t* name,int value){
    if(!WritePrivateProfileStringW(L"GameBox",name,std::to_wstring(value).c_str(),settings.c_str()))
        throw std::runtime_error("无法保存游戏资料库设置");
}
void applyThreshold(HWND h){
    BOOL valid=FALSE;unsigned value=GetDlgItemInt(h,84,&valid,FALSE);
    if(!valid||value>999)throw std::runtime_error("请输入 0–999 GiB。0 表示关闭空间提醒。");
    settingsValue(L"ThresholdGiB",value);diskThreshold=int(value);diskChecked=0;lowSpace.clear();checkDisk();
    SetDlgItemTextW(h,88,L"提醒阈值已保存");
}
void reveal(const std::wstring& path,bool select){
    std::error_code ec;
    if(!std::filesystem::exists(path,ec))throw std::runtime_error("路径已移动或不可访问，请检查素材位置");
    auto args=select?L"/select,\""+path+L"\"":L"\""+path+L"\"";
    if(INT_PTR(ShellExecuteW(window,L"open",L"explorer.exe",args.c_str(),nullptr,SW_SHOWNORMAL))<=32)
        throw std::runtime_error("无法打开文件位置");
}
INT_PTR CALLBACK correction(HWND h,UINT m,WPARAM w,LPARAM l){
    if(m==WM_INITDIALOG){
        auto v=reinterpret_cast<Visit*>(l);SetWindowLongPtrW(h,DWLP_USER,LPARAM(v->id));
        fillGames(h,80,v->game,false);
        SetDlgItemInt(h,84,unsigned(std::clamp<int64_t>((v->end>0?v->end:now())-v->start,0,UINT_MAX)),FALSE);
        return TRUE;
    }
    if(m!=WM_COMMAND)return FALSE;
    if(LOWORD(w)==IDCANCEL){EndDialog(h,0);return TRUE;}
    if(LOWORD(w)!=IDOK)return FALSE;
    BOOL valid=FALSE;auto seconds=GetDlgItemInt(h,84,&valid,FALSE);
    auto id=uint64_t(GetWindowLongPtrW(h,DWLP_USER));
    auto v=std::find_if(visits.begin(),visits.end(),[&](const Visit& visit){return visit.id==id;});
    int game=gameSelection(h,80);
    if(!valid||game<=0||v==visits.end()){
        MessageBoxW(h,L"请选择游戏并填写游玩秒数。",L"修正会话",MB_ICONWARNING);return TRUE;
    }
    auto previous=*v;v->game=game;v->end=v->start+seconds;
    for(auto& [path,a]:associations)if(a.context.visit==id)a.context.game=game;
    try{persistActivity();EndDialog(h,1);}
    catch(const std::exception& e){
        *v=previous;for(auto& [path,a]:associations)if(a.context.visit==id)a.context.game=previous.game;
        MessageBoxW(h,wide(e.what()).c_str(),L"修正会话",MB_ICONWARNING);
    }
    return TRUE;
}
const MediaAsset* selectedAsset(HWND h){
    auto id=uint64_t(selectedRow(GetDlgItem(h,81)));
    if(mediaLibrary)for(const auto& a:mediaLibrary->assets())if(a.id==id)return &a;
    return nullptr;
}
INT_PTR CALLBACK activityDialog(HWND h,UINT m,WPARAM w,LPARAM l){
    try{
        if(m==WM_INITDIALOG){
            auto state=reinterpret_cast<ActivityView*>(l);SetWindowLongPtrW(h,DWLP_USER,l);
            COLORREF blue=RGB(0,0,128);DwmSetWindowAttribute(h,35,&blue,sizeof blue);
            SendMessageW(h,WM_SETICON,ICON_SMALL,(LPARAM)retroIcon);
            fillGames(h,80,state->game,true);
            auto list=GetDlgItem(h,81);
            if(state->mode){
                columns(list,{{L"游戏",130},{L"类型",80},{L"大小 / 状态",145},{L"文件路径",560}});
                fillGames(h,98,0,false);SetDlgItemInt(h,84,diskThreshold,FALSE);
            }else{
                columns(list,{{L"游戏",125},{L"开始",160},{L"结束",160},{L"游玩时长",100},{L"回放 / 截图 / 导出",150}});
                columns(GetDlgItem(h,82),{{L"类型",100},{L"会话素材路径",680}});
            }
            renderActivity(h,*state);return TRUE;
        }
        auto state=reinterpret_cast<ActivityView*>(GetWindowLongPtrW(h,DWLP_USER));
        if(!state)return FALSE;
        if(m==WM_NOTIFY&&l&&reinterpret_cast<NMHDR*>(l)->idFrom==81&&reinterpret_cast<NMHDR*>(l)->code==LVN_ITEMCHANGED){
            if(!state->mode)visitAssets(h);
            else if(auto asset=selectedAsset(h)){
                auto folder=std::filesystem::path(wide(asset->path)).parent_path();
                ULARGE_INTEGER free{};
                auto location=folder.wstring()+L" · "+(GetDiskFreeSpaceExW(storageDirectory(folder).c_str(),&free,nullptr,nullptr)
                    ?L"可用 "+sizeText(free.QuadPart):L"无法读取可用空间");
                SetDlgItemTextW(h,87,location.c_str());
            }
            return TRUE;
        }
        if(m==WM_CLOSE){EndDialog(h,0);return TRUE;}
        if(m!=WM_COMMAND)return FALSE;
        int id=LOWORD(w);
        if(id==IDCANCEL){EndDialog(h,0);return TRUE;}
        if(id==80&&HIWORD(w)!=CBN_SELCHANGE)return FALSE;
        if(id==80||id==85){renderActivity(h,*state);return TRUE;}
        if(!state->mode){
            if(id==90){if(auto v=selectedVisit(h)){if(v->id==currentVisit)throw std::runtime_error("请先结束进行中的会话");DialogBoxParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(7104),h,correction,(LPARAM)v);renderActivity(h,*state);}return TRUE;}
            if(id==91){manualGame=0;SendMessageW(window,WM_TIMER,85,0);renderActivity(h,*state);return TRUE;}
            return FALSE;
        }
        if(id==92){applyThreshold(h);return TRUE;}
        if(id==93){reveal(config.folder,false);return TRUE;}
        if(auto a=selectedAsset(h)){
            auto path=wide(a->path);
            if(id==94)reveal(path,true);
            if(id==95||id==96)mediaLibrary->removeAsset(a->id,id==96,h);
            if(id==97){
                int game=gameSelection(h,98);
                if(game<0)throw std::runtime_error("请先选择归档目标");
                auto association=classify(path);
                if(game!=association.context.game){association.context={game,0};associate(path,association);selectGameMedia(-1);}
            }
            renderActivity(h,*state);
        }
        return TRUE;
    }catch(const std::exception& e){MessageBoxW(h,wide(e.what()).c_str(),L"游戏资料库",MB_ICONWARNING);return TRUE;}
}
void openActivity(int mode,int game,HWND owner){
    ActivityView state{mode,game};
    const int resource=mode?7105:7102;
    auto result=DialogBoxParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(resource),owner?owner:window,activityDialog,(LPARAM)&state);
    if(result==-1)throw std::runtime_error("无法打开游戏活动窗口");
}
