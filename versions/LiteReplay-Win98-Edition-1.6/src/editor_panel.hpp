#pragma once
#include <windowsx.h>
#include "editor/process.hpp"
#include "ui_theme.hpp"
#include "editor/history.hpp"
#include "editor/media_probe.hpp"
#include "editor/preview_range.hpp"
#include <functional>
#include <wrl/client.h>
#include <mfplay.h>
#include <mferror.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <thread>
#include <mutex>
#include <deque>
#include <map>
namespace lr_editor {
using Microsoft::WRL::ComPtr;
using namespace lc;
HWND mainWindow,preview,timeline;HFONT font;std::map<int,HWND> ui;
constexpr int ClipsDock=96,ExportDock=104;
bool showingExport=false;
std::filesystem::path toolsDir,iniPath,projectPath,logPath,autosavePath;History history;HMODULE mfModule=nullptr;std::function<bool()> recording;std::function<void(const std::wstring&)> exportRequested;std::function<void(const std::wstring&)> saved;std::atomic<int> exportThreads{2};bool copyExport=false;double timelineZoom=1;int timelineOffset=0;Project project;bool english=false,dirty=false,busy=false,closing=false;int current=-1,playingClip=-1;uint64_t generation=0;double previewTime=0;std::atomic<bool> cancel{false};std::atomic<int> progress{0};std::thread worker;
ComPtr<IMFPMediaPlayer> player;
PreviewRange activeRange;bool previewReady=false,playWhenReady=true,sequenceMode=false,scrubbing=false;double requestedTime=0;bool choosing=false;std::wstring exportDestination;ULONGLONG exportStarted=0;
std::vector<std::pair<int,int>> outputSizes;std::vector<double> outputRates;
void stopPreview();
struct MediaEvent {MFP_EVENT_TYPE type;HRESULT hr;ComPtr<IMFPMediaItem> item;uint64_t token=0;};
std::mutex eventMutex;std::deque<MediaEvent> events;
struct WorkerResult {int kind=0;std::vector<Clip> clips;std::string error;std::wstring path;bool canceled=false;};
std::mutex resultMutex;bool resultReady=false;WorkerResult workResult;
const wchar_t* tr(const wchar_t* zh,const wchar_t* en){return english?en:zh;}
void status(const std::wstring& s){SetWindowTextW(ui[80],s.c_str());}
HWND dialogOwner(){return GetAncestor(mainWindow,GA_ROOT);}
void error(const std::exception& e){try{writeFile(autosavePath.parent_path()/L"editor-operation.log",e.what());}catch(...){}status(tr(L"操作失败，请查看错误提示。",L"Operation failed; see error details."));MessageBoxW(dialogOwner(),wide(e.what()).c_str(),tr(L"操作失败",L"Operation failed"),MB_ICONERROR);}
void hrcheck(HRESULT h,const char* operation="Playback"){if(FAILED(h)){std::ostringstream s;s<<"Windows media error 0x"<<std::hex<<unsigned(h)<<" — "<<operation<<std::dec<<"\nPreview range (100ns): "<<activeRange.start<<" .. "<<activeRange.end;try{writeFile(autosavePath.parent_path()/L"editor-preview.log",s.str());}catch(...){}stopPreview();throw std::runtime_error(s.str());}}
std::wstring value(int id){int n=GetWindowTextLengthW(ui[id]);std::wstring s(size_t(n)+1,L'\0');GetWindowTextW(ui[id],s.data(),n+1);s.resize(n);return s;}
std::wstring timeText(double n){std::wostringstream s;s.imbue(std::locale::classic());s<<std::fixed<<std::setprecision(3)<<n;return s.str();}
std::wstring clockText(double n){int secs=int(n);std::wostringstream s;s<<secs/60<<L":"<<std::setw(2)<<std::setfill(L'0')<<secs%60<<L"."<<std::setw(3)<<int((n-secs)*1000);return s.str();}
class Callback:public IMFPMediaPlayerCallback {
    std::atomic<ULONG> refs{1};uint64_t token_;
public:
    explicit Callback(uint64_t token):token_(token){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** p)override{if(!p)return E_POINTER;if(iid==__uuidof(IUnknown)||iid==__uuidof(IMFPMediaPlayerCallback)){*p=static_cast<IMFPMediaPlayerCallback*>(this);AddRef();return S_OK;}*p=nullptr;return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}ULONG STDMETHODCALLTYPE Release()override{auto n=--refs;if(!n)delete this;return n;}
    void STDMETHODCALLTYPE OnMediaPlayerEvent(MFP_EVENT_HEADER* e)override{MediaEvent m{e->eEventType,e->hrEvent,{},token_};if(e->eEventType==MFP_EVENT_TYPE_MEDIAITEM_CREATED)m.item=reinterpret_cast<MFP_MEDIAITEM_CREATED_EVENT*>(e)->pMediaItem;if(e->eEventType==MFP_EVENT_TYPE_MEDIAITEM_SET)m.item=reinterpret_cast<MFP_MEDIAITEM_SET_EVENT*>(e)->pMediaItem;std::lock_guard<std::mutex> lock(eventMutex);if(events.size()<64)events.push_back(std::move(m));}
};
ComPtr<Callback> callback;
void stopPreview(){sequenceMode=false;scrubbing=false;previewReady=false;++generation;playingClip=-1;if(player){player->Shutdown();player.Reset();}callback.Reset();{std::lock_guard<std::mutex> lock(eventMutex);events.clear();}if(!busy)KillTimer(mainWindow,1);InvalidateRect(preview,nullptr,TRUE);}
void rebuild();void selectClip(int index);void setLanguage();void layout();void openPreview(bool play);void seekPreview();void finishPreview();
void controlsEnabled(){for(int id:{90,91,92,93,94,95,10,11,12,13,21,22,23,24,25,30,31,32,33,34,35,36,37,40,41,42,43,44,45,100,101,102,103,108})EnableWindow(ui[id],!busy);EnableWindow(ui[46],busy);}
void autosave();
void markDirty(){history.record(project);SetTimer(mainWindow,2,800,nullptr);dirty=true;std::wstring title=L"LiteReplay Editor — Yuzi_Lj";if(!projectPath.empty())title+=L" — "+projectPath.filename().wstring();title+=L" *";SetWindowTextW(mainWindow,title.c_str());}
bool sourceReady(){try{sourceLimit(project);return true;}catch(...){return false;}}
void updateSettingsControls(){
 outputSizes.clear();outputRates.clear();SendMessageW(ui[40],CB_RESETCONTENT,0,0);SendMessageW(ui[41],CB_RESETCONTENT,0,0);
 if(!sourceReady()){SetWindowTextW(ui[109],tr(L"需读取素材规格后才能重编码导出。",L"Read source specifications before re-encoding."));for(int id:{40,41,43,44,103})EnableWindow(ui[id],FALSE);return;}
 auto cap=sourceLimit(project);project.settings=clampExport(project,project.settings);auto addSize=[&](int width,int height){std::pair<int,int> pair{width,height};if(width<2||height<2||width>cap.width||height>cap.height)return;if(std::find(outputSizes.begin(),outputSizes.end(),pair)==outputSizes.end())outputSizes.push_back(pair);};
 addSize(cap.width,cap.height);for(double ratio:{.75,.5,.25})addSize(int(cap.width*ratio)/2*2,int(cap.height*ratio)/2*2);
 for(int height:{1440,1080,720,480,360})if(height<cap.height)addSize(int(double(cap.width)*height/cap.height)/2*2,height);
 addSize(project.settings.width,project.settings.height);int selected=0;for(size_t i=0;i<outputSizes.size();++i){auto [w,h]=outputSizes[i];auto label=std::to_wstring(w)+L" × "+std::to_wstring(h)+(i==0?tr(L" · 上限",L" · Max"):L"");SendMessageW(ui[40],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));if(w==project.settings.width&&h==project.settings.height)selected=int(i);}SendMessageW(ui[40],CB_SETCURSEL,selected,0);
 outputRates.push_back(cap.fps);for(double f:{120.,90.,75.,60.,50.,30.,25.,24.,20.,15.})if(f<cap.fps-1e-6)outputRates.push_back(f);if(std::none_of(outputRates.begin(),outputRates.end(),[&](double f){return std::abs(f-project.settings.fps)<1e-7;}))outputRates.push_back(project.settings.fps);
 selected=0;for(size_t i=0;i<outputRates.size();++i){auto label=timeText(outputRates[i])+L" FPS";SendMessageW(ui[41],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));if(std::abs(outputRates[i]-project.settings.fps)<1e-7)selected=int(i);}SendMessageW(ui[41],CB_SETCURSEL,selected,0);
 SendMessageW(ui[42],CB_SETCURSEL,project.settings.encoder,0);SetWindowTextW(ui[43],wide(decimal(project.settings.mbps)).c_str());
 auto label=tr(L"上限：",L"Max: ")+std::to_wstring(cap.width)+L"×"+std::to_wstring(cap.height)+L" / "+timeText(cap.fps)+L" FPS / "+wide(decimal(cap.mbps))+L" Mbps";
 if(std::any_of(project.clips.begin(),project.clips.end(),[](const Clip& c){return c.estimatedBitrate;}))label+=tr(L"（含估算码率）",L" (includes estimated bitrate)");SetWindowTextW(ui[109],label.c_str());for(int id:{40,41,43,44,103})EnableWindow(ui[id],!busy);
}
void readSettings(){if(project.clips.empty())return;auto before=serialize(project);int size=int(SendMessageW(ui[40],CB_GETCURSEL,0,0)),rate=int(SendMessageW(ui[41],CB_GETCURSEL,0,0));if(size<0||size>=int(outputSizes.size())||rate<0||rate>=int(outputRates.size()))throw std::runtime_error("Refresh source specifications / 请重新读取规格");Export e=project.settings;e.width=outputSizes[size].first;e.height=outputSizes[size].second;e.fps=outputRates[rate];e.encoder=int(SendMessageW(ui[42],CB_GETCURSEL,0,0));auto input=utf8(value(43));e.mbps=mediaNumber(input);enforceLimit(project,e);project.settings=e;if(serialize(project)!=before)markDirty();}

