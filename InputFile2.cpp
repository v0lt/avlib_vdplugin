#include "InputFile2.h"
#include "FileInfo2.h"
#include "VideoSource2.h"
#include "AudioSource2.h"
#include "cineform.h"
#include "mov_mp4.h"
#include "export.h"
#include <windows.h>
#include <vfw.h>
#include <aviriff.h>
#include "resource.h"

typedef struct AVCodecTag {
  enum AVCodecID id;
  unsigned int tag;
} AVCodecTag;

void init_av();

extern bool config_decode_raw;
extern bool config_decode_magic;
extern bool config_decode_cfhd;
extern bool config_disable_cache;

void widechar_to_utf8(char *dst, int max_dst, const wchar_t *src)
{
  *dst = 0;
  WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, max_dst, 0, 0);
}

void utf8_to_widechar(wchar_t *dst, int max_dst, const char *src)
{
  *dst = 0;
  MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, max_dst);
}

bool FileExist(const wchar_t* name)
{
  DWORD a = GetFileAttributesW(name);
  return ((a!=0xFFFFFFFF) && !(a & FILE_ATTRIBUTE_DIRECTORY));
}

bool check_magic_vfw(DWORD fcc)
{
  if(config_decode_magic) return false;
  HIC codec = ICOpen(ICTYPE_VIDEO, fcc, ICMODE_DECOMPRESS);
  ICClose(codec);
  if(!codec) return false;
  return true;
}

DWORD fcc_toupper(DWORD a)
{
  DWORD r = a;
  char* c = (char*)(&r);
  {for(int i=0; i<4; i++){
    int v = c[i];
    if(v>='a' && v<='z') c[i] = v+'A'-'a';
  }}
  return r;
}

