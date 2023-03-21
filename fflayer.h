#ifndef fflayer_header
#define fflayer_header

#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/VDXFrame/VideoFilterDialog.h>
#include "InputFile2.h"
#include "VideoSource2.h"

struct LogoParam {
  wchar_t path[MAX_PATH];

  int	pos_x, pos_y;

  enum {
    blend_replace = 0,
    blend_alpha = 1,
    blend_alpha_pm = 2,

    anim_single = 0,
    anim_follow = 1,
    loop_saw = 2,
  };

  int blendMode;
  int animMode;
  int loopMode;
  int refFrame;
  double rate;

  LogoParam(){
    pos_x = 0;
    pos_y = 0;
    blendMode = 0;
    animMode = 0;
    loopMode = 0;
    refFrame = 0;
    rate = 1;
    path[0] = 0;
  }
};

class LogoFilter;

class LogoDialog: public VDXVideoFilterDialog{
public:
  LogoFilter* filter;
  LogoParam old_param;
  IVDXFilterPreview* const ifp;

  LogoDialog(IVDXFilterPreview* ifp): ifp(ifp){}
  bool Show(HWND parent);
  virtual INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
  void init_edit(int id);
  void init_path();
  void init_pos();
  void init_rate();
  void init_ref(int64 r=-1);
  void init_buttons();
  bool modify_value(HWND item, int& v);
  bool modify_value(HWND item, double& v);
  bool modify_path();
  bool OnCommand(uint32 id, uint32 extcode);
  void redo_frame();
};

class LogoFilter : public VDXVideoFilter {
public:

  class Callbacks: public IVDXPluginCallbacks {
    virtual void * VDXAPIENTRY GetExtendedAPI(const char *pExtendedAPIName){ return 0; }
    virtual void VDXAPIENTRYV SetError(const char *format, ...){}
    virtual void VDXAPIENTRY SetErrorOutOfMemory(){}
    virtual uint32 VDXAPIENTRY GetCPUFeatureFlags(){ return 0; }
  } callbacks;

  LogoParam param;
  int64 kPixFormat_XRGB64;

  VDXInputDriverContext context;
  VDFFInputFile* file;
  VDFFVideoSource* video;
  int last_frame;
  int frame_count;
  bool file_dirty;

  LogoFilter(){ init(); }
  LogoFilter(const LogoFilter& a);
  ~LogoFilter(){ clear(); }
  void init();
  void clear();
  void update_file();
  virtual uint32 GetParams();
  virtual void Start();
  virtual void Run();
  virtual bool Configure(VDXHWND hwnd);
  virtual void GetSettingString(char* buf, int maxlen);
  virtual void GetScriptString(char* buf, int maxlen);
  VDXVF_DECLARE_SCRIPT_METHODS();
  void ScriptConfig(IVDXScriptInterpreter* isi, const VDXScriptValue* argv, int argc);

  int64 currentFrame(int64 r=-1);
  int currentRFrame(int64 r=-1);
  void render_xrgb8();
  void render_xrgb64();
};

const UINT WM_EDIT_CHANGED = WM_USER+666;
LRESULT CALLBACK EditWndProc(HWND wnd,UINT msg,WPARAM wparam,LPARAM lparam);

#endif