void selectClip(int index){stopPreview();current=index;if(current<0||size_t(current)>=project.clips.size()){current=-1;SetWindowTextW(ui[30],L"0");SetWindowTextW(ui[31],L"0");SetWindowTextW(ui[33],L"100");SetWindowTextW(ui[60],tr(L"导入视频以开始剪辑",L"Import videos to start editing"));return;}SendMessageW(ui[20],LB_SETCURSEL,current,0);const auto& c=project.clips[size_t(current)];SetWindowTextW(ui[30],timeText(c.in).c_str());SetWindowTextW(ui[31],timeText(c.out).c_str());SetWindowTextW(ui[33],std::to_wstring(c.volume).c_str());SetWindowTextW(ui[60],wide(c.path).c_str());previewTime=c.in;SetWindowTextW(ui[62],clockText(previewTime).c_str());SendMessageW(ui[50],TBM_SETPOS,TRUE,0);InvalidateRect(timeline,nullptr,TRUE);}
void rebuild(){updateSettingsControls();SendMessageW(ui[20],LB_RESETCONTENT,0,0);for(size_t i=0;i<project.clips.size();++i){auto& c=project.clips[i];std::wstring s=std::to_wstring(i+1)+L"  "+std::filesystem::path(wide(c.path)).filename().wstring()+L"  ["+clockText(c.out-c.in)+L"]";SendMessageW(ui[20],LB_ADDSTRING,0,reinterpret_cast<LPARAM>(s.c_str()));}if(current>=int(project.clips.size()))current=int(project.clips.size())-1;if(current>=0)SendMessageW(ui[20],LB_SETCURSEL,current,0);SetWindowTextW(ui[61],(std::to_wstring(project.clips.size())+tr(L" 个片段 · 总长 ",L" clips · Total ")+clockText(total(project))).c_str());InvalidateRect(timeline,nullptr,TRUE);}
void applyTrim(){if(current<0)throw std::runtime_error(utf8(tr(L"请先选择片段",L"Select a clip first")));Clip next=project.clips[size_t(current)];auto number=[&](int id){auto s=value(id);std::wistringstream input(s);input.imbue(std::locale::classic());double n;if(!(input>>n))throw std::runtime_error("Invalid time / volume");input>>std::ws;if(!input.eof())throw std::runtime_error("Use seconds with a decimal point");return n;};next.in=number(30);next.out=number(31);double vol=number(33);if(std::floor(vol)!=vol)throw std::runtime_error("Volume must be an integer");next.volume=int(vol);validate(next);const auto& old=project.clips[size_t(current)];if(old.in==next.in&&old.out==next.out&&old.volume==next.volume)return;project.clips[size_t(current)]=next;stopPreview();previewTime=std::clamp(previewTime,next.in,next.out);rebuild();markDirty();status(tr(L"裁剪已应用；不会修改原视频。",L"Trim applied. Source files are unchanged."));}
std::vector<std::filesystem::path> choose(bool save,const wchar_t* filter,const wchar_t* ext,bool multi=false){
 struct Guard{Guard(){choosing=true;}~Guard(){choosing=false;}} guard;
 if(save){
  ComPtr<IFileSaveDialog> picker;auto checkDialog=[](HRESULT hr,const char* step){if(FAILED(hr)){std::ostringstream msg;msg<<step<<" failed: 0x"<<std::hex<<unsigned(hr);throw std::runtime_error(msg.str());}};
  checkDialog(CoCreateInstance(CLSID_FileSaveDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&picker)),"Create save dialog");
  FILEOPENDIALOGOPTIONS opts;checkDialog(picker->GetOptions(&opts),"Get dialog options");checkDialog(picker->SetOptions(opts|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST|FOS_NOCHANGEDIR|FOS_OVERWRITEPROMPT),"Set dialog options");
  COMDLG_FILTERSPEC type{filter,filter+wcslen(filter)+1};checkDialog(picker->SetFileTypes(1,&type),"Set file type");checkDialog(picker->SetDefaultExtension(ext),"Set extension");
  SYSTEMTIME now;GetLocalTime(&now);wchar_t name[128];swprintf_s(name,L"LiteReplay-%04d%02d%02d-%02d%02d%02d.%ls",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,ext);picker->SetFileName(name);
  status(tr(L"请选择文件名与保存位置…",L"Choose filename and destination…"));
  HRESULT hr=picker->Show(dialogOwner());if(hr==HRESULT_FROM_WIN32(ERROR_CANCELLED)){status(tr(L"已取消保存。",L"Save canceled."));return {};}
  checkDialog(hr,"Show save dialog");ComPtr<IShellItem> item;checkDialog(picker->GetResult(&item),"Read selected destination");PWSTR path=nullptr;checkDialog(item->GetDisplayName(SIGDN_FILESYSPATH,&path),"Read destination path");std::filesystem::path chosen(path);CoTaskMemFree(path);return {chosen};
 }
 std::vector<wchar_t> buffer(65536);OPENFILENAMEW of{sizeof of};of.hwndOwner=dialogOwner();of.lpstrFilter=filter;of.lpstrFile=buffer.data();of.nMaxFile=DWORD(buffer.size());of.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST|(multi?OFN_ALLOWMULTISELECT:0);
 if(!GetOpenFileNameW(&of)){DWORD e=CommDlgExtendedError();if(e)throw std::runtime_error("Open dialog failed: "+std::to_string(e));return {};}
 std::vector<std::filesystem::path> paths;std::filesystem::path first=buffer.data();wchar_t* p=buffer.data()+wcslen(buffer.data())+1;if(multi&&*p){while(*p){paths.push_back(first/p);p+=wcslen(p)+1;}}else paths.push_back(first);return paths;
}