int detect_avi(VDXMediaInfo& info, const void *pHeader, int32_t nHeaderSize)
{
  if(nHeaderSize<64) return -1;
  uint8_t* data = (uint8_t*)pHeader;
  int rsize = nHeaderSize;

  RIFFCHUNK ch;
  memcpy(&ch,data,sizeof(ch)); data+=sizeof(ch); rsize-=sizeof(ch);
  if(ch.fcc!=0x46464952) return -1; //RIFF
  DWORD fmt;
  memcpy(&fmt,data,4); data+=4; rsize-=4;
  if(fmt!=0x20495641) return -1; //AVI
  memcpy(&ch,data,sizeof(ch)); data+=sizeof(ch); rsize-=sizeof(ch);
  if(ch.fcc!=0x5453494C) return -1; //LIST
  memcpy(&fmt,data,4); data+=4; rsize-=4;
  if(fmt!=0x6C726468) return -1; //hdrl

  memcpy(&ch,data,sizeof(ch));
  if(ch.fcc!=ckidMAINAVIHEADER) return -1; //avih

  if(rsize<sizeof(AVIMAINHEADER)){
    AVIMAINHEADER mh = {0};
    memcpy(&mh,data,rsize); data+=rsize; rsize=0;
    return 1;
  }

  AVIMAINHEADER mh;
  memcpy(&mh,data,sizeof(mh)); data+=sizeof(mh); rsize-=sizeof(mh);
  info.width = mh.dwWidth;
  info.height = mh.dwHeight;
  wcscpy(info.format_name,L"AVI");

  if(rsize<sizeof(ch)) return 1;
  memcpy(&ch,data,sizeof(ch)); data+=sizeof(ch); rsize-=sizeof(ch);
  if(ch.fcc!=0x5453494C) return -1; //LIST

  if(rsize<sizeof(fmt)) return 1;
  memcpy(&fmt,data,4); data+=4; rsize-=4;
  if(fmt!=ckidSTREAMLIST) return -1; //strl

  if(rsize<sizeof(AVISTREAMHEADER)) return 1;
  AVISTREAMHEADER sh;
  memcpy(&sh,data,sizeof(sh)); data+=sizeof(sh); rsize-=sizeof(sh);
  if(sh.fcc!=ckidSTREAMHEADER) return -1; //strh

  // reject if there is unsupported video codec
  if(sh.fccType==streamtypeVIDEO){
    init_av();
    bool have_codec = false;
    DWORD h1 = sh.fccHandler;
    char* ch = (char*)(&h1);
    info.vcodec_name[0] = 0;
    {for(int i=0; i<4; i++){
      int v = ch[i];
      if(v>=32){
        wchar_t vc[] = {0,0};
        vc[0] = v;
        wcscat(info.vcodec_name,vc);
      } else {
        wchar_t vc[10];
        swprintf(vc,10,L"[%d]",v);
        wcscat(info.vcodec_name,vc);
      }
    }}

    // skip internally supported formats
    if(!config_decode_raw){
      if(h1==MKTAG('U', 'Y', 'V', 'Y')) return 0;
      if(h1==MKTAG('Y', 'U', 'Y', 'V')) return 0;
      if(h1==MKTAG('Y', 'U', 'Y', '2')) return 0;
      if(h1==MKTAG('Y', 'V', '2', '4')) return 0;
      if(h1==MKTAG('Y', 'V', '1', '6')) return 0;
      if(h1==MKTAG('Y', 'V', '1', '2')) return 0;
      if(h1==MKTAG('I', '4', '2', '0')) return 0;
      if(h1==MKTAG('I', 'Y', 'U', 'V')) return 0;
      if(h1==MKTAG('Y', 'V', 'U', '9')) return 0;
      if(h1==MKTAG('Y', '8', ' ', ' ')) return 0;
      if(h1==MKTAG('Y', '8', '0', '0')) return 0;
      if(h1==MKTAG('H', 'D', 'Y', 'C')) return 0;
      if(h1==MKTAG('N', 'V', '1', '2')) return 0;
      if(h1==MKTAG('v', '3', '0', '8')) return 0;

      if(h1==MKTAG('v', '2', '1', '0')) return 0;
      if(h1==MKTAG('b', '6', '4', 'a')) return 0;
      if(h1==MKTAG('B', 'R', 'A', 64)) return 0;
      if(h1==MKTAG('b', '4', '8', 'r')) return 0;
      if(h1==MKTAG('P', '0', '1', '0')) return 0;
      if(h1==MKTAG('P', '0', '1', '6')) return 0;
      if(h1==MKTAG('P', '2', '1', '0')) return 0;
      if(h1==MKTAG('P', '2', '1', '6')) return 0;
      if(h1==MKTAG('r', '2', '1', '0')) return 0;
      if(h1==MKTAG('R', '1', '0', 'k')) return 0;
      if(h1==MKTAG('v', '4', '1', '0')) return 0;
      if(h1==MKTAG('Y', '4', '1', '0')) return 0;
      if(h1==MKTAG('Y', '4', '1', '6')) return 0;
    }

    if(!have_codec){
      AVCodecTag* riff_tag = (AVCodecTag*)avformat_get_riff_video_tags();
      while(riff_tag->id!=AV_CODEC_ID_NONE){
        if(riff_tag->tag==h1 || fcc_toupper(riff_tag->tag)==fcc_toupper(h1)){
          if(riff_tag->id==AV_CODEC_ID_MAGICYUV && check_magic_vfw(h1)) return 0;
          have_codec = true;
          break;
        }
        riff_tag++;
      }
    }

    if(!have_codec){
      AVCodecTag* mov_tag = (AVCodecTag*)avformat_get_mov_video_tags();
      while(mov_tag->id!=AV_CODEC_ID_NONE){
        if(mov_tag->tag==h1 || fcc_toupper(mov_tag->tag)==fcc_toupper(h1)){
          if(mov_tag->id==AV_CODEC_ID_MAGICYUV && check_magic_vfw(h1)) return 0;
          have_codec = true;
          break;
        }
        mov_tag++;
      }
    }

    if(!have_codec) return 0;
  }

  return 1;
}

