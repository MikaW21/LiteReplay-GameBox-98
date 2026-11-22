#pragma once
#include "common.hpp"
#include "screenshot_geometry.hpp"
#include "clipboard_dib.hpp"
#include <deque>
#include <wincodec.h>
#include <windowsx.h>
#include <vector>
#include <mutex>
#include <utility>
namespace lr {
class ScreenshotTool {
 HWND owner_,overlay_=nullptr;HBITMAP bitmap_=nullptr;std::vector<BYTE> pixels_;
 int x_=0,y_=0,width_=0,height_=0;POINT anchor_{},cursor_{};bool dragging_=false,english_=false;
 std::filesystem::path folder_;std::thread writer_;std::atomic<bool> writing_{false};std::mutex mutex_;std::deque<std::wstring> results_,savedPaths_;
 HGLOBAL clipboard_=nullptr;int clipboardTries_=0;bool clipboardEnglish_=false;
 static constexpr UINT Completed=WM_APP+4;
 void releaseCaptureImage(){if(bitmap_)DeleteObject(bitmap_);bitmap_=nullptr;std::vector<BYTE>().swap(pixels_);}
 void closeOverlay(){if(overlay_){HWND h=overlay_;overlay_=nullptr;DestroyWindow(h);}dragging_=false;}
 void capture(){
  releaseCaptureImage();x_=GetSystemMetrics(SM_XVIRTUALSCREEN);y_=GetSystemMetrics(SM_YVIRTUALSCREEN);width_=GetSystemMetrics(SM_CXVIRTUALSCREEN);height_=GetSystemMetrics(SM_CYVIRTUALSCREEN);pixels_.resize(shotBytes(width_,height_));
  HDC screen=GetDC(nullptr);if(!screen)throw std::runtime_error("Cannot access desktop / 无法访问桌面");HDC memory=CreateCompatibleDC(screen);
  if(!memory){ReleaseDC(nullptr,screen);throw std::runtime_error("Cannot create screenshot DC");}
  bitmap_=CreateCompatibleBitmap(screen,width_,height_);BOOL ok=FALSE;
  if(bitmap_){auto old=SelectObject(memory,bitmap_);ok=BitBlt(memory,0,0,width_,height_,screen,x_,y_,SRCCOPY|CAPTUREBLT);SelectObject(memory,old);}
  BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width_;info.bmiHeader.biHeight=-height_;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
  if(ok)ok=GetDIBits(memory,bitmap_,0,height_,pixels_.data(),&info,DIB_RGB_COLORS)==UINT(height_);
  DeleteDC(memory);ReleaseDC(nullptr,screen);if(ok)for(size_t i=3;i<pixels_.size();i+=4)pixels_[i]=255;if(!ok){releaseCaptureImage();throw std::runtime_error("Desktop screenshot failed / 屏幕截图失败");}
 }
 static void writePng(const std::filesystem::path& path,const std::vector<BYTE>& pixels,int width,int height){
  ComPtr<IWICImagingFactory> factory;check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)),"Create PNG factory");
  ComPtr<IWICStream> stream;check(factory->CreateStream(&stream),"Create PNG stream");check(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE),"Open screenshot file");
  ComPtr<IWICBitmapEncoder> encoder;check(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder),"PNG encoder");check(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache),"Initialize PNG");
  ComPtr<IWICBitmapFrameEncode> frame;ComPtr<IPropertyBag2> bag;check(encoder->CreateNewFrame(&frame,&bag),"PNG frame");check(frame->Initialize(bag.Get()),"Initialize frame");check(frame->SetSize(width,height),"PNG size");
  WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;check(frame->SetPixelFormat(&format),"PNG pixel format");
  if(format!=GUID_WICPixelFormat32bppBGRA)throw std::runtime_error("PNG encoder does not support BGR32");
  check(frame->WritePixels(height,width*4,UINT(pixels.size()),const_cast<BYTE*>(pixels.data())),"Write PNG");check(frame->Commit(),"Commit PNG frame");check(encoder->Commit(),"Commit PNG");
 }
 void report(std::wstring text){ {std::lock_guard<std::mutex> lock(mutex_);results_.push_back(std::move(text));}PostMessageW(owner_,Completed,0,0);}
 void queueClipboard(const std::vector<BYTE>& pixels,int w,int h){
  try{auto dib=clipboardDib(pixels,w,h);clipboard_=GlobalAlloc(GMEM_MOVEABLE,dib.size());
   if(!clipboard_)throw std::bad_alloc();void* memory=GlobalLock(clipboard_);if(!memory){GlobalFree(clipboard_);clipboard_=nullptr;throw std::bad_alloc();}
   memcpy(memory,dib.data(),dib.size());GlobalUnlock(clipboard_);clipboardTries_=0;clipboardEnglish_=english_;pumpClipboard();
  }catch(const std::exception& e){report((english_?L"Clipboard copy failed: ":L"复制到剪贴板失败：")+wide(e.what()));}
 }
 void save(ShotRect r){
  int w=r.right-r.left,h=r.bottom-r.top;if(w<1||h<1){cancel();return;}
  std::vector<BYTE> crop(shotBytes(w,h));for(int row=0;row<h;++row)memcpy(crop.data()+size_t(row)*w*4,pixels_.data()+(size_t(row+r.top)*width_+r.left)*4,size_t(w)*4);
  closeOverlay();releaseCaptureImage();queueClipboard(crop,w,h);if(writer_.joinable())writer_.join();writing_=true;auto folder=folder_;bool en=english_;
  try{writer_=std::thread([this,folder,en,w,h,pixels=std::move(crop)](){
   SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
   HRESULT co=CoInitializeEx(nullptr,COINIT_MULTITHREADED);std::filesystem::path temp;std::wstring message;
   try{check(co,"Screenshot COM");std::filesystem::create_directories(folder);GUID id{};check(CoCreateGuid(&id),"Screenshot ID");wchar_t guid[40];StringFromGUID2(id,guid,40);
    SYSTEMTIME now{};GetLocalTime(&now);wchar_t stamp[64];swprintf_s(stamp,L"Screenshot_%04u%02u%02u_%02u%02u%02u_",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond);
    auto dest=folder/(std::wstring(stamp)+guid+L".png");temp=dest;temp+=L".partial";
    HANDLE reserved=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(reserved==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create screenshot file");CloseHandle(reserved);
    writePng(temp,pixels,w,h);if(!MoveFileExW(temp.c_str(),dest.c_str(),MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot finalize screenshot");temp.clear();{std::lock_guard<std::mutex> lock(mutex_);savedPaths_.push_back(dest.wstring());}message=(en?L"Screenshot saved: ":L"截图已保存：")+dest.wstring();
   }catch(const std::exception& e){if(!temp.empty())DeleteFileW(temp.c_str());message=(en?L"Screenshot failed: ":L"截图失败：")+wide(e.what());}
   if(SUCCEEDED(co))CoUninitialize();writing_=false;report(std::move(message));
  });}catch(...){writing_=false;throw;}
 }
 static LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
  auto self=reinterpret_cast<ScreenshotTool*>(GetWindowLongPtrW(h,GWLP_USERDATA));if(msg==WM_NCCREATE){self=static_cast<ScreenshotTool*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
  if(!self)return DefWindowProcW(h,msg,wp,lp);
  try{switch(msg){
   case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);HDC memory=CreateCompatibleDC(dc);if(memory&&self->bitmap_){auto old=SelectObject(memory,self->bitmap_);BitBlt(dc,0,0,self->width_,self->height_,memory,0,0,SRCCOPY);SelectObject(memory,old);}if(memory)DeleteDC(memory);
    SetBkColor(dc,RGB(25,25,25));SetTextColor(dc,RGB(255,255,255));const wchar_t* text=self->english_?L"Drag to select. Release to save. Esc / right-click to cancel. — Yuzi_Lj":L"拖动框选，松开保存；Esc / 右键取消。软件由 Yuzi_Lj 制作";TextOutW(dc,20,20,text,int(wcslen(text)));
    if(self->dragging_){auto r=shotRect(self->anchor_.x,self->anchor_.y,self->cursor_.x,self->cursor_.y,self->width_,self->height_);RECT box{r.left,r.top,r.right,r.bottom};FrameRect(dc,&box,reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));}EndPaint(h,&ps);return 0;}
   case WM_LBUTTONDOWN:self->anchor_=self->cursor_={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};self->dragging_=true;SetCapture(h);return 0;
   case WM_MOUSEMOVE:if(self->dragging_){self->cursor_={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};InvalidateRect(h,nullptr,FALSE);}return 0;
   case WM_LBUTTONUP:if(self->dragging_){self->cursor_={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};self->dragging_=false;ReleaseCapture();self->save(shotRect(self->anchor_.x,self->anchor_.y,self->cursor_.x,self->cursor_.y,self->width_,self->height_));}return 0;
   case WM_KEYDOWN:if(wp==VK_ESCAPE)self->cancel();return 0;
   case WM_RBUTTONDOWN:case WM_CLOSE:case WM_DISPLAYCHANGE:self->cancel();return 0;
   case WM_KILLFOCUS:if(self->overlay_)self->cancel();return 0;
  }}catch(const std::exception& e){self->cancel();self->report(L"Screenshot failed / 截图失败："+wide(e.what()));}
  return DefWindowProcW(h,msg,wp,lp);
 }