void finishWork(WorkerResult r){std::lock_guard<std::mutex> lock(resultMutex);workResult=std::move(r);resultReady=true;}
void startWork(){if(worker.joinable())worker.join();cancel=false;progress=0;busy=true;controlsEnabled();stopPreview();SetTimer(mainWindow,1,100,nullptr);}
void importVideos(std::vector<std::filesystem::path> paths={},bool refreshSpecs=false){if(busy)throw std::runtime_error("Editor is busy");if(paths.empty())paths=choose(false,L"Video / 视频\0*.mp4;*.mkv;*.mov;*.avi;*.webm;*.ts\0All / 所有文件\0*.*\0\0",nullptr,true);if(paths.empty())return;if(!refreshSpecs&&project.clips.size()+paths.size()>256)throw std::runtime_error("Maximum 256 clips per project");exportDestination.clear();startWork();status(tr(L"正在读取媒体信息…",L"Reading media information…"));worker=std::thread([paths,refreshSpecs]{WorkerResult r;r.kind=refreshSpecs?3:1;try{for(size_t i=0;i<paths.size();++i){if(cancel)break;auto result=run(toolsDir/L"ffprobe.exe",{"-v","error","-show_entries","format=duration,bit_rate:format_tags=comment:stream=codec_type,width,height,avg_frame_rate,r_frame_rate,bit_rate:stream_side_data=rotation","-of","default",utf8(paths[i].wstring())},cancel,{},30000);if(result.code){if(cancel)break;r.error+=utf8(paths[i].filename().wstring())+": "+result.output+"\n";continue;}Clip c;try{c=parseMediaProbe(utf8(std::filesystem::absolute(paths[i]).wstring()),result.output);}catch(const std::exception& e){r.error+=utf8(paths[i].wstring())+": "+e.what()+"\n";continue;}r.clips.push_back(c);progress=int(100*(i+1)/paths.size());}}catch(const std::exception& e){r.error+=e.what();}r.canceled=cancel;finishWork(std::move(r));});}
void refreshSpecifications(){std::vector<std::filesystem::path> paths;for(const auto& c:project.clips)paths.emplace_back(wide(c.path));if(!paths.empty())importVideos(paths,true);}
void saveProject(){readSettings();if(current>=0)applyTrim();auto paths=choose(true,L"LiteCut project / 工程\0*.lcut\0\0",L"lcut");if(paths.empty())return;for(const auto& c:project.clips){std::error_code ec;if(std::filesystem::equivalent(paths[0],std::filesystem::path(wide(c.path)),ec))throw std::runtime_error("Cannot overwrite source media with a project");}auto tmp=paths[0];tmp+=L".tmp-"+std::to_wstring(GetCurrentProcessId());writeFile(tmp,serialize(project));if(!MoveFileExW(tmp.c_str(),paths[0].c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){DeleteFileW(tmp.c_str());throw std::runtime_error("Cannot save project");}projectPath=paths[0];dirty=false;SetWindowTextW(mainWindow,(L"LiteReplay Editor — "+projectPath.filename().wstring()).c_str());status(tr(L"工程已保存；源视频仍需保留在原位置。",L"Project saved. Keep source videos at their original paths."));}
bool discard(){return !dirty||MessageBoxW(mainWindow,tr(L"未保存的工程修改将丢失。继续吗？",L"Unsaved project changes will be lost. Continue?"),L"LiteReplay Editor",MB_YESNO|MB_ICONQUESTION)==IDYES;}
void openProject(){if(!discard())return;auto paths=choose(false,L"LiteCut project / 工程\0*.lcut\0\0",L"lcut");if(paths.empty())return;auto next=deserialize(readFile(paths[0]));for(auto& c:next.clips)if(!std::filesystem::is_regular_file(wide(c.path)))throw std::runtime_error("Source file missing: "+c.path);stopPreview();project=std::move(next);history.reset(project);SetTimer(mainWindow,2,800,nullptr);projectPath=paths[0];dirty=false;current=project.clips.empty()?-1:0;rebuild();selectClip(current);updateSettingsControls();SetWindowTextW(mainWindow,(L"LiteReplay Editor — "+projectPath.filename().wstring()).c_str());status(tr(L"工程已打开。",L"Project opened."));if(!sourceReady())refreshSpecifications();}
void openPreview(bool play){
 if(recording&&recording())throw std::runtime_error("Stop replay before editor preview / 请先停止回放再预览剪辑");if(current<0)return;
 bool sequence=sequenceMode;double target=previewTime;if(play&&target>=project.clips[size_t(current)].out-.0001)target=project.clips[size_t(current)].in;stopPreview();sequenceMode=sequence;playWhenReady=play;requestedTime=target;
 if(!mfModule)mfModule=LoadLibraryW(L"mfplay.dll");if(!mfModule)throw std::runtime_error("Media Foundation unavailable");
 using Create=HRESULT(WINAPI*)(LPCWSTR,BOOL,MFP_CREATION_OPTIONS,IMFPMediaPlayerCallback*,HWND,IMFPMediaPlayer**);
 auto create=reinterpret_cast<Create>(GetProcAddress(mfModule,"MFPCreateMediaPlayer"));if(!create)throw std::runtime_error("MFPlay unavailable");callback.Attach(new Callback(generation));hrcheck(create(nullptr,FALSE,0,callback.Get(),preview,&player));
 playingClip=current;hrcheck(player->CreateMediaItemFromURL(wide(project.clips[size_t(current)].path).c_str(),FALSE,DWORD_PTR(generation),nullptr));SetTimer(mainWindow,1,40,nullptr);status(tr(L"正在打开预览…",L"Opening preview…"));
}
void playClip(){
 if(recording&&recording())throw std::runtime_error("Stop replay before editor preview / 请先停止回放再预览剪辑");if(current<0)return;
 if(player&&playingClip==current){if(!previewReady){playWhenReady=!playWhenReady;return;}MFP_MEDIAPLAYER_STATE state;hrcheck(player->GetState(&state));if(state==MFP_MEDIAPLAYER_STATE_PLAYING){hrcheck(player->Pause());playWhenReady=false;KillTimer(mainWindow,1);}else{playWhenReady=true;hrcheck(player->Play());SetTimer(mainWindow,1,40,nullptr);}return;}
 applyTrim();openPreview(true);
}
void seekPreview(){
 sequenceMode=false;
 if(!player||playingClip!=current){openPreview(false);return;}
 if(!previewReady){requestedTime=previewTime;playWhenReady=false;return;}
 hrcheck(player->Pause(),"Seek pause");playWhenReady=false;
 PROPVARIANT pos{};pos.vt=VT_I8;pos.hVal.QuadPart=activeRange.seek(previewTime);
 hrcheck(player->SetPosition(MFP_POSITIONTYPE_100NS,&pos),"Seek position");SetTimer(mainWindow,1,40,nullptr);
}
void finishPreview(){bool next=sequenceMode&&size_t(current+1)<project.clips.size();if(next){selectClip(current+1);sequenceMode=true;openPreview(true);}else{stopPreview();status(tr(L"预览结束。",L"Preview finished."));}}

void exportVideo(){if(recording&&recording())throw std::runtime_error("Stop replay before exporting / 请先停止回放再导出，避免争抢编码器");if(project.clips.empty())throw std::runtime_error(utf8(tr(L"请先添加片段",L"Add clips first")));readSettings();if(current>=0)applyTrim();auto paths=choose(true,L"MP4 video / 视频\0*.mp4\0\0",L"mp4");if(paths.empty())return;if(std::filesystem::exists(paths[0]))throw std::runtime_error(utf8(tr(L"请选择新文件名；导出不覆盖现有文件。",L"Choose a new filename. Export does not overwrite existing files.")));Project snapshot=project;auto output=std::filesystem::absolute(paths[0]);exportDestination=output.wstring();exportStarted=GetTickCount64();if(exportRequested)exportRequested(exportDestination);startWork();status(tr(L"正在启动编码器…",L"Starting encoder…"));UpdateWindow(mainWindow);worker=std::thread([snapshot,output]{WorkerResult r;r.kind=2;r.path=output.wstring();bool committed=false;std::filesystem::path temp;std::ostringstream logs;try{temp=output.parent_path()/(L".LiteReplay-edit-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));if(!std::filesystem::create_directory(temp))throw std::runtime_error("Cannot create export working directory");std::ostringstream concat;for(size_t i=0;i<snapshot.clips.size();++i){if(cancel)break;std::string name="segment_"+std::to_string(i)+".mp4";auto args=segmentArgs(snapshot.clips[i],snapshot.settings,utf8((temp/wide(name)).wstring()));for(size_t k=0;k+1<args.size();++k)if(args[k]=="-threads")args[k+1]=std::to_string(exportThreads.load());args.insert(args.begin(),{"-progress","pipe:1","-nostats"});double before=0;for(size_t k=0;k<i;++k)before+=snapshot.clips[k].out-snapshot.clips[k].in;
 auto result=run(toolsDir/L"ffmpeg.exe",args,cancel,{},0,[&](double seconds){progress=int(90*std::clamp((before+std::min(seconds,snapshot.clips[i].out-snapshot.clips[i].in))/total(snapshot),0.0,1.0));});logs<<"Segment "<<i+1<<"\n"<<result.output<<'\n';if(result.code){if(cancel)break;throw std::runtime_error(result.output);}concat<<"file '"<<name<<"'\n";progress=int(90*(i+1)/snapshot.clips.size());}if(!cancel){writeFile(temp/L"list.txt",concat.str());auto result=run(toolsDir/L"ffmpeg.exe",{"-hide_banner","-nostdin","-n","-f","concat","-safe","1","-i",utf8((temp/L"list.txt").wstring()),"-map","0:v:0","-map","0:a:0","-c","copy","-movflags","+faststart",utf8((temp/L"output.mp4").wstring())},cancel,temp);logs<<result.output;if(result.code&&!cancel)throw std::runtime_error(result.output);if(!cancel){auto verify=run(toolsDir/L"ffprobe.exe",{"-v","error","-show_entries","stream=codec_type","-of","default=noprint_wrappers=1",utf8((temp/L"output.mp4").wstring())},cancel);if(cancel){r.canceled=true;}else if(verify.code||verify.output.find("codec_type=video")==std::string::npos||verify.output.find("codec_type=audio")==std::string::npos)throw std::runtime_error("Export validation failed: "+verify.output);if(cancel)throw std::runtime_error("Export canceled");if(!MoveFileExW((temp/L"output.mp4").c_str(),output.c_str(),MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot finalize export; destination may already exist");committed=true;progress=100;}}}catch(const std::exception& e){r.error=e.what();logs<<"ERROR: "<<e.what()<<"\n";}r.canceled=cancel&&!committed;try{writeFile(logPath,logs.str().substr(logs.str().size()>1024*1024?logs.str().size()-1024*1024:0));}catch(...){}if(!temp.empty()){std::error_code ec;std::filesystem::remove_all(temp,ec);}finishWork(std::move(r));});}
void mediaEvents(){std::deque<MediaEvent> pending;{std::lock_guard<std::mutex> lock(eventMutex);pending.swap(events);}for(auto& e:pending){if(e.token!=generation)continue;if(!player||playingClip<0||size_t(playingClip)>=project.clips.size())continue;DWORD_PTR token=0;if(e.item){e.item->GetUserData(&token);if(token!=generation)continue;}if(FAILED(e.hr)){stopPreview();std::wostringstream msg;msg<<tr(L"系统预览不可用（可尝试直接导出）。错误 0x",L"System preview unavailable (export may still work). Error 0x")<<std::hex<<unsigned(e.hr);status(msg.str());continue;}auto& c=project.clips[size_t(playingClip)];if(e.type==MFP_EVENT_TYPE_PLAYBACK_ENDED){finishPreview();continue;}if(e.type==MFP_EVENT_TYPE_MEDIAITEM_CREATED){PROPVARIANT duration{};HRESULT durationHr=e.item->GetDuration(MFP_POSITIONTYPE_100NS,&duration);int64_t ticks=0;if(SUCCEEDED(durationHr)){if(duration.vt==VT_UI8&&duration.uhVal.QuadPart<=uint64_t(INT64_MAX))ticks=int64_t(duration.uhVal.QuadPart);else if(duration.vt==VT_I8)ticks=duration.hVal.QuadPart;}PropVariantClear(&duration);hrcheck(durationHr,"GetDuration");try{activeRange=previewRange(c.in,c.out,ticks);}catch(...){stopPreview();throw;}// Keep the media item untrimmed; some sources reject SetStartStopPosition even at their reported duration.
hrcheck(player->SetMediaItem(e.item.Get()),"SetMediaItem");}if(e.type==MFP_EVENT_TYPE_MEDIAITEM_SET){previewReady=true;auto target=activeRange.seek(requestedTime);previewTime=activeRange.sourceTime(target);hrcheck(player->SetVolume(std::min(c.volume,100)/100.f));PROPVARIANT initial{};initial.vt=VT_I8;initial.hVal.QuadPart=target;hrcheck(player->SetPosition(MFP_POSITIONTYPE_100NS,&initial),"Initial SetPosition");status(tr(L"预览就绪 · 空格播放，方向键微调，I/O设入出点。",L"Ready · Space play, arrows seek, I/O mark In/Out."));}
 if(e.type==MFP_EVENT_TYPE_POSITION_SET&&previewReady){if(playWhenReady)hrcheck(player->Play());else KillTimer(mainWindow,1);InvalidateRect(preview,nullptr,FALSE);}
}}
void tick(){mediaEvents();if(!busy&&!player)KillTimer(mainWindow,1);if(player&&previewReady&&playingClip>=0&&!scrubbing){PROPVARIANT pos{};if(SUCCEEDED(player->GetPosition(MFP_POSITIONTYPE_100NS,&pos))&&pos.vt==VT_I8){previewTime=activeRange.sourceTime(pos.hVal.QuadPart);const auto& c=project.clips[size_t(playingClip)];SendMessageW(ui[50],TBM_SETPOS,TRUE,int(10000*(previewTime-c.in)/(c.out-c.in)));InvalidateRect(timeline,nullptr,FALSE);SetWindowTextW(ui[62],clockText(previewTime).c_str());if(pos.hVal.QuadPart>=activeRange.end){finishPreview();}}PropVariantClear(&pos);}if(busy){SendMessageW(ui[81],PBM_SETPOS,progress,0);if(!exportDestination.empty())status((cancel?tr(L"正在取消 · ",L"Canceling · "):tr(L"正在导出 · ",L"Exporting · "))+std::to_wstring(progress.load())+L"% · "+std::to_wstring((GetTickCount64()-exportStarted)/1000)+tr(L" 秒 · ",L" s · ")+exportDestination);bool ready;WorkerResult r;{std::lock_guard<std::mutex> lock(resultMutex);ready=resultReady;if(ready){r=std::move(workResult);resultReady=false;}}if(ready){if(worker.joinable())worker.join();busy=false;exportDestination.clear();controlsEnabled();updateSettingsControls();if(r.kind==3){for(auto& existing:project.clips)for(const auto& fresh:r.clips)if(existing.path==fresh.path){existing.width=fresh.width;existing.height=fresh.height;existing.fps=fresh.fps;existing.mbps=fresh.mbps;existing.estimatedBitrate=fresh.estimatedBitrate;}if(sourceReady()){auto cap=sourceLimit(project);cap.encoder=project.settings.encoder;project.settings=cap;}rebuild();markDirty();status(tr(L"规格已刷新；导出不会超过素材上限。",L"Specifications refreshed; export is source-limited."));}else if(r.kind==1){bool wasEmpty=project.clips.empty();for(auto& c:r.clips)project.clips.push_back(std::move(c));if(wasEmpty&&sourceReady()){auto cap=sourceLimit(project);cap.encoder=project.settings.encoder;project.settings=cap;}if(!r.clips.empty()){current=int(project.clips.size())-int(r.clips.size());rebuild();selectClip(current);markDirty();}status(r.canceled?tr(L"导入已取消，已读取片段保留。",L"Import canceled; completed clips retained."):tr(L"导入完成。选择片段并点击播放。",L"Import complete. Select a clip and press Play."));}else status(r.canceled?tr(L"导出已取消。",L"Export canceled."):r.error.empty()?std::wstring(tr(L"导出完成：",L"Export complete: "))+r.path:tr(L"导出失败；日志位于 LocalAppData\\LiteReplay\\2.0\\editor-export.log。",L"Export failed; see LocalAppData\\LiteReplay\\2.0\\editor-export.log."));if(r.kind==2&&!r.canceled&&r.error.empty()&&saved)saved(r.path);if(!r.error.empty()&&!r.canceled){auto message=wide(r.error.substr(r.error.size()>4000?r.error.size()-4000:0));MessageBoxW(dialogOwner(),message.c_str(),tr(L"媒体处理详情",L"Media processing details"),MB_ICONWARNING);}}}}
LRESULT CALLBACK previewProc(HWND h,UINT m,WPARAM w,LPARAM l){if(m==WM_PAINT){PAINTSTRUCT ps;BeginPaint(h,&ps);if(player)player->UpdateVideo();else{const auto& palette=lr_ui::colors();RECT r;GetClientRect(h,&r);lr_ui::fill(ps.hdc,r,palette.preview);SetTextColor(ps.hdc,palette.previewInk);SetBkMode(ps.hdc,TRANSPARENT);auto old=SelectObject(ps.hdc,font);DrawTextW(ps.hdc,tr(L"选择片段，然后点击播放",L"Select a clip, then press Play"),-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);SelectObject(ps.hdc,old);}EndPaint(h,&ps);return 0;}if(m==WM_SIZE&&player)player->UpdateVideo();return DefWindowProcW(h,m,w,l);}
LRESULT CALLBACK timelineProc(HWND h,UINT m,WPARAM w,LPARAM l){
 RECT r{};GetClientRect(h,&r);
 constexpr int gutter=84;
 const int viewport=std::max(1,int(r.right)-gutter-8);
 double sum=total(project),span=viewport*timelineZoom;
 if(m==WM_MOUSEWHEEL){
  timelineOffset=std::clamp(timelineOffset-int(short(HIWORD(w)))/120*80,0,std::max(0,int(span)-viewport));
  InvalidateRect(h,nullptr,TRUE);return 0;
 }
 if(m==WM_PAINT){
  PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);
  const auto& palette=lr_ui::colors();
  const int videoBottom=65,audioTop=66,audioBottom=r.bottom-4;
  lr_ui::fill(dc,r,palette.canvas);
  lr_ui::fill(dc,{2,25,gutter-2,videoBottom},palette.surface);
  lr_ui::fill(dc,{2,audioTop,gutter-2,audioBottom},palette.surface);
  lr_ui::bevel(dc,{2,25,gutter-2,videoBottom},false);
  lr_ui::bevel(dc,{2,audioTop,gutter-2,audioBottom},false);
  lr_ui::label(dc,h,{8,25,gutter-7,videoBottom},palette.ink,tr(L"视频",L"Video"),DT_LEFT);
  lr_ui::label(dc,h,{8,audioTop,gutter-7,audioBottom},palette.ink,tr(L"音频",L"Audio"),DT_LEFT);
  int saved=SaveDC(dc);IntersectClipRect(dc,gutter,0,r.right,r.bottom);
  lr_ui::fill(dc,{gutter,2,r.right,24},palette.input);
  lr_ui::fill(dc,{gutter,25,r.right,videoBottom},palette.surface);
  lr_ui::fill(dc,{gutter,audioTop,r.right,audioBottom},palette.input);
  auto old=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,palette.ink);
  if(sum>0){
   for(int px=((timelineOffset+79)/80)*80;px<=timelineOffset+viewport;px+=80){
    const int x=gutter+px-timelineOffset;
    const auto tick=clockText(sum*px/span);
    lr_ui::fill(dc,{x,18,x+1,25},palette.edge);
    RECT mark{x+3,0,x+76,19};DrawTextW(dc,tick.c_str(),-1,&mark,DT_SINGLELINE|DT_END_ELLIPSIS);
   }
   for(int px=(timelineOffset/16)*16;px<=timelineOffset+viewport;px+=16){
    if(px%80==0)continue;
    const int x=gutter+px-timelineOffset;
    lr_ui::fill(dc,{x,21,x+1,25},palette.edge);
   }
   double time=0;int cursor=-1;
   for(size_t i=0;i<project.clips.size();++i){
    auto& clip=project.clips[i];
    int x=gutter+int(span*time/sum)-timelineOffset;
    int end=gutter+int(span*(time+clip.out-clip.in)/sum)-timelineOffset;
    if(end>gutter&&x<r.right&&end>x){
     bool selected=int(i)==current;
     RECT video{x,26,end,videoBottom-1},audio{x,audioTop+1,end,audioBottom};
     lr_ui::frame(dc,video,selected?palette.accent:RGB(207,217,235),palette.edge,0);
     lr_ui::frame(dc,audio,clip.audio&&clip.volume?RGB(149,201,163):RGB(219,219,219),palette.edge,0);
     auto text=std::filesystem::path(wide(clip.path)).filename().wstring();
     InflateRect(&video,-4,0);
     SetTextColor(dc,selected?palette.onAccent:palette.ink);
     DrawTextW(dc,text.c_str(),-1,&video,DT_NOPREFIX|DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
     if(end-x>37){
      InflateRect(&audio,-4,0);SetTextColor(dc,palette.ink);
      auto volume=clip.audio?std::to_wstring(clip.volume)+L"%":std::wstring(L"–");
      DrawTextW(dc,volume.c_str(),-1,&audio,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
     }
     if(selected)cursor=x+int(span*std::clamp(previewTime-clip.in,0.0,clip.out-clip.in)/sum);
    }
    time+=clip.out-clip.in;
   }
   if(cursor>=gutter){
    constexpr COLORREF playhead=RGB(180,29,42);
    lr_ui::fill(dc,{cursor-4,2,cursor+6,7},playhead);
    lr_ui::fill(dc,{cursor,7,cursor+2,r.bottom},playhead);
   }
  }
  SelectObject(dc,old);if(saved)RestoreDC(dc,saved);
  EndPaint(h,&ps);return 0;
 }
 if((m==WM_LBUTTONDOWN||(m==WM_MOUSEMOVE&&(w&MK_LBUTTON))||m==WM_LBUTTONUP)&&!busy&&sum>0){
  if(m==WM_LBUTTONDOWN&&GET_X_LPARAM(l)<gutter)return 0;
  if(m==WM_MOUSEMOVE&&!scrubbing)return 0;
  if(m==WM_LBUTTONDOWN){SetCapture(h);scrubbing=true;}
  if(m==WM_LBUTTONUP){
   if(!scrubbing)return 0;
   ReleaseCapture();scrubbing=false;
  }
  double at=std::clamp((double(GET_X_LPARAM(l))-gutter+timelineOffset)/span*sum,0.0,std::max(0.0,sum-.0001));
  double start=0;
  for(size_t i=0;i<project.clips.size();++i){
   auto clip=project.clips[i];
   if(at<start+clip.out-clip.in){
    if(current!=int(i))selectClip(int(i));
    scrubbing=m!=WM_LBUTTONUP;
    previewTime=clip.in+at-start;
    SendMessageW(ui[50],TBM_SETPOS,TRUE,int(10000*(previewTime-clip.in)/(clip.out-clip.in)));
    SetWindowTextW(ui[62],clockText(previewTime).c_str());
    if(m==WM_LBUTTONUP){try{seekPreview();}catch(const std::exception& e){error(e);}}
    InvalidateRect(h,nullptr,FALSE);break;
   }
   start+=clip.out-clip.in;
  }
  return 0;
 }
 if(m==WM_CAPTURECHANGED)scrubbing=false;
 return DefWindowProcW(h,m,w,l);
}
void layout(){
 RECT r{};GetClientRect(mainWindow,&r);
 const int w=r.right,h=r.bottom;
 const int previewX=16,dockX=w-282,dockWidth=264,previewWidth=dockX-previewX-18;
 const int dockBottom=h-208;
 auto move=[&](int id,int x,int y,int width,int height){MoveWindow(ui[id],x,y,width,height,TRUE);};

 int x=12;
 for(int id:{11,12,13,10}){move(id,x,9,98,26);x+=104;}
 ShowWindow(ui[14],SW_HIDE);
 move(90,x+4,9,64,26);move(91,x+74,9,64,26);
 move(92,x+146,9,146,26);move(93,w-168,9,156,26);

 move(ClipsDock,dockX,55,128,25);move(ExportDock,dockX+132,55,132,25);
 move(20,dockX,88,dockWidth,dockBottom-188);
 move(61,dockX,dockBottom-94,dockWidth,27);
 for(int i=0;i<4;++i)move(21+i,dockX+(i%2)*134,dockBottom-61+(i/2)*28,130,25);

 move(97,previewX,53,108,22);move(60,previewX+113,53,previewWidth-113,22);
 MoveWindow(preview,previewX,79,previewWidth,dockBottom-149,TRUE);
 move(50,previewX,dockBottom-66,previewWidth-88,24);
 move(62,dockX-103,dockBottom-66,84,22);
 int transportX=previewX;
 for(auto [id,width]:{std::pair<int,int>{25,112},{26,52},{100,112},{101,56},{102,56}}){
  move(id,transportX,dockBottom-34,width,25);transportX+=width+4;
 }

 move(105,dockX,90,dockWidth,19);move(40,dockX,110,dockWidth,180);
 move(106,dockX,137,dockWidth,19);move(41,dockX,157,dockWidth,180);
 move(107,dockX,184,dockWidth,19);move(42,dockX,204,dockWidth,180);
 move(73,dockX,236,100,22);move(43,dockX+105,234,159,25);
 move(103,dockX,264,dockWidth,25);move(108,dockX,293,dockWidth,25);
 move(109,dockX,324,dockWidth,37);
 move(44,dockX,365,dockWidth,28);

 for(int id:{20,21,22,23,24,61})ShowWindow(ui[id],showingExport?SW_HIDE:SW_SHOWNA);
 for(int id:{40,41,42,43,44,73,103,105,106,107,108,109})ShowWindow(ui[id],showingExport?SW_SHOWNA:SW_HIDE);
 lr_ui::active(ui[ClipsDock],!showingExport);lr_ui::active(ui[ExportDock],showingExport);

 const int trim=dockBottom+10;
 move(70,16,trim,42,24);move(30,60,trim,62,24);move(34,126,trim,74,24);
 move(71,212,trim,48,24);move(31,262,trim,62,24);move(35,328,trim,78,24);
 move(72,418,trim,70,24);move(33,490,trim,56,24);
 move(32,558,trim,98,24);move(36,662,trim,98,24);move(37,766,trim,98,24);
 move(98,16,h-165,w-144,22);move(94,w-111,h-167,46,24);move(95,w-60,h-167,46,24);
 MoveWindow(timeline,16,h-138,w-32,104,TRUE);
 move(80,16,h-28,w-339,22);move(81,w-318,h-25,123,15);
 move(46,w-190,h-29,104,24);move(45,w-80,h-29,65,24);
 lr_ui::sections[mainWindow]={{8,45,dockX-10,dockBottom},
                            {dockX-8,45,w-8,dockBottom},{8,dockBottom+4,w-8,h-30}};
 InvalidateRect(mainWindow,nullptr,TRUE);
 if(player)player->UpdateVideo();
}

void setLanguage(){auto set=[&](int id,const wchar_t* zh,const wchar_t* en){SetWindowTextW(ui[id],tr(zh,en));};set(ExportDock,L"导出设置",L"Export settings");set(105,L"输出分辨率",L"Resolution");set(106,L"输出帧率",L"Frame rate");set(107,L"视频编码器",L"Video encoder");set(100,L"连续预览",L"Play sequence");set(101,L"−0.1 秒",L"−0.1 s");set(102,L"+0.1 秒",L"+0.1 s");set(103,L"匹配原视频上限",L"Match source limits");set(108,L"重新读取规格",L"Refresh source info");set(ClipsDock,L"工程素材",L"Project clips");set(97,L"剪辑预览",L"Preview");set(98,L"时间轴 · 点击定位 / I 入点 / O 出点 / S 分割",L"TIMELINE · Seek / I In / O Out / S Split");set(90,L"撤销",L"Undo");set(91,L"重做",L"Redo");set(92,L"恢复自动保存",L"Recover autosave");set(93,L"快速裁剪导出",L"Stream-copy trim");set(94,L"放大",L"+");set(95,L"缩小",L"−");set(10,L"导入视频",L"Import video");set(11,L"新建工程",L"New project");set(12,L"打开工程",L"Open project");set(13,L"保存工程",L"Save project");set(21,L"上移",L"Up");set(22,L"下移",L"Down");set(23,L"复制",L"Duplicate");set(24,L"移除",L"Remove");set(25,L"播放 / 暂停",L"Play / Pause");set(26,L"停止",L"Stop");set(32,L"应用裁剪",L"Apply trim");set(34,L"设为入点",L"Set In");set(35,L"设为出点",L"Set Out");set(36,L"此处分割",L"Split here");set(37,L"重置片段",L"Reset clip");set(44,L"导出 MP4",L"Export MP4");set(45,L"使用说明",L"Help");set(46,L"取消任务",L"Cancel job");set(70,L"入点秒",L"In (s)");set(71,L"出点秒",L"Out (s)");set(72,L"音量 %",L"Volume %");set(73,L"码率 M",L"Mbps");SendMessageW(ui[42],CB_RESETCONTENT,0,0);for(auto s:{tr(L"GPU · H.264 NVENC",L"GPU · H.264 NVENC"),tr(L"CPU · MPEG-4 兼容",L"CPU · MPEG-4 fallback"),L"GPU · HEVC NVENC"})SendMessageW(ui[42],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s));SendMessageW(ui[42],CB_SETCURSEL,project.settings.encoder,0);SendMessageW(ui[14],CB_SETCURSEL,english?1:0,0);rebuild();if(current<0)SetWindowTextW(ui[60],tr(L"导入视频以开始剪辑",L"Import videos to start editing"));status(tr(L"就绪 · 先导入，再裁剪、排序和导出。",L"Ready · Import, trim, reorder and export."));}
void buildUI(){font=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Tahoma");auto add=[&](int id,const wchar_t* cls,DWORD style){HWND item=CreateWindowExW(cls==std::wstring(L"EDIT")?WS_EX_CLIENTEDGE:0,cls,L"",WS_CHILD|WS_VISIBLE|style,0,0,100,25,mainWindow,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);if(!item)throw std::runtime_error("Cannot create UI control");SendMessageW(item,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);ui[id]=item;};for(int id:{10,11,12,13,21,22,23,24,25,26,32,34,35,36,37,44,45,46,90,91,92,93,94,95,100,101,102,103,108,ClipsDock,ExportDock})add(id,L"BUTTON",WS_TABSTOP);for(int id:{30,31,33,43})add(id,L"EDIT",ES_AUTOHSCROLL|WS_TABSTOP);for(int id:{60,61,62,70,71,72,73,80,97,98,105,106,107,109})add(id,L"STATIC",SS_LEFT|SS_PATHELLIPSIS);add(20,L"LISTBOX",LBS_NOTIFY|WS_VSCROLL|WS_HSCROLL|WS_BORDER|WS_TABSTOP);for(int id:{14,40,41,42})add(id,WC_COMBOBOXW,CBS_DROPDOWNLIST|WS_TABSTOP);add(50,TRACKBAR_CLASSW,WS_TABSTOP);SendMessageW(ui[50],TBM_SETRANGE,TRUE,MAKELPARAM(0,10000));add(81,PROGRESS_CLASSW,0);
    for(auto s:{L"简体中文",L"English"})SendMessageW(ui[14],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s));for(auto s:{L"1280 × 720",L"1920 × 1080",L"2560 × 1440"})SendMessageW(ui[40],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s));for(auto s:{L"24 FPS",L"30 FPS",L"60 FPS",L"75 FPS"})SendMessageW(ui[41],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s));
    WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpfnWndProc=previewProc;wc.hbrBackground=reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));wc.lpszClassName=L"LiteCutPreview";RegisterClassW(&wc);preview=CreateWindowExW(0,wc.lpszClassName,L"",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,0,0,100,100,mainWindow,nullptr,wc.hInstance,nullptr);wc.lpfnWndProc=timelineProc;wc.hbrBackground=nullptr;wc.lpszClassName=L"LiteCutTimeline";RegisterClassW(&wc);timeline=CreateWindowExW(0,wc.lpszClassName,L"",WS_CHILD|WS_VISIBLE,0,0,100,50,mainWindow,nullptr,wc.hInstance,nullptr);
    lr_ui::apply(mainWindow);lr_ui::primary(ui[10]);lr_ui::primary(ui[44]);setLanguage();updateSettingsControls();selectClip(-1);layout();controlsEnabled();}
void streamCopy();void recover();void undoRedo(bool redo);
void action(int id){if(choosing)return;
    if(id==ClipsDock||id==ExportDock){showingExport=id==ExportDock;layout();return;}
    if(busy&&id!=26&&id!=45&&id!=46&&id!=14)return;
    if(id==108){refreshSpecifications();return;}
    if(id==103){auto cap=sourceLimit(project);cap.encoder=int(SendMessageW(ui[42],CB_GETCURSEL,0,0));project.settings=cap;updateSettingsControls();markDirty();status(tr(L"已匹配素材上限；所有参数仅可向下调整。",L"Matched source limits; only lower values are allowed."));return;}

    if(id==100&&current>=0){applyTrim();sequenceMode=true;openPreview(true);return;}
    if((id==101||id==102)&&current>=0){auto& c=project.clips[size_t(current)];previewTime=std::clamp(previewTime+(id==101?-.1:.1),c.in,std::max(c.in,c.out-.0001));SetWindowTextW(ui[62],clockText(previewTime).c_str());SendMessageW(ui[50],TBM_SETPOS,TRUE,int(10000*(previewTime-c.in)/(c.out-c.in)));seekPreview();InvalidateRect(timeline,nullptr,FALSE);return;}
    if(id==90){undoRedo(false);return;}if(id==91){undoRedo(true);return;}if(id==92){recover();return;}if(id==93){streamCopy();return;}if(id==94||id==95){timelineZoom=std::clamp(timelineZoom*(id==94?1.5:1/1.5),1.0,16.0);timelineOffset=0;InvalidateRect(timeline,nullptr,TRUE);return;}
    if(id==10)importVideos();if(id==11&&discard()){stopPreview();project={};history.reset(project);SetTimer(mainWindow,2,800,nullptr);current=-1;projectPath.clear();dirty=false;rebuild();selectClip(-1);updateSettingsControls();SetWindowTextW(mainWindow,L"LiteReplay Editor — Yuzi_Lj");}if(id==12)openProject();if(id==13)saveProject();if(id==25)playClip();if(id==26)stopPreview();if(id==32)applyTrim();if(id==34)SetWindowTextW(ui[30],timeText(previewTime).c_str());if(id==35)SetWindowTextW(ui[31],timeText(previewTime).c_str());if(id==44)exportVideo();if(id==46){cancel=true;status(tr(L"正在取消…",L"Canceling…"));}
    if(id==45)MessageBoxW(mainWindow,tr(L"软件由 Yuzi_Lj 制作\n导入视频，选择片段，定位后设入点/出点，再应用裁剪。分割按当前播放头位置执行；上下移改变拼接顺序。\n撤销/重做最多64个快照；修改后自动保存，恢复按钮可读取上次快照。工程不包含原视频。\n快速裁剪仅导出选中片段、不重编码，边界受关键帧影响；音量必须100%。\n常规导出会重编码；NVENC需驱动，CPU兼容为MPEG-4而非H.264。导出分辨率/帧率/目标码率不能超过素材上限。\n回放运行时禁止预览/导出；开启回放会取消剪辑任务。\n尚无BGM、文字、变速、代理预览或多轨合成。",L"Software by Yuzi_Lj\nImport, select, seek, set In/Out, then Apply trim. Split uses the playhead. Up/Down changes sequence.\nUndo/redo: up to 64 snapshots. Applied edits autosave. Recover restores the latest autosave. Projects reference source files.\nStream-copy exports only the selected clip, with keyframe-dependent cut boundaries and unchanged volume.\nNormal export re-encodes. CPU fallback is MPEG-4, not H.264. Resolution/FPS/target bitrate are source-limited.\nReplay blocks editor preview/export and cancels active editor work.\nNo BGM, titles, speed changes, proxy preview or multi-track composition yet."),L"LiteReplay Editor",MB_OK);

    if(current>=0&&size_t(current)<project.clips.size()){
        if(id==21&&current>0){stopPreview();std::swap(project.clips[size_t(current)],project.clips[size_t(current-1)]);--current;rebuild();selectClip(current);markDirty();}
        if(id==22&&size_t(current+1)<project.clips.size()){stopPreview();std::swap(project.clips[size_t(current)],project.clips[size_t(current+1)]);++current;rebuild();selectClip(current);markDirty();}
        if(id==23){if(project.clips.size()>=256)throw std::runtime_error("Maximum 256 clips");stopPreview();auto copy=project.clips[size_t(current)];project.clips.insert(project.clips.begin()+current+1,copy);++current;rebuild();selectClip(current);markDirty();}
        if(id==24){stopPreview();project.clips.erase(project.clips.begin()+current);rebuild();selectClip(current);markDirty();}
        if(id==36){if(project.clips.size()>=256)throw std::runtime_error("Maximum 256 clips");auto first=project.clips[size_t(current)],second=first;first.out=previewTime;second.in=previewTime;validate(first);validate(second);stopPreview();project.clips[size_t(current)]=first;project.clips.insert(project.clips.begin()+current+1,second);rebuild();selectClip(current);markDirty();}
        if(id==37){auto& c=project.clips[size_t(current)];c.in=0;c.out=c.duration;c.volume=100;selectClip(current);rebuild();markDirty();}
    }
}
LRESULT CALLBACK mainProc(HWND h,UINT m,WPARAM w,LPARAM l){try{switch(m){case WM_ERASEBKGND:lr_ui::paint(h,reinterpret_cast<HDC>(w));return 1;case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:return lr_ui::color(h,m,w,l);
case WM_GETMINMAXINFO:reinterpret_cast<MINMAXINFO*>(l)->ptMinTrackSize={1030,710};return 0;case WM_SIZE:if(preview)layout();return 0;case WM_COMMAND:{int id=LOWORD(w),notice=HIWORD(w);if(!busy&&sourceReady()&&(((id==40||id==41||id==42)&&notice==CBN_SELCHANGE)||(id==43&&notice==EN_KILLFOCUS))){readSettings();return 0;}if(id==14&&notice==CBN_SELCHANGE){int encoder=int(SendMessageW(ui[42],CB_GETCURSEL,0,0));english=SendMessageW(ui[14],CB_GETCURSEL,0,0)==1;setLanguage();SendMessageW(ui[42],CB_SETCURSEL,encoder,0);if(busy)status(tr(L"任务处理中…",L"Task in progress…"));return 0;}if(id==20&&notice==LBN_SELCHANGE&&!busy){selectClip(int(SendMessageW(ui[20],LB_GETCURSEL,0,0)));return 0;}if(id==20&&notice==LBN_DBLCLK&&!busy){playClip();return 0;}if(notice==BN_CLICKED)action(id);return 0;}case WM_HSCROLL:if(reinterpret_cast<HWND>(l)==ui[50]&&current>=0&&!busy){auto& c=project.clips[size_t(current)];previewTime=c.in+(c.out-c.in)*double(SendMessageW(ui[50],TBM_GETPOS,0,0))/10000;SetWindowTextW(ui[62],clockText(previewTime).c_str());InvalidateRect(timeline,nullptr,FALSE);scrubbing=LOWORD(w)==TB_THUMBTRACK;if(!scrubbing)seekPreview();return 0;}break;case WM_TIMER:if(w==2){KillTimer(h,2);autosave();}else tick();return 0;case WM_CLOSE:stopPreview();return 0;case WM_DESTROY:KillTimer(h,1);KillTimer(h,2);return 0;}}catch(const std::exception& e){error(e);}return DefWindowProcW(h,m,w,l);}

void atomicProject(const std::filesystem::path& path){auto tmp=path;tmp+=L".tmp";writeFile(tmp,serialize(project));if(!MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){DeleteFileW(tmp.c_str());throw std::runtime_error("Cannot commit project autosave");}}
void autosave(){atomicProject(autosavePath);}
void undoRedo(bool redo){Project next=project;bool changed=redo?history.redo(next):history.undo(next);if(!changed)return;stopPreview();project=std::move(next);current=project.clips.empty()?-1:std::clamp(current,0,int(project.clips.size())-1);rebuild();selectClip(current);updateSettingsControls();dirty=true;SetTimer(mainWindow,2,800,nullptr);}
void recover(){if(!discard())return;auto next=deserialize(readFile(autosavePath));stopPreview();project=std::move(next);history.reset(project);projectPath.clear();current=project.clips.empty()?-1:0;rebuild();selectClip(current);updateSettingsControls();dirty=true;status(tr(L"已恢复自动保存，原视频需保留。",L"Autosave recovered; source files must still exist."));if(!sourceReady())refreshSpecifications();}
void streamCopy(){
 if(recording&&recording())throw std::runtime_error("Stop replay before export");if(current<0)throw std::runtime_error("Select a clip first");applyTrim();auto clip=project.clips[size_t(current)];if(clip.volume!=100)throw std::runtime_error("Stream copy requires 100% volume / 快速裁剪要求音量100%");
 if(MessageBoxW(mainWindow,tr(L"仅导出选中片段，不重新编码。切点受关键帧影响，可能包含额外画面；分辨率/帧率设置不生效。继续？",L"Export only the selected clip without re-encoding. Keyframe boundaries may include extra footage. Resolution/FPS settings do not apply. Continue?"),L"Stream copy",MB_OKCANCEL|MB_ICONINFORMATION)!=IDOK)return;
 auto paths=choose(true,L"MP4\0*.mp4\0\0",L"mp4");if(paths.empty())return;auto output=std::filesystem::absolute(paths[0]);if(std::filesystem::exists(output))throw std::runtime_error("Choose a new output filename / 请使用新的输出文件名");exportDestination=output.wstring();exportStarted=GetTickCount64();if(exportRequested)exportRequested(exportDestination);startWork();
 worker=std::thread([clip,output]{WorkerResult r;r.kind=2;r.path=output.wstring();auto tmp=output.parent_path()/(L".LiteReplay-copy-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));try{if(!std::filesystem::create_directory(tmp))throw std::runtime_error("Cannot create export work directory");auto result=run(toolsDir/L"ffmpeg.exe",copyArgs(clip,utf8((tmp/L"output.mp4").wstring())),cancel);writeFile(logPath,result.output);if(result.code)throw std::runtime_error(result.output);if(cancel)throw std::runtime_error("Canceled");auto verify=run(toolsDir/L"ffprobe.exe",{"-v","error","-show_entries","stream=codec_type","-of","default=noprint_wrappers=1",utf8((tmp/L"output.mp4").wstring())},cancel,{},30000);if(verify.code||verify.output.find("codec_type=video")==std::string::npos||cancel)throw std::runtime_error("Export validation failed or canceled");if(!MoveFileExW((tmp/L"output.mp4").c_str(),output.c_str(),MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot commit export");progress=100;}catch(const std::exception& e){r.error=e.what();}r.canceled=cancel;std::error_code ec;std::filesystem::remove_all(tmp,ec);finishWork(std::move(r));});
}
void initialize(HWND parent,const std::filesystem::path& root,const std::filesystem::path& tools,bool en,std::function<bool()> rec,std::function<void(const std::wstring&)> onSaved){
 if(mainWindow)return;english=en;recording=std::move(rec);saved=std::move(onSaved);toolsDir=tools;autosavePath=root/L"editor-autosave.lcut";logPath=root/L"editor-export.log";
 WNDCLASSW wc{};wc.lpfnWndProc=mainProc;wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=L"LiteReplayEditorPanel";RegisterClassW(&wc);
 mainWindow=CreateWindowExW(WS_EX_CONTROLPARENT,wc.lpszClassName,L"LiteReplay Editor — Yuzi_Lj",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,0,0,1120,780,parent,nullptr,wc.hInstance,nullptr);if(!mainWindow)throw std::runtime_error("Cannot create editor panel");buildUI();history.reset(project);
}
void hide(){if(mainWindow)stopPreview();}
void localize(bool en){english=en;if(mainWindow){auto encoder=SendMessageW(ui[42],CB_GETCURSEL,0,0);setLanguage();SendMessageW(ui[42],CB_SETCURSEL,encoder,0);}}
void prioritizeReplay(){if(!mainWindow)return;stopPreview();if(busy){cancel=true;if(worker.joinable())worker.join();tick();}}
void shutdown(){if(!mainWindow)return;cancel=true;if(worker.joinable())worker.join();
 if(resultReady&&workResult.kind==2&&!workResult.canceled&&workResult.error.empty()&&saved){saved(workResult.path);resultReady=false;}
 if(dirty)try{autosave();}catch(...){}stopPreview();KillTimer(mainWindow,1);KillTimer(mainWindow,2);lr_ui::sections.erase(mainWindow);DestroyWindow(mainWindow);mainWindow=nullptr;if(font)DeleteObject(font);font=nullptr;if(mfModule)FreeLibrary(mfModule);mfModule=nullptr;}
bool dialog(MSG* msg){if(!mainWindow||!IsWindowVisible(mainWindow)||(msg->hwnd!=mainWindow&&!IsChild(mainWindow,msg->hwnd)))return false;
 if(msg->message==WM_KEYDOWN&&(GetKeyState(VK_CONTROL)&0x8000)&&!busy){int id=msg->wParam=='I'?10:msg->wParam=='O'?12:msg->wParam=='S'?13:msg->wParam=='Z'?90:msg->wParam=='Y'?91:0;wchar_t cls[32]{};GetClassNameW(msg->hwnd,cls,32);if((id==90||id==91)&&wcscmp(cls,L"Edit")==0)id=0;if(id){try{action(id);}catch(const std::exception& e){error(e);}return true;}}
 if(msg->message==WM_KEYDOWN&&!busy&&!(GetKeyState(VK_CONTROL)&0x8000)){wchar_t cls[32]{};GetClassNameW(msg->hwnd,cls,32);if(wcscmp(cls,L"Edit")!=0&&wcscmp(cls,L"ComboBox")!=0){int id=msg->wParam==VK_SPACE?25:msg->wParam==VK_LEFT?101:msg->wParam==VK_RIGHT?102:msg->wParam=='I'?34:msg->wParam=='O'?35:msg->wParam=='S'?36:0;if(id){try{action(id);}catch(const std::exception& e){error(e);}return true;}}}
 return IsDialogMessageW(mainWindow,msg)!=FALSE;}
}