int detect_mp4_mov(const void *pHeader, int32_t nHeaderSize, int64_t nFileSize)
{
  MovParser parser(pHeader,nHeaderSize,nFileSize);

  MovAtom a;
  if(!parser.read(a)) return -1;
  if(a.sz>nFileSize && nFileSize>0) return -1;

  switch(a.t){
  case 'wide':
  case 'ftyp':
  case 'skip':
  case 'mdat':
  case 'moov':
    return 1;
  }

  return -1;
}

int detect_ff(VDXMediaInfo& info, const void *pHeader, int32_t nHeaderSize, const wchar_t* fileName)
{
  init_av();

  IOBuffer buf;
  buf.copy(pHeader,nHeaderSize);
  AVProbeData pd = {0};
  pd.buf = buf.data;
  pd.buf_size = (int)buf.size;

  int score=0;
  AVInputFormat* fmt = av_probe_input_format3(&pd,true,&score);
  if(fmt){
    for(int i=0; i<100; i++){
      int c = fmt->name[i];
      info.format_name[i] = c;
      if(c==0) break;
    }
    info.format_name[99] = 0;
  }
  // some examples
  // mpegts is detected with score 2, can be confused with arbitrary text file
  // ac3 is not detected, but score is raised to 1 (not 0)
  // tga is not detected, score is 0
  // also tga is detected as mp3 with score 1
   
  if(score==AVPROBE_SCORE_MAX) return 1;

  if(!fileName){
    if(score>0) return 0;
    return -1;
  }

  const int ff_path_size = MAX_PATH*4; // utf8, worst case
  char ff_path[ff_path_size];
  widechar_to_utf8(ff_path, ff_path_size, fileName);

  AVFormatContext* ctx = 0;
  int err = 0; 
  err = avformat_open_input(&ctx, ff_path, 0, 0);
  if(err!=0) return -1;
  err = avformat_find_stream_info(ctx, 0);
  if(err<0){
    avformat_close_input(&ctx);
    return -1;
  }
  fmt = ctx->iformat;
  {for(int i=0; i<100; i++){
    int c = fmt->name[i];
    info.format_name[i] = c;
    if(c==0) break;
  }}
  info.format_name[99] = 0;
  avformat_close_input(&ctx);

  return 1;
}

//----------------------------------------------------------------------------------------------

VDFFInputFileDriver::VDFFInputFileDriver(const VDXInputDriverContext& context)
: mContext(context)
{
}

VDFFInputFileDriver::~VDFFInputFileDriver()
{
}

int VDXAPIENTRY VDFFInputFileDriver::DetectBySignature(const void *pHeader, int32_t nHeaderSize, const void *pFooter, int32_t nFooterSize, int64_t nFileSize)
{
  return -1;
}

int VDXAPIENTRY VDFFInputFileDriver::DetectBySignature2(VDXMediaInfo& info, const void *pHeader, int32_t nHeaderSize, const void *pFooter, int32_t nFooterSize, int64_t nFileSize)
{
  return DetectBySignature3(info,pHeader,nHeaderSize,pFooter,nFooterSize,nFileSize,0);
}

int VDXAPIENTRY VDFFInputFileDriver::DetectBySignature3(VDXMediaInfo& info, const void *pHeader, sint32 nHeaderSize, const void *pFooter, sint32 nFooterSize, sint64 nFileSize, const wchar_t* fileName)
{
  int avi_q = detect_avi(info,pHeader,nHeaderSize);

  if(avi_q==1) return kDC_High;
  if(avi_q==0) return kDC_Moderate;

  if(detect_mp4_mov(pHeader,nHeaderSize,nFileSize)==1){
    wcscpy(info.format_name,L"iso media");
    return kDC_High;
  }

  int ff_q = detect_ff(info,pHeader,nHeaderSize,fileName);
  if(ff_q==1) return kDC_Moderate;
  if(ff_q==0) return kDC_VeryLow;

  return kDC_None;
}

bool VDXAPIENTRY VDFFInputFileDriver::CreateInputFile(uint32_t flags, IVDXInputFile **ppFile)
{
  VDFFInputFile* p = new VDFFInputFile(mContext);
  if(!p) return false;

  if(flags & kOF_AutoSegmentScan) p->auto_append = true;
  if(flags & kOF_SingleFile) p->single_file_mode = true;
  //p->auto_append = true;
  if(config_decode_cfhd) p->cfg_skip_cfhd = true;
  if(config_disable_cache) p->cfg_disable_cache = true;

  *ppFile = p;
  p->AddRef();
  return true;
}

