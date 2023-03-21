#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>
#include <string>

#include "fflayer.h"
#include "resource.h"

extern HINSTANCE hInstance;

void utf8_to_widechar(wchar_t *dst, int max_dst, const char *src);
void widechar_to_utf8(char *dst, int max_dst, const wchar_t *src);

//-------------------------------------------------------------------------------------------------

void edit_changed(HWND wnd)
{
  WPARAM id = GetWindowLong(wnd,GWL_ID);
  SendMessage(GetParent(wnd),WM_EDIT_CHANGED,id,(LPARAM)wnd);
}

LRESULT CALLBACK EditWndProc(HWND wnd,UINT msg,WPARAM wparam,LPARAM lparam)
{
  if(msg==WM_KEYDOWN){
    if(wparam==VK_RETURN){
      edit_changed(wnd);
      return 1;
    }
  }
  if(msg==WM_KILLFOCUS) edit_changed(wnd);

  WNDPROC p = (WNDPROC)GetWindowLongPtr(wnd,GWLP_USERDATA);
  return CallWindowProc(p,wnd,msg,wparam,lparam);
}

//-------------------------------------------------------------------------------------------------

extern std::wstring option_image;
extern std::wstring option_video;

bool logoOpenImage(HWND hwnd, wchar_t* path, int max_path) {
  OPENFILENAMEW ofn = {0};
  wchar_t szFile[MAX_PATH];

  if (path)
    wcscpy(szFile,path);
  else
    szFile[0] = 0;

  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hwnd;
  std::wstring image = option_image;
  image.resize(image.length()+1,0);
  int x1 = image.find('|');
  image[x1] = 0;
  std::wstring video = option_video;
  video.resize(video.length()+1,0);
  int x2 = video.find('|');
  video[x2] = 0;
  std::wstring all = L"All files (*.*)";
  all.resize(all.length()+1,0);
  all += L"*.*";
  all.resize(all.length()+1,0);

  std::wstring s;
  s += video;
  s += image;
  s += all;
  ofn.lpstrFilter = s.c_str();
  ofn.nFilterIndex = 2;
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_ENABLETEMPLATE;
  ofn.lpTemplateName = MAKEINTRESOURCEW(IDD_FFLAYER_FILE);
  ofn.hInstance = hInstance;

  BOOL result = GetOpenFileNameW(&ofn);

  if(!result && CommDlgExtendedError()){
    szFile[0] = 0;
    ofn.lpstrInitialDir = NULL;
    result = GetOpenFileNameW(&ofn);
  }

  if(result){
    wcscpy(path,szFile);
    return true;
  }

  return false;
}

static void __cdecl PreviewPositionCallback(int64 pos, void *pData)
{
  LogoDialog* dialog = (LogoDialog*)pData;
  dialog->init_ref(pos);
}

static void __cdecl PreviewZoomCallback(PreviewZoomInfo& info, void *pData)
{
  LogoDialog* dialog = (LogoDialog*)pData;
  if(info.flags & info.popup_click){
    dialog->filter->param.pos_x = info.x;
    dialog->filter->param.pos_y = info.y;
    dialog->init_pos();
    dialog->redo_frame();
  }
}

//-------------------------------------------------------------------------------------------------

bool LogoDialog::Show(HWND parent){
  return 0 != VDXVideoFilterDialog::Show(hInstance, MAKEINTRESOURCE(IDD_FFLAYER), parent);
}