public:
 explicit ScreenshotTool(HWND owner):owner_(owner){}
 ~ScreenshotTool(){cancel();KillTimer(owner_,76);if(clipboard_)GlobalFree(clipboard_);if(writer_.joinable())writer_.join();}
 void cancel(){closeOverlay();releaseCaptureImage();}
 std::wstring takeResult(){if(writer_.joinable()&&!writing_)writer_.join();std::lock_guard<std::mutex> lock(mutex_);std::wstring result;for(auto& item:results_){if(!result.empty())result+=L"\n";result+=item;}results_.clear();return result;}
 void finish(){cancel();if(writer_.joinable())writer_.join();}
 std::deque<std::wstring> takeSavedPaths(){std::lock_guard<std::mutex> lock(mutex_);return std::exchange(savedPaths_,{});}
 void pumpClipboard(){
  if(!clipboard_){KillTimer(owner_,76);return;}
  bool ok=false,opened=OpenClipboard(owner_)!=FALSE;
  if(opened){if(EmptyClipboard()&&SetClipboardData(CF_DIB,clipboard_)){clipboard_=nullptr;ok=true;}CloseClipboard();}
  else if(++clipboardTries_<10){if(SetTimer(owner_,76,50,nullptr))return;}
  KillTimer(owner_,76);if(clipboard_){GlobalFree(clipboard_);clipboard_=nullptr;}
  report(ok?(clipboardEnglish_?L"Screenshot copied. Press Ctrl+V to paste.":L"截图已复制，可按 Ctrl+V 粘贴。"):(clipboardEnglish_?L"Clipboard unavailable; PNG saving continues.":L"剪贴板不可用，仍会继续保存 PNG。"));
 }
 void start(bool region,const std::filesystem::path& folder,bool english){
  if(writing_||overlay_||clipboard_)throw std::runtime_error("Screenshot already active / 正在截图或保存，请稍候");folder_=folder;english_=english;
  try{capture();if(!region){save({0,0,width_,height_});return;}
   WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursorW(nullptr,IDC_CROSS);wc.lpszClassName=L"LiteReplayScreenshot";
   if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)throw std::runtime_error("Cannot register screenshot overlay");
   overlay_=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,wc.lpszClassName,L"LiteReplay — Yuzi_Lj",WS_POPUP,x_,y_,width_,height_,nullptr,nullptr,wc.hInstance,this);
   if(!overlay_)throw std::runtime_error("Cannot create screenshot overlay");ShowWindow(overlay_,SW_SHOW);SetForegroundWindow(overlay_);SetFocus(overlay_);
  }catch(...){cancel();throw;}
 }
};
}