//----------------------------------------------------------------------------------------------

extern HINSTANCE hInstance;
extern bool VDXAPIENTRY StaticConfigureProc(VDXHWND parent);

class FileConfigureDialog: public VDXVideoFilterDialog {
public:
  VDFFInputFileOptions::Data* data;

	virtual INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
  void Show(HWND parent){
    VDXVideoFilterDialog::Show(hInstance,MAKEINTRESOURCE(IDD_INPUT_OPTIONS),parent);
  }
};

INT_PTR FileConfigureDialog::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch(msg){
  case WM_INITDIALOG:
    CheckDlgButton(mhdlg, IDC_DECODE_CFHD, !data->skip_native_cfhd ? BST_CHECKED:BST_UNCHECKED);
    CheckDlgButton(mhdlg, IDC_DECODE_CFHD_AM, !data->skip_cfhd_am ? BST_CHECKED:BST_UNCHECKED);
    EnableWindow(GetDlgItem(mhdlg, IDC_DECODE_CFHD_AM), !data->skip_native_cfhd);
    CheckDlgButton(mhdlg, IDC_DISABLE_CACHE, data->disable_cache ? BST_CHECKED:BST_UNCHECKED);
    return TRUE;
  case WM_COMMAND:
    switch(LOWORD(wParam)){
    case IDC_SYS_OPTIONS:
      StaticConfigureProc((VDXHWND)mhdlg);
      return true;

    case IDC_DECODE_CFHD:
      data->skip_native_cfhd = !IsDlgButtonChecked(mhdlg,IDC_DECODE_CFHD)!=0;
      EnableWindow(GetDlgItem(mhdlg, IDC_DECODE_CFHD_AM), !data->skip_native_cfhd);
      return TRUE;

    case IDC_DECODE_CFHD_AM:
      data->skip_cfhd_am = !IsDlgButtonChecked(mhdlg,IDC_DECODE_CFHD_AM)!=0;
      return TRUE;

    case IDC_DISABLE_CACHE:
      data->disable_cache = IsDlgButtonChecked(mhdlg,IDC_DISABLE_CACHE)!=0;
      return TRUE;

    case IDOK:
      EndDialog(mhdlg, TRUE);
      return TRUE;

    case IDCANCEL:
      EndDialog(mhdlg, FALSE);
      return TRUE;
    }
  }
  return false;
}

bool VDXAPIENTRY VDFFInputFile::PromptForOptions(VDXHWND parent, IVDXInputOptions **r)
{
  VDFFInputFileOptions* opt = new VDFFInputFileOptions;
  if(cfg_skip_cfhd) opt->data.skip_native_cfhd = true;
  if(cfg_disable_cache) opt->data.disable_cache = true;

  opt->AddRef();
  *r = opt;
  FileConfigureDialog dlg;
  dlg.data = &opt->data;
  dlg.Show((HWND)parent);
  return true;
}

bool VDXAPIENTRY VDFFInputFile::CreateOptions(const void *buf, uint32_t len, IVDXInputOptions **r)
{
  VDFFInputFileOptions* opt = new VDFFInputFileOptions;
  if(opt->Read(buf,len)){
    opt->AddRef();
    *r = opt;
    return true;
  }

  delete opt;
  *r=0;
  return false;
}

//----------------------------------------------------------------------------------------------

VDFFInputFile::VDFFInputFile(const VDXInputDriverContext& context)
  :mContext(context)
{
  m_pFormatCtx = 0;
  video_start_time = 0;
  video_source = 0;
  audio_source = 0;
  next_segment = 0;
  head_segment = 0;
  auto_append = false;
  single_file_mode = false;
  is_image = false;
  is_image_list = false;
  is_anim_image = false;

  cfg_frame_buffers = 0;
  cfg_skip_cfhd = false;
  cfg_skip_cfhd_am = false;
  cfg_disable_cache = false;
}