INT_PTR LogoDialog::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam){
  switch(msg){
  case WM_INITDIALOG:
    {
      ifp->InitButton((VDXHWND)GetDlgItem(mhdlg, IDPREVIEW));
      init_edit(IDC_XPOS);
      init_edit(IDC_YPOS);
      init_edit(IDC_LOGOFILE);
      init_edit(IDC_REF_SINGLE);
      init_edit(IDC_REF_FOLLOW);
      init_edit(IDC_FOLLOW_RATE);
      init_pos();
      init_ref();
      init_rate();
      init_path();
      init_buttons();
      return true;
    }

  case WM_DESTROY:
    ifp->InitButton(0);
    break;

  case WM_COMMAND:
    switch(LOWORD(wParam)){
    case IDC_LOGOFILE_BROWSE:
      if(logoOpenImage(mhdlg, filter->param.path, MAX_PATH)){
        init_path();
        filter->update_file();
        if(filter->video){
          const FilterModPixmapInfo& layerInfo = filter->video->GetFrameBufferInfo();
          if(layerInfo.alpha_type){
            filter->param.blendMode = LogoParam::blend_alpha;
            if(layerInfo.alpha_type==FilterModPixmapInfo::kAlphaOpacity_pm)
              filter->param.blendMode = LogoParam::blend_alpha_pm;
          }
        }
        init_buttons();
        redo_frame();
      }
      return TRUE;

    case IDC_FILE_INFO:
      filter->file->DisplayInfo((VDXHWND)mhdlg);
      return TRUE;

    case IDPREVIEW:
      if(filter->fma && filter->fma->fmpreview){
        filter->fma->fmpreview->FMSetPositionCallback(PreviewPositionCallback,this);
        filter->fma->fmpreview->FMSetZoomCallback(PreviewZoomCallback,this);
      }
      ifp->Toggle((VDXHWND)mhdlg);
      break;

    case IDOK:
      EndDialog(mhdlg, true);
      return TRUE;

    case IDCANCEL:
      filter->param = old_param;
      EndDialog(mhdlg, false);
      return TRUE;

    case IDC_ALPHABLEND:
      {
        bool v = SendMessage((HWND)lParam,BM_GETCHECK,0,0)==BST_CHECKED;
        if(v) filter->param.blendMode = LogoParam::blend_alpha;
        if(!v) filter->param.blendMode = LogoParam::blend_replace;
        init_buttons();
        redo_frame();
      }
      return TRUE;

    case IDC_PREMULTALPHA:
      {
        bool v = SendMessage((HWND)lParam,BM_GETCHECK,0,0)==BST_CHECKED;
        if(v) filter->param.blendMode = LogoParam::blend_alpha_pm;
        if(!v) filter->param.blendMode = LogoParam::blend_alpha;
        init_buttons();
        redo_frame();
      }
      return TRUE;

    case IDC_LOOP:
      {
        bool v = SendMessage((HWND)lParam,BM_GETCHECK,0,0)==BST_CHECKED;
        if(v) filter->param.loopMode = LogoParam::loop_saw;
        if(!v) filter->param.loopMode = 0;
        redo_frame();
      }
      return TRUE;

    case IDC_ANIM_SINGLE:
      if(filter->param.animMode!=LogoParam::anim_single){
        filter->param.animMode = LogoParam::anim_single;
        filter->param.refFrame = 0;
        init_ref();
        init_buttons();
        redo_frame();
      }
      return TRUE;

    case IDC_ANIM_FOLLOW:
      if(filter->param.animMode!=LogoParam::anim_follow){
        filter->param.animMode = LogoParam::anim_follow;
        filter->param.refFrame = 0;
        init_ref();
        init_buttons();
        redo_frame();
      }
      return TRUE;
    }
    return TRUE;
  
  case WM_EDIT_CHANGED:
    {
      HWND item = (HWND)lParam;
      bool changed = false;
      if(wParam==IDC_XPOS) if(modify_value(item,filter->param.pos_x)) changed=true;
      if(wParam==IDC_YPOS) if(modify_value(item,filter->param.pos_y)) changed=true;
      if(wParam==IDC_REF_SINGLE) if(modify_value(item,filter->param.refFrame)) changed=true;
      if(wParam==IDC_REF_FOLLOW) if(modify_value(item,filter->param.refFrame)) changed=true;
      if(wParam==IDC_FOLLOW_RATE) if(modify_value(item,filter->param.rate)) changed=true;
      if(wParam==IDC_LOGOFILE){ 
        if(modify_path()){
          filter->update_file();
          init_buttons();
          changed=true;
        }
      }

      if(changed) redo_frame();
      return TRUE;
    }
  }

  return FALSE;
}

//-------------------------------------------------------------------------------------------------

void LogoDialog::init_buttons()
{
  bool enable_info = filter->video!=0;
  EnableWindow(GetDlgItem(mhdlg,IDC_FILE_INFO),enable_info);
  bool use_alpha = false;
  bool use_alpha_pm = false;
  if(filter->param.blendMode==LogoParam::blend_alpha) use_alpha = true;
  if(filter->param.blendMode==LogoParam::blend_alpha_pm){ use_alpha = true; use_alpha_pm = true; }
  SendDlgItemMessage(mhdlg, IDC_ALPHABLEND, BM_SETCHECK, use_alpha ? BST_CHECKED:BST_UNCHECKED, 0);
  SendDlgItemMessage(mhdlg, IDC_PREMULTALPHA, BM_SETCHECK, use_alpha_pm ? BST_CHECKED:BST_UNCHECKED, 0);

  EnableWindow(GetDlgItem(mhdlg,IDC_REF_SINGLE),filter->param.animMode==LogoParam::anim_single);
  EnableWindow(GetDlgItem(mhdlg,IDC_REF_FOLLOW),filter->param.animMode==LogoParam::anim_follow);
  EnableWindow(GetDlgItem(mhdlg,IDC_FOLLOW_RATE),filter->param.animMode==LogoParam::anim_follow);
  SendDlgItemMessage(mhdlg, IDC_ANIM_SINGLE, BM_SETCHECK, filter->param.animMode==LogoParam::anim_single ? BST_CHECKED:BST_UNCHECKED, 0);
  SendDlgItemMessage(mhdlg, IDC_ANIM_FOLLOW, BM_SETCHECK, filter->param.animMode==LogoParam::anim_follow ? BST_CHECKED:BST_UNCHECKED, 0);
  bool loop = filter->param.loopMode==LogoParam::loop_saw;
  SendDlgItemMessage(mhdlg, IDC_LOOP, BM_SETCHECK, loop ? BST_CHECKED:BST_UNCHECKED, 0);
}

