#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>

#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdinputdriver.h>
#include <vd2/VDXFrame/VideoFilterDialog.h>
#include <string>
#include "InputFile2.h"
#include "export.h"
#include "cineform.h"
#include "a_compress.h"
#include "resource.h"

#ifdef _MSC_VER
#include <delayimp.h>
#pragma comment(lib, "avcodec-57")
#pragma comment(lib, "avformat-57")
#pragma comment(lib, "avutil-55")
#pragma comment(lib, "swscale-4")
#pragma comment(lib, "swresample-2")
#pragma comment(lib, "delayimp")
#endif

#pragma comment(lib, "vfw32")

HINSTANCE hInstance;
bool config_decode_raw = false;
bool config_decode_magic = false;
bool config_decode_cfhd = false;
bool config_force_thread = false;
bool config_disable_cache = false;
float config_cache_size = 0.5;
void saveConfig();

int av_initialized;

static int logmode = 0;

void av_log_func(void* obj, int type, const char* msg, va_list arg)
{
  switch(type){
  case AV_LOG_PANIC:
  case AV_LOG_FATAL:
  case AV_LOG_ERROR:
  case AV_LOG_WARNING:
    break;
  default:
    if(logmode==0) return;
  }

  char buf[1024];
  vsprintf(buf,msg,arg);
  OutputDebugString(buf);
  //DebugBreak();
}

void init_av()
{
  if(!av_initialized){
    av_initialized = 1;
    av_register_all();
    avcodec_register_all();
    av_register_cfhd();

    #ifdef FFDEBUG
    //av_log_set_callback(av_log_func);
    //av_log_set_level(AV_LOG_INFO);
    //av_log_set_flags(AV_LOG_SKIP_REPEATED);
    #endif
  }
}

class ConfigureDialog: public VDXVideoFilterDialog {
public:
	virtual INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
  void Show(HWND parent){
    VDXVideoFilterDialog::Show(hInstance,MAKEINTRESOURCE(IDD_SYS_INPUT_OPTIONS),parent);
  }
  void init_cache();
};

float GetDlgItemFloat(HWND wnd, int id, float bv){
  HWND w1 = GetDlgItem(wnd,id);
  char buf[128];
  if(!GetWindowText(w1,buf,128)) return bv;
  float v;
  if(sscanf(buf,"%f",&v)!=1) return bv;
  return v;
}

void SetDlgItemFloat(HWND wnd, int id, float v){
  HWND w1 = GetDlgItem(wnd,id);
  char buf[128];
  sprintf(buf,"%g",v);
  SetWindowText(w1,buf);
}

void ConfigureDialog::init_cache()
{
  MEMORYSTATUSEX ms = {sizeof(MEMORYSTATUSEX)};
  GlobalMemoryStatusEx(&ms);
  uint64_t gb1 = 0x40000000;
  int max = int(ms.ullTotalPhys/gb1);

  SetDlgItemFloat(mhdlg,IDC_CACHE_SIZE,config_cache_size);
  SendMessage(GetDlgItem(mhdlg, IDC_CACHE_SPIN), UDM_SETRANGE, 0, (LPARAM)MAKELONG(max,0));
}

INT_PTR ConfigureDialog::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch( msg ){
  case WM_INITDIALOG:
    CheckDlgButton(mhdlg,IDC_DECODE_RAW, config_decode_raw ? BST_CHECKED:BST_UNCHECKED);
    CheckDlgButton(mhdlg,IDC_DECODE_MAGIC, config_decode_magic ? BST_CHECKED:BST_UNCHECKED);
    CheckDlgButton(mhdlg,IDC_DECODE_CFHD, config_decode_cfhd ? BST_CHECKED:BST_UNCHECKED);
    CheckDlgButton(mhdlg,IDC_FORCE_THREAD, config_force_thread ? BST_CHECKED:BST_UNCHECKED);
    CheckDlgButton(mhdlg,IDC_DISABLE_CACHE2, config_disable_cache ? BST_CHECKED:BST_UNCHECKED);
    init_cache();
    return TRUE;
  case WM_COMMAND:
    switch(LOWORD(wParam)){
    case IDOK:
      config_decode_raw = IsDlgButtonChecked(mhdlg,IDC_DECODE_RAW)!=0;
      config_decode_magic = IsDlgButtonChecked(mhdlg,IDC_DECODE_MAGIC)!=0;
      config_decode_cfhd = IsDlgButtonChecked(mhdlg,IDC_DECODE_CFHD)!=0;
      config_force_thread = IsDlgButtonChecked(mhdlg,IDC_FORCE_THREAD)!=0;
      config_disable_cache = IsDlgButtonChecked(mhdlg,IDC_DISABLE_CACHE2)!=0;
      config_cache_size = GetDlgItemFloat(mhdlg,IDC_CACHE_SIZE,0.5);
      saveConfig();
      EndDialog(mhdlg, TRUE);
      return TRUE;

    case IDCANCEL:
      EndDialog(mhdlg, FALSE);
      return TRUE;
    }
  }
  return false;
}