VDFFInputFile::~VDFFInputFile()
{
  if(next_segment) next_segment->Release();
  if(video_source) video_source->Release();
  if(audio_source) audio_source->Release();
  if(m_pFormatCtx) avformat_close_input(&m_pFormatCtx);	
}

void VDFFInputFile::DisplayInfo(VDXHWND hwndParent) 
{
  VDFFInputFileInfoDialog dlg;
  dlg.Show(hwndParent, this);
}

void VDFFInputFile::Init(const wchar_t *szFile, IVDXInputOptions *in_opts) 
{
  if(!szFile){
    mContext.mpCallbacks->SetError("No File Given");
    return;
  }

  if(in_opts){
    VDFFInputFileOptions* opt = (VDFFInputFileOptions*)in_opts;
    if(opt->data.skip_native_cfhd) cfg_skip_cfhd = true;
    if(opt->data.skip_cfhd_am) cfg_skip_cfhd_am = true;
    if(opt->data.disable_cache) cfg_disable_cache = true;
  }

  if(cfg_frame_buffers<1) cfg_frame_buffers = 1;

  wcscpy(path,szFile);

  init_av();
  //! this context instance is granted to video stream: wasted in audio-only mode
  // audio will manage its own
  m_pFormatCtx = open_file(AVMEDIA_TYPE_VIDEO);

  if(auto_append) do_auto_append(szFile);
}

int VDFFInputFile::GetFileFlags()
{
  int flags = VDFFInputFileDriver::kFF_AppendSequence;
  if(is_image_list) flags |= VDFFInputFileDriver::kFF_Sequence;
  return flags;
}

void VDFFInputFile::do_auto_append(const wchar_t *szFile)
{
  const wchar_t* ext = wcsrchr(szFile,'.');
  if(!ext) return;
  if(ext-szFile<3) return;
  if(ext[-3]=='.' && ext[-2]=='0' && ext[-1]=='0'){
    int x = 1;
    while(1){
      wchar_t buf[MAX_PATH+128];
      wcsncpy(buf,szFile,ext-szFile-3); buf[ext-szFile-3]=0;
      wchar_t buf1[32];
      swprintf(buf1,32,L".%02d",x);
      wcscat(buf,buf1);
      wcscat(buf,ext);
      if(!FileExist(buf)) break;
      if(!Append(buf)) break;
      x++;
    }
  }
}

bool VDFFInputFile::test_append(VDFFInputFile* f0, VDFFInputFile* f1)
{
  int s0 = f0->find_stream(f0->m_pFormatCtx,AVMEDIA_TYPE_VIDEO);
  int s1 = f1->find_stream(f1->m_pFormatCtx,AVMEDIA_TYPE_VIDEO);
  if(s0==-1 || s1==-1) return false;
  AVStream* v0 = f0->m_pFormatCtx->streams[s0];
  AVStream* v1 = f1->m_pFormatCtx->streams[s1];
  if(v0->codecpar->width != v1->codecpar->width) return false;
  if(v0->codecpar->height != v1->codecpar->height) return false;

  s0 = f0->find_stream(f0->m_pFormatCtx,AVMEDIA_TYPE_AUDIO);
  s1 = f1->find_stream(f1->m_pFormatCtx,AVMEDIA_TYPE_AUDIO);
  if(s0==-1 || s1==-1) return true;
  AVStream* a0 = f0->m_pFormatCtx->streams[s0];
  AVStream* a1 = f1->m_pFormatCtx->streams[s1];
  if(a0->codecpar->sample_rate!=a1->codecpar->sample_rate) return false;

  return true;
}

bool VDXAPIENTRY VDFFInputFile::Append(const wchar_t* szFile)
{
  if(!szFile) return true;

  return Append2(szFile, VDFFInputFileDriver::kOF_SingleFile, 0);
}