void LogoDialog::init_path()
{
  SetDlgItemTextW(mhdlg, IDC_LOGOFILE, filter->param.path);
}

bool LogoDialog::modify_path()
{
  wchar_t buf[MAX_PATH];
  GetDlgItemTextW(mhdlg, IDC_LOGOFILE, buf, MAX_PATH);
  if(wcscmp(buf,filter->param.path)!=0){
    wcscpy(filter->param.path,buf);
    return true;
  }
  return false;
}

void LogoDialog::init_pos()
{
  char buf[80];
  sprintf(buf,"%d",filter->param.pos_x);
  SetDlgItemText(mhdlg, IDC_XPOS, buf);
  sprintf(buf,"%d",filter->param.pos_y);
  SetDlgItemText(mhdlg, IDC_YPOS, buf);
}

void LogoDialog::init_rate()
{
  char buf[80];
  sprintf(buf,"%g",filter->param.rate);
  SetDlgItemText(mhdlg, IDC_FOLLOW_RATE, buf);
}

void LogoDialog::init_ref(int64 r)
{
  int rframe = filter->currentRFrame(r);

  char buf[80];
  sprintf(buf,"%d",filter->param.animMode==LogoParam::anim_single ? filter->param.refFrame:rframe);
  SetDlgItemText(mhdlg, IDC_REF_SINGLE, buf);
  sprintf(buf,"%d",filter->param.animMode==LogoParam::anim_follow ? filter->param.refFrame:0);
  SetDlgItemText(mhdlg, IDC_REF_FOLLOW, buf);
}

bool LogoDialog::modify_value(HWND item, int& v)
{
  char buf[80];
  GetWindowText(item,buf,sizeof(buf));
  int val;
  if(sscanf(buf,"%d",&val)==1){
    v = val;
    return true;
  }
  return false;
}

bool LogoDialog::modify_value(HWND item, double& v)
{
  char buf[80];
  GetWindowText(item,buf,sizeof(buf));
  float val;
  if(sscanf(buf,"%f",&val)==1){
    v = val;
    return true;
  }
  return false;
}

void LogoDialog::init_edit(int id)
{
  HWND hWnd = GetDlgItem(mhdlg,id);
  LPARAM p = GetWindowLongPtr(hWnd,GWLP_WNDPROC);
  SetWindowLongPtr(hWnd,GWLP_USERDATA,p);
  SetWindowLongPtr(hWnd,GWLP_WNDPROC,(LPARAM)EditWndProc);
}

void LogoDialog::redo_frame()
{
  ifp->RedoFrame();
}

//-------------------------------------------------------------------------------------------------

void LogoFilter::GetSettingString(char* buf, int maxlen){
  char path[MAX_PATH];
  widechar_to_utf8(path,MAX_PATH,param.path);

  SafePrintf(buf, maxlen, " %s", path);
}

void LogoFilter::GetScriptString(char* buf, int maxlen){
  char path[MAX_PATH*2];
  widechar_to_utf8(path+MAX_PATH,MAX_PATH,param.path);
  char* s0 = path+MAX_PATH;
  char* s1 = path;
  while(1){
    int v = *s0; s0++;
    *s1 = v; s1++;
    if(v=='\\'){ *s1 = v; s1++; }
    if(v==0) break;
  }

  SafePrintf(buf, maxlen, "Config(%d,%d,%d,\"%s\",%d,%d,%g)", param.pos_x, param.pos_y, param.blendMode, path, param.animMode|param.loopMode, param.refFrame, param.rate);
}

void LogoFilter::ScriptConfig(IVDXScriptInterpreter* isi, const VDXScriptValue* argv, int argc){
  param.pos_x = argv[0].asInt();
  param.pos_y = argv[1].asInt();
  param.blendMode = argv[2].asInt();
  param.refFrame = argv[5].asInt();
  param.rate = argv[6].asDouble();

  int anim = argv[4].asInt();
  param.animMode = anim & LogoParam::anim_follow;
  param.loopMode = anim & LogoParam::loop_saw;

  wchar_t buf[MAX_PATH];
  utf8_to_widechar(buf,MAX_PATH,*argv[3].asString());
  if(wcscmp(buf,param.path)!=0){
    wcscpy(param.path,buf);
    file_dirty = true;
  }
}