bool VDXAPIENTRY StaticConfigureProc(VDXHWND parent)
{
  ConfigureDialog dlg;
  dlg.Show((HWND)parent);
  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool VDXAPIENTRY ff_create(const VDXInputDriverContext *pContext, IVDXInputFileDriver **ppDriver)
{
  VDFFInputFileDriver *p = new VDFFInputFileDriver(*pContext);
  if(!p) return false;
  *ppDriver = p;
  p->AddRef();
  return true;
}

#define option_video_init L"FFMpeg : video|*.mov;*.mp4;*.avi"
std::wstring option_video = option_video_init;
//std::wstring pattern_video; // example "*.mov|*.mp4|*.avi"

VDXInputDriverDefinition ff_video={
  sizeof(VDXInputDriverDefinition),
  VDXInputDriverDefinition::kFlagSupportsVideo|
  VDXInputDriverDefinition::kFlagSupportsAudio|
  VDXInputDriverDefinition::kFlagCustomSignature,
  1, //priority, reset from options
  0, //SignatureLength
  0, //Signature
  0, //pattern_video.c_str(),
  option_video.c_str(),
  L"Caching input driver",
  ff_create
};

#define option_image_init L"FFMpeg : images|*.tiff;*.dpx"
std::wstring option_image = option_image_init;

VDXInputDriverDefinition ff_image={
  sizeof(VDXInputDriverDefinition),
  VDXInputDriverDefinition::kFlagSupportsVideo|
  VDXInputDriverDefinition::kFlagCustomSignature|
  VDXInputDriverDefinition::kFlagDuplicate,
  1, //priority, reset from options
  0, //SignatureLength
  0, //Signature
  0,
  option_image.c_str(),
  L"Caching input driver",
  ff_create
};

#define option_audio_init L"FFMpeg : audio|*.wav;*.ogg"
std::wstring option_audio = option_audio_init;

VDXInputDriverDefinition ff_audio={
  sizeof(VDXInputDriverDefinition),
  VDXInputDriverDefinition::kFlagSupportsAudio|
  VDXInputDriverDefinition::kFlagCustomSignature|
  VDXInputDriverDefinition::kFlagDuplicate,
  1, //priority, reset from options
  0, //SignatureLength
  0, //Signature
  0,
  option_audio.c_str(),
  L"Caching input driver",
  ff_create
};

VDXPluginInfo ff_plugin_video={
  sizeof(VDXPluginInfo),
  L"Caching input driver",
  L"Anton Shekhovtsov",
  L"Loads and decode files through ffmpeg libs.",
  (1 << 24) + (9 << 16),
  kVDXPluginType_Input,
  0,
  10, // min api version
  kVDXPlugin_APIVersion,
  8,  // min input api version
  kVDXPlugin_InputDriverAPIVersion,
  &ff_video
};

VDXPluginInfo ff_plugin_image;
VDXPluginInfo ff_plugin_audio;

VDPluginInfo* kPlugins[]={
  &ff_output_info,
  &ff_mp3enc_info,
  &ff_aacenc_info,
  &ff_flacenc_info,
  &ff_alacenc_info,
  &ff_vorbisenc_info,
  &ff_opusenc_info,
  &ff_plugin_video,
  &ff_plugin_image,
  &ff_plugin_audio,
  0
};

extern "C" VDPluginInfo** __cdecl VDGetPluginInfo()
{
  return kPlugins;
}

void saveConfig()
{
  wchar_t buf[MAX_PATH+128];
  size_t n = GetModuleFileNameW(hInstance, buf, MAX_PATH);
  if(n<=0) return;
  buf[n] = 0;

  wchar_t* p1 = wcsrchr(buf,'\\');
  wchar_t* p2 = wcsrchr(buf,'/');
  if(p2>p1) p1=p2;
  if(!p1) return;
  *p1 = 0;

  wcscat(buf,L"\\cch_input.ini");

  WritePrivateProfileStringW(L"force_ffmpeg",L"raw",config_decode_raw ? L"1":L"0",buf);
  WritePrivateProfileStringW(L"force_ffmpeg",L"MagicYUV",config_decode_magic ? L"1":L"0",buf);
  WritePrivateProfileStringW(L"force_ffmpeg",L"CineformHD",config_decode_cfhd ? L"1":L"0",buf);
  WritePrivateProfileStringW(L"decode_model",L"force_frame_thread",config_force_thread ? L"1":L"0",buf);
  WritePrivateProfileStringW(L"decode_model",L"disable_cache",config_disable_cache ? L"1":L"0",buf);

  wchar_t buf2[128];
  swprintf(buf2,128,L"%g",config_cache_size);
  WritePrivateProfileStringW(L"decode_model",L"cache_size",buf2,buf);

  WritePrivateProfileStringW(0,0,0,buf);
}

void loadConfig()
{
  wchar_t buf[MAX_PATH+128];
  size_t n = GetModuleFileNameW(hInstance, buf, MAX_PATH);
  if(n<=0) return;
  buf[n] = 0;

  wchar_t* p1 = wcsrchr(buf,'\\');
  wchar_t* p2 = wcsrchr(buf,'/');
  if(p2>p1) p1=p2;
  if(!p1) return;
  *p1 = 0;

  wcscat(buf,L"\\cch_input.ini");

  config_decode_raw = GetPrivateProfileIntW(L"force_ffmpeg",L"raw",0,buf)!=0;
  config_decode_magic = GetPrivateProfileIntW(L"force_ffmpeg",L"MagicYUV",0,buf)!=0;
  config_decode_cfhd = GetPrivateProfileIntW(L"force_ffmpeg",L"CineformHD",0,buf)!=0;
  config_force_thread = GetPrivateProfileIntW(L"decode_model",L"force_frame_thread",0,buf)!=0;
  config_disable_cache = GetPrivateProfileIntW(L"decode_model",L"disable_cache",0,buf)!=0;

  wchar_t buf2[128];
  GetPrivateProfileStringW(L"decode_model",L"cache_size",L"0.5",buf2,128,buf);
  float v2;
  if(swscanf(buf2,L"%f",&v2)==1) config_cache_size = v2; else config_cache_size = 0.5;

  ff_plugin_video.mpStaticConfigureProc = 0;

  ff_plugin_image = ff_plugin_video;
  ff_plugin_image.mpTypeSpecificInfo = &ff_image;
  ff_plugin_image.mpName = L"Caching input driver (images)"; // name must be unique

  ff_plugin_audio = ff_plugin_video;
  ff_plugin_audio.mpTypeSpecificInfo = &ff_audio;
  ff_plugin_audio.mpName = L"Caching input driver (audio)"; // name must be unique

  ff_plugin_video.mpStaticConfigureProc = StaticConfigureProc;

  int priority = GetPrivateProfileIntW(L"priority",L"default",ff_video.mPriority,buf);
  ff_video.mPriority = priority;
  ff_image.mPriority = priority;
  ff_audio.mPriority = priority;

  {
    option_video = option_video_init;
    int mp = option_video.rfind('|');
    wchar_t mask[2048];
    GetPrivateProfileStringW(L"file_mask",L"video",&option_video[mp+1],mask,2048,buf);
    option_video.resize(mp+1);

    int p = 0;
    int n = wcslen(mask);
    int count1 = 0;
    while(p<n){
      int p1 = n;
      wchar_t* p2 = wcschr(mask+p,';');
      if(p2 && p2-mask<p1) p1 = p2-mask;

      if(count1) option_video += L";";
      option_video += std::wstring(mask+p,p1-p);

      p = p1+1;
      count1++;
    }

    ff_video.mpFilenamePattern = option_video.c_str();
  }

  {
    option_image = option_image_init;
    int mp = option_image.rfind('|');
    wchar_t mask[2048];
    GetPrivateProfileStringW(L"file_mask",L"images",&option_image[mp+1],mask,2048,buf);
    option_image.resize(mp+1);

    int p = 0;
    int n = wcslen(mask);
    int count1 = 0;
    while(p<n){
      int p1 = n;
      wchar_t* p2 = wcschr(mask+p,';');
      if(p2 && p2-mask<p1) p1 = p2-mask;

      if(count1) option_image += L";";
      option_image += std::wstring(mask+p,p1-p);

      p = p1+1;
      count1++;
    }

    ff_image.mpFilenamePattern = option_image.c_str();
  }

  {
    option_audio = option_audio_init;
    int mp = option_audio.rfind('|');
    wchar_t mask[2048];
    GetPrivateProfileStringW(L"file_mask",L"audio",&option_audio[mp+1],mask,2048,buf);
    option_audio.resize(mp+1);

    int p = 0;
    int n = wcslen(mask);
    int count1 = 0;
    while(p<n){
      int p1 = n;
      wchar_t* p2 = wcschr(mask+p,';');
      if(p2 && p2-mask<p1) p1 = p2-mask;

      if(count1) option_audio += L";";
      option_audio += std::wstring(mask+p,p1-p);

      p = p1+1;
      count1++;
    }

    ff_audio.mpFilenamePattern = option_audio.c_str();
  }
}

#ifdef _MSC_VER

HINSTANCE module_base[5];

bool loadModules()
{
  wchar_t buf[MAX_PATH+128];
  size_t n = GetModuleFileNameW(hInstance, buf, MAX_PATH);
  if(n<=0) return false;
  buf[n] = 0;

  wchar_t* p1 = wcsrchr(buf,'\\');
  wchar_t* p2 = wcsrchr(buf,'/');
  if(p2>p1) p1=p2;
  if(!p1) return false;

  *p1 = 0;
  wcscat(buf,L"\\ffdlls\\");
  p1+=8;

  wchar_t* module_name[5] = {
    L"avutil-55.dll",
    L"swresample-2.dll",
    L"swscale-4.dll",
    L"avcodec-57.dll",
    L"avformat-57.dll",
  };

  {for(int i=0; i<5; i++){
    *p1 = 0;
    wcscat(buf,module_name[i]);
    HINSTANCE h = LoadLibraryW(buf);
    if(!h){
      wchar_t buf2[MAX_PATH+128];
      wcscpy(buf2,L"Module failed to load correctly:\n");
      wcscat(buf2,buf);
      MessageBoxW(0,buf2,L"FFMpeg input driver",MB_OK|MB_ICONSTOP);
      return false;
    }
    module_base[i] = h;
  }}

  return true;
}

void unloadModules()
{
  char* module_name[5] = {
    "avutil-55.dll",
    "swresample-2.dll",
    "swscale-4.dll",
    "avcodec-57.dll",
    "avformat-57.dll",
  };

  {for(int i=4; i>=0; i--){
    __FUnloadDelayLoadedDLL2(module_name[i]);
  }}

  {for(int i=4; i>=0; i--){
    HINSTANCE h = module_base[i];
    if(h) FreeLibrary(h);
  }}
}

BOOLEAN WINAPI DllMain( IN HINSTANCE hDllHandle, IN DWORD nReason, IN LPVOID Reserved )
{
  switch ( nReason ){
  case DLL_PROCESS_ATTACH:
    hInstance = hDllHandle;
    if(!loadModules()) return false;
    loadConfig();
    return true;

  case DLL_PROCESS_DETACH:
    if(Reserved==0) unloadModules();
    return true;
  }

  return true;
}

#endif

#ifdef __GNUC__

BOOLEAN WINAPI DllMain( IN HINSTANCE hDllHandle, IN DWORD nReason, IN LPVOID Reserved )
{
  switch ( nReason ){
  case DLL_PROCESS_ATTACH:
    hInstance = hDllHandle;
    loadConfig();
    return true;

  case DLL_PROCESS_DETACH:
    return true;
  }

  return true;
}

#endif