bool VDXAPIENTRY VDFFInputFile::Append2(const wchar_t *szFile, int flags, IVDXInputOptions *opts)
{
  if(!szFile) return true;

  VDFFInputFile* head = head_segment ? head_segment : this;
  VDFFInputFile* last = head;
  while(last->next_segment) last = last->next_segment;

  VDFFInputFile* f = new VDFFInputFile(mContext);
  f->head_segment = head;
  if(flags & VDFFInputFileDriver::kOF_AutoSegmentScan) f->auto_append = true; else f->auto_append = false;
  if(flags & VDFFInputFileDriver::kOF_SingleFile) f->single_file_mode = true; else f->single_file_mode = false;
  f->Init(szFile,0);

  if(!f->m_pFormatCtx){
    delete f;
    return false;
  }

  if(!test_append(head,f)){
    mContext.mpCallbacks->SetError("FFMPEG: Couldn't append incompatible formats.");
    delete f;
    return false;
  }

  last->next_segment = f;
  last->next_segment->AddRef();

  if(head->video_source){
    if(f->GetVideoSource(0,0)){
      VDFFVideoSource::ConvertInfo& convertInfo = head->video_source->convertInfo;
      f->video_source->SetTargetFormat(convertInfo.req_format,convertInfo.req_dib,head->video_source);
      VDFFInputFile* f1 = head;
      while(1){
        f1->video_source->m_streamInfo.mInfo.mSampleCount += f->video_source->sample_count;
        f1 = f1->next_segment;
        if(!f1) break;
        if(f1==f) break;
      }
    } else {
      last->next_segment = 0;
      f->Release();
      return false;
    }
  }

  if(head->audio_source){
    if(f->GetAudioSource(0,0)){
      VDFFInputFile* f1 = head;
      while(1){
        f1->audio_source->m_streamInfo.mSampleCount += f->audio_source->sample_count;
        f1 = f1->next_segment;
        if(!f1) break;
        if(f1==f) break;
      }
    } else {
      // no audio is allowed
      /*
      last->next_segment = 0;
      f->Release();
      return false;
      */
    }
  }

  return true;
}