void LogoFilter::clear()
{
  if(video) video->Release();
  video = 0;
  if(file) file->Release();
  file = 0;
  last_frame = -1;
  file_dirty = false;
}

void LogoFilter::init()
{
  file = 0;
  video = 0;
  last_frame = -1;
  file_dirty = false;
}

LogoFilter::LogoFilter(const LogoFilter& a)
{
  init(); 
  param = a.param;
  file_dirty = true;

  LogoFilter& ma = (LogoFilter&)a;
  ma.clear();
  ma.file_dirty = true;
}

void LogoFilter::update_file()
{
  clear();
  if(!param.path[0]) return;

  context.mpCallbacks = &callbacks;
  VDFFInputFileDriver driver(context);
  IVDXInputFile* ff;
  driver.CreateInputFile(0,&ff);
  file = dynamic_cast<VDFFInputFile*>(ff);
  file->cfg_frame_buffers = 0;
  file->cfg_skip_cfhd = false;
  file->Init(param.path,0);
  IVDXVideoSource* vs;
  file->GetVideoSource(0,&vs);

  if(!vs){
    clear();
    return;
  }

  video = dynamic_cast<VDFFVideoSource*>(vs);

  video->SetTargetFormat(nsVDXPixmap::kPixFormat_XRGB8888,false);
  VDXStreamSourceInfo info;
  video->GetStreamSourceInfo(info);
  frame_count = (int)info.mSampleCount;
}

bool LogoFilter::Configure(VDXHWND hwnd)
{
  if(file_dirty) update_file();

  LogoDialog dlg(fa->ifp);
  dlg.filter = this;
  dlg.old_param = param;
  return dlg.Show((HWND)hwnd);
}

uint32 LogoFilter::GetParams() {
  kPixFormat_XRGB64 = 0;
  if(fma && fma->fmpixmap) kPixFormat_XRGB64 = fma->fmpixmap->GetFormat_XRGB64();

  if (sAPIVersion >= 12) {
    switch(fa->src.mpPixmapLayout->format) {
    case nsVDXPixmap::kPixFormat_XRGB8888:
      break;

    default:
      //if(fa->src.mpPixmapLayout->format==kPixFormat_XRGB64)
      //  break;
      return FILTERPARAM_NOT_SUPPORTED;
    }
  }

  fa->dst.offset = 0;
  fa->dst.mpPixmapLayout->pitch = fa->src.mpPixmapLayout->pitch;
  return FILTERPARAM_ALIGN_SCANLINES | FILTERPARAM_SUPPORTS_ALTFORMATS;
}

void LogoFilter::Start()
{
  if(file_dirty) update_file();
}

int64 LogoFilter::currentFrame(int64 r)
{
  int64 frame = fa->pfsi->lCurrentSourceFrame;
  if(r!=-1) frame = r;
  if(fma && fma->fmtimeline){
    frame = fma->fmtimeline->FilterSourceToTimeline(frame);
  }
  return frame;
}

int LogoFilter::currentRFrame(int64 r)
{
  int64 rframe = param.refFrame;
  if(param.animMode==LogoParam::anim_follow){
    rframe = int64(floor((currentFrame(r) - param.refFrame)*param.rate + 0.5));
  }

  if(param.loopMode==LogoParam::loop_saw){
    if(rframe>=0){
      rframe = rframe % frame_count;
    } else {
      rframe = frame_count-((-rframe-1) % frame_count)-1;
    }
  }

  return int(rframe);
}

void LogoFilter::Run()
{
  if(sAPIVersion<12) return;
  if(!video) return;

  int rframe = currentRFrame();
  if(rframe<0) return;
  if(rframe>=frame_count) return;

  if(rframe!=last_frame){
    last_frame = rframe;
    char data[32];
    uint32 bytesRead;
    uint32 samplesRead;
    video->Read(rframe,1,data,sizeof(data),&bytesRead,&samplesRead);
    video->DecodeFrame(0,0,false,rframe,rframe);
  }

  if(fa->src.mpPixmapLayout->format==kPixFormat_XRGB64){
    //render_xrgb64();
    return;
  }

  render_xrgb8();
}

//-------------------------------------------------------------------------------------------------

VDXFilterDefinition2 filterDef_fflayer = VDXVideoFilterDefinition<LogoFilter>("Anton", "fflayer", "Overlay image/video (ffmpeg)");
VDXVF_BEGIN_SCRIPT_METHODS(LogoFilter)
VDXVF_DEFINE_SCRIPT_METHOD(LogoFilter, ScriptConfig, "iiisiid")
VDXVF_END_SCRIPT_METHODS()

