#pragma once
#include "common.hpp"
#include <mmdeviceapi.h>
#include <propsys.h>
namespace lr {
struct RecordingDevice {std::wstring id,name;};
inline std::vector<RecordingDevice> recordingDevices(bool microphone){
 ComPtr<IMMDeviceEnumerator> enumerator;check(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator)),"Enumerate recording devices");ComPtr<IMMDeviceCollection> list;check(enumerator->EnumAudioEndpoints(microphone?eCapture:eRender,DEVICE_STATE_ACTIVE,&list),"List audio endpoints");UINT count=0;check(list->GetCount(&count),"Count audio devices");std::vector<RecordingDevice> result;
 constexpr PROPERTYKEY nameKey={{0xa45c254e,0xdf1c,0x4efd,{0x80,0x20,0x67,0xd1,0x46,0xa8,0x50,0xe0}},14};
 for(UINT i=0;i<count;++i){ComPtr<IMMDevice> device;if(FAILED(list->Item(i,&device)))continue;LPWSTR id=nullptr;if(FAILED(device->GetId(&id)))continue;RecordingDevice row{id,id};CoTaskMemFree(id);ComPtr<IPropertyStore> props;if(SUCCEEDED(device->OpenPropertyStore(STGM_READ,&props))){PROPVARIANT v{};if(SUCCEEDED(props->GetValue(nameKey,&v))&&v.vt==VT_LPWSTR&&v.pwszVal)row.name=v.pwszVal;PropVariantClear(&v);}result.push_back(std::move(row));}return result;
}
inline void validateRecordingDevice(bool microphone,const std::wstring& id){
 ComPtr<IMMDeviceEnumerator> enumerator;check(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator)),"Audio device validation");ComPtr<IMMDevice> device;HRESULT hr=id.empty()?enumerator->GetDefaultAudioEndpoint(microphone?eCapture:eRender,eConsole,&device):enumerator->GetDevice(id.c_str(),&device);DWORD state=0;
 if(FAILED(hr)||FAILED(device->GetState(&state))||state!=DEVICE_STATE_ACTIVE)throw std::runtime_error(microphone?"Selected microphone is unavailable; refresh devices / 麦克风不可用，请连接设备后刷新并重新选择":"Selected system-audio output is unavailable / 系统声音设备不可用，请刷新并重新选择");
 ComPtr<IMMEndpoint> endpoint;check(device.As(&endpoint),"Audio endpoint type");EDataFlow flow;check(endpoint->GetDataFlow(&flow),"Audio endpoint flow");if(flow!=(microphone?eCapture:eRender))throw std::runtime_error("Wrong audio endpoint type / 音频设备类型错误");
}
}