AVFormatContext* VDFFInputFile::open_file(AVMediaType type, int streamIndex)
{
  const int ff_path_size = MAX_PATH*4; // utf8, worst case
  char ff_path[ff_path_size];
  widechar_to_utf8(ff_path, ff_path_size, path);

  AVFormatContext* fmt = 0;
  int err = 0; 
  err = avformat_open_input(&fmt, ff_path, 0, 0);
  if(err!=0){
    mContext.mpCallbacks->SetError("FFMPEG: Unable to open file.");
    return 0;
  }

  if(type==AVMEDIA_TYPE_VIDEO){
    // I absolutely do not want index getting condensed
    fmt->max_index_size = 512 * 1024 * 1024;
  }

  err = avformat_find_stream_info(fmt, 0);
  if(err<0){
    mContext.mpCallbacks->SetError("FFMPEG: Couldn't find stream information of file.");
    return 0;
  }

  is_image = false;
  is_image_list = false;
  is_anim_image = false;
  if(strcmp(fmt->iformat->name,"image2")==0) is_image=true;
  if(strcmp(fmt->iformat->name,"bmp_pipe")==0) is_image=true;
  if(strcmp(fmt->iformat->name,"dds_pipe")==0) is_image=true;
  if(strcmp(fmt->iformat->name,"dpx_pipe")==0) is_image=true;
  if(strcmp(fmt->iformat->name,"exr_pipe")==0) is_image=true;
  if(strcmp(fmt->iformat->name,"j2k_pipe")==0) is_image=true;
  if(strcmp(fmt->iformat->name,"jpeg_pipe")==0) is_image=true;
  if(strcmp(fmt->iformat->name,"jpegls_pipe")==0) is_image=true;
  if(strcmp(fmt->iformat->name,"pam_pipe")==0) is_image=true;
  if(strcmp(fmt->iformat->name,"pbm_pipe")==0) is_image=true;
  if(strcmp(fmt->iformat->name,"pcx_pipe")==0) is_image=true;
  if(strcmp(fmt->iformat->name,"pgmyuv_pipe")==0) is_image=true;
  if(strcmp(fmt->iformat->name,"pgm_pipe")==0) is_image=true;
  if(strcmp(fmt->iformat->name,"pictor_pipe")==0) is_image=true;
  if(strcmp(fmt->iformat->name,"png_pipe")==0) is_image=true;
  if(strcmp(fmt->iformat->name,"ppm_pipe")==0) is_image=true;
  if(strcmp(fmt->iformat->name,"psd_pipe")==0) is_image=true;
  if(strcmp(fmt->iformat->name,"qdraw_pipe")==0) is_image=true;
  if(strcmp(fmt->iformat->name,"sgi_pipe")==0) is_image=true;
  if(strcmp(fmt->iformat->name,"sunrast_pipe")==0) is_image=true;
  if(strcmp(fmt->iformat->name,"tiff_pipe")==0) is_image=true;
  if(strcmp(fmt->iformat->name,"webp_pipe")==0) is_image=true;
  if(strcmp(fmt->iformat->name,"apng")==0){ is_image=true; single_file_mode=true; }
  if(strcmp(fmt->iformat->name,"gif")==0){ is_image=true; single_file_mode=true; }
  if(strcmp(fmt->iformat->name,"fits")==0){ is_image=true; single_file_mode=true; }

  AVInputFormat* fmt_image2 = av_find_input_format("image2");

  if(is_image && fmt_image2 && !single_file_mode){
    AVRational r_fr = av_stream_get_r_frame_rate(fmt->streams[0]);
    wchar_t list_path[MAX_PATH];
    int start,count;
    if(detect_image_list(list_path,MAX_PATH,&start,&count)){
      is_image_list = true;
      auto_append = false;
      avformat_close_input(&fmt);
      widechar_to_utf8(ff_path, ff_path_size, list_path);
      AVDictionary* options = 0;
      av_dict_set_int(&options, "start_number", start, 0);
      if(r_fr.num!=0){
        char buf[80];
        sprintf(buf,"%d/%d",r_fr.num,r_fr.den);
        av_dict_set(&options, "framerate", buf, 0);
      }
      err = avformat_open_input(&fmt, ff_path, fmt_image2, &options);
      av_dict_free(&options);
      if(err!=0){
        mContext.mpCallbacks->SetError("FFMPEG: Unable to open image sequence.");
        return 0;
      }
      err = avformat_find_stream_info(fmt, 0);
      if(err<0){
        mContext.mpCallbacks->SetError("FFMPEG: Couldn't find stream information of file.");
        return 0;
      }

      AVStream& st = *fmt->streams[0];
      st.nb_frames = count;
    }
  }

  int st = streamIndex;
  if(st==-1) st = find_stream(fmt,type);
  if(st!=-1){
    // disable unwanted streams
    bool is_avi = strcmp(fmt->iformat->name,"avi")==0;
    {for(int i=0; i<(int)fmt->nb_streams; i++){
      if(i==st) continue;
      // this has minor impact on performance, ~10% for video
      fmt->streams[i]->discard = AVDISCARD_ALL;

      // this is HUGE if streams are not evenly interleaved (like VD does by default)
      // this fix is hack, I dont know if it will work for other format
      if(is_avi) fmt->streams[i]->nb_index_entries = 0;
    }}
  }

  return fmt;
}

bool VDFFInputFile::detect_image_list(wchar_t* dst, int dst_count, int* start, int* count)
{
  wchar_t* p = wcsrchr(path,'.');
  if(!p) return false;

  int digit0 = -1;
  int digit1 = -1;

  while(p>path){
    p--;
    int c = *p;
    if(c=='\\' || c=='/') break;
    if(c>='0' && c<='9'){
      if(digit0==-1){
        digit0 = p-path;
        digit1 = digit0;
      } else digit0--;
    } else if(digit0!=-1) break;
  }

  if(digit0==-1) return false;

  char start_buf[128];
  char* s = start_buf;
  {for(int i=digit0; i<=digit1; i++){
    int c = path[i];
    *s = c;
    s++;
  }}
  *s = 0;

  wchar_t buf[20];
  swprintf(buf,20,L"%%0%dd",digit1-digit0+1);

  wcsncpy(dst,path,digit0);
  dst[digit0] = 0;
  wcscat(dst,buf);
  wcscat(dst,path+digit1+1);

  sscanf(start_buf,"%d",start);
  int n = 0;

  wchar_t test[MAX_PATH];
  while(1){
    swprintf(test,MAX_PATH,dst,*start+n);
    if(GetFileAttributesW(test)==(DWORD)-1) break;
	  n++;
  }

  *count = n;

  return true;
}

int VDFFInputFile::find_stream(AVFormatContext* fmt, AVMediaType type)
{
  int video = -1;
  int r0 = av_find_best_stream(fmt,AVMEDIA_TYPE_VIDEO,-1,-1,0,0);
  if(r0>=0) video = r0;
  if(type==AVMEDIA_TYPE_VIDEO) return video;

  int r1 = av_find_best_stream(fmt,type,-1,video,0,0);
  if(r1>=0) return r1;
  return -1;
}

bool VDFFInputFile::GetVideoSource(int index, IVDXVideoSource **ppVS) 
{
  if(ppVS) *ppVS = 0;

  if(!m_pFormatCtx) return false;
  if(index!=0) return false;
 
  index = find_stream(m_pFormatCtx, AVMEDIA_TYPE_VIDEO);

  if(index < 0) return false;

  VDFFVideoSource *pVS = new VDFFVideoSource(mContext);

  if(!pVS) return false;

  if(pVS->initStream(this, index) < 0){
    delete pVS;
    return false;
  }

  video_source = pVS;
  video_source->AddRef();

  if(ppVS){
    *ppVS = pVS;
    pVS->AddRef();
  }

  if(next_segment && next_segment->GetVideoSource(0,0)){
    video_source->m_streamInfo.mInfo.mSampleCount += next_segment->video_source->m_streamInfo.mInfo.mSampleCount;
  }

  return true;
} 

bool VDFFInputFile::GetAudioSource(int index, IVDXAudioSource **ppAS)
{
  if(ppAS) *ppAS = 0;

  if(!m_pFormatCtx) return false;

  int s_index = find_stream(m_pFormatCtx, AVMEDIA_TYPE_AUDIO);
  if(index>0){
    int n = 0;
    {for(int i=0; i<(int)m_pFormatCtx->nb_streams; i++){
      AVStream* st = m_pFormatCtx->streams[i];
      if(i==s_index) continue;
      if(st->codecpar->codec_type!=AVMEDIA_TYPE_AUDIO) continue;
      if(!st->codecpar->channels) continue;
      if(!st->codecpar->sample_rate) continue;
      n++;
      if(n==index){ s_index=i; break; }
    }}
    if(n!=index) return false;
  }

  if(s_index < 0) return false;
  if(m_pFormatCtx->streams[s_index]->codecpar->codec_id==AV_CODEC_ID_NONE) return false;

  VDFFAudioSource *pAS = new VDFFAudioSource(mContext);

  if(!pAS) return false;

  if(pAS->initStream(this, s_index) < 0){
    delete pAS;
    return false;
  }

  if(ppAS){
    *ppAS = pAS;
    pAS->AddRef();
  }

  if(index==0 && !audio_source){
    audio_source = pAS;
    audio_source->AddRef();
  }
    
  // delete unused
  pAS->AddRef();
  pAS->Release();

  if(index==0){
    if(next_segment && next_segment->GetAudioSource(0,0)){
      pAS->m_streamInfo.mSampleCount += next_segment->audio_source->m_streamInfo.mSampleCount;
    }
  }

  return true;
}

int seek_frame(AVFormatContext *s, int stream_index, int64_t timestamp, int flags)
{
  int r = av_seek_frame(s,stream_index,timestamp,flags);
  if(r==-1 && timestamp==AV_SEEK_START){
    AVStream* st = s->streams[stream_index];
    if(st->nb_index_entries) timestamp = st->index_entries[0].timestamp;
    r = av_seek_frame(s,stream_index,timestamp,flags);
  }
  return r;
}