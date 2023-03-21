#include "cineform.h"
#include "compress.h"
#include <stdint.h>

#include "cineform/common/CFHDEncoder.h"
#include "cineform/common/ver.h"
#include "cineform/common/metadatatags.h"
#include "cineform/common/aviextendedheader.h"
#include "vd2\plugin\vdinputdriver.h"
#include <vd2/VDXFrame/VideoFilterDialog.h>
#include <commctrl.h>
#include "resource.h"

extern HINSTANCE hInstance;

CFHD_EncodingQuality quality_list[] = {
  CFHD_ENCODING_QUALITY_FILMSCAN3,
  CFHD_ENCODING_QUALITY_FILMSCAN2,
  CFHD_ENCODING_QUALITY_FILMSCAN1,
  CFHD_ENCODING_QUALITY_HIGH,
  CFHD_ENCODING_QUALITY_MEDIUM,
  CFHD_ENCODING_QUALITY_LOW,
};

const char* quality_names[] = {
  "Filmscan 3",
  "Filmscan 2",
  "Filmscan 1",
  "High",
  "Medium",
  "Low",
};

struct CodecCF;

class ConfigCF: public VDXVideoFilterDialog{
public:
  CodecCF* codec;
  void* old_param;

  ConfigCF()
  {
    codec = 0;
    old_param = 0;
  }

  virtual ~ConfigCF()
  {
    free(old_param);
  }

  void Show(HWND parent, CodecCF* codec);
  virtual INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
  virtual void init_format();
  virtual void change_format(int sel);
  virtual void change_bits(){}
  void init_bits();
  void adjust_bits();
};

struct CodecCF: public CodecClass{
  enum {
    format_rgb = 1,
    format_rgba = 2,
    format_yuv422 = 3,
    format_bayer_rg = 4,
    format_bayer_gr = 5,
    format_bayer_gb = 6,
    format_bayer_bg = 7,
  };

  struct Config{
    int version;
    int format;
    int bits;
    int quality;
    int threads;

    Config(){ clear(); }
    void clear(){
      version = 0;
      format = format_yuv422;
      bits = 8;
      quality = 2;
      threads = 0; // 0=auto, 1=off
    }
  } config;

  AVRational time_base;
  int keyint;
  LONG frame_total;
  bool use709;
  bool topdown;

  CFHD_EncoderRef encRef;
  CFHD_EncoderPoolRef poolRef;
  CFHD_MetadataRef metadataRef;
  uint32_t frameNumber;
  uint32_t pool_depth;

  struct Buffer{
    void* p;
    void* data;
    ptrdiff_t pitch;
    uint32_t frameNumber;
    int ref;
  };
  Buffer* buffer;
  int buffer_count;

  CodecCF(){
    class_id=1;
    keyint=1;
    encRef=0;
    poolRef=0;
    metadataRef=0;
    use709=true;
    buffer=0;
    buffer_count=0;
  }

  virtual ~CodecCF(){
    compress_end();
  }

  virtual void getinfo(ICINFO& info){
    info.fccHandler = CFHD_TAG;
    info.dwFlags = VIDCF_COMPRESSFRAMES;
    wcscpy(info.szName, L"CineForm");
    wcscpy(info.szDescription, L"GoPro CineForm (native)");
  }

  virtual int config_size(){ return sizeof(Config); }

  virtual void reset_config(){ config.clear(); }

  virtual bool load_config(void* data, size_t size){
    size_t rsize = config_size();
    if(size!=rsize) return false;
    int src_version = *(int*)data;
    if(src_version!=config.version) return false;
    memcpy(&config, data, rsize);
    return true;
  }

  virtual int compress_input_info(VDXPixmapLayout* src){
    switch(src->format){
    case nsVDXPixmap::kPixFormat_RGB888:
    case nsVDXPixmap::kPixFormat_XRGB8888:
    case nsVDXPixmap::kPixFormat_R210:
    case nsVDXPixmap::kPixFormat_XRGB64:
    case nsVDXPixmap::kPixFormat_YUV422_YUYV:
    case nsVDXPixmap::kPixFormat_YUV422_V210:
    case nsVDXPixmap::kPixFormat_YUV422_YU64:
    case nsVDXPixmap::kPixFormat_Y16:
      return 1;
    }
    return 0;
  }

  LRESULT compress_input_format(FilterModPixmapInfo* info){
    if(config.format==format_rgba){
      if(config.bits==8) return nsVDXPixmap::kPixFormat_XRGB8888;
      if(config.bits==16){
        if(info){
          int max_value = 0xFFF; // request rounding to 14 bits
          info->ref_r = max_value;
          info->ref_g = max_value;
          info->ref_b = max_value;
          info->ref_a = max_value;
        }
        return nsVDXPixmap::kPixFormat_XRGB64;
      }
    }

    if(config.format==format_rgb){
      if(config.bits==8) return nsVDXPixmap::kPixFormat_RGB888;
      if(config.bits==10) return nsVDXPixmap::kPixFormat_R210;
      if(config.bits==16){
        if(info){
          int max_value = 0xFFF; // request rounding to 14 bits
          info->ref_r = max_value;
          info->ref_g = max_value;
          info->ref_b = max_value;
          info->ref_a = max_value;
        }
        return nsVDXPixmap::kPixFormat_XRGB64;
      }
    }

    if(config.format==format_yuv422){
      if(config.bits==8) return nsVDXPixmap::kPixFormat_YUV422_YUYV;
      if(config.bits==10) return nsVDXPixmap::kPixFormat_YUV422_V210;
      if(config.bits==16){
        if(info) info->ref_r = 0xFFFF;
        return nsVDXPixmap::kPixFormat_YUV422_YU64;
      }
    }

    if(config.format>=format_bayer_rg){
      if(info) info->ref_r = 0xFFFF;
      return nsVDXPixmap::kPixFormat_Y16;
    }

    return 0;
  }

  LRESULT compress_get_format(BITMAPINFO *lpbiOutput, VDXPixmapLayout* layout)
  {
    BITMAPINFOHEADER *outhdr = &lpbiOutput->bmiHeader;

    if(!lpbiOutput) return sizeof(BITMAPINFOHEADER);

    if(layout->format!=compress_input_format(0)) return ICERR_BADFORMAT;
    int iWidth  = layout->w;
    int iHeight = layout->h;

    if(iWidth <= 0 || iHeight <= 0) return ICERR_BADFORMAT;

    memset(outhdr, 0, sizeof(BITMAPINFOHEADER));
    outhdr->biSize        = sizeof(BITMAPINFOHEADER);
    outhdr->biWidth       = iWidth;
    outhdr->biHeight      = iHeight;
    outhdr->biPlanes      = 1;
    outhdr->biBitCount    = 32;
    outhdr->biCompression = CFHD_TAG;
    outhdr->biSizeImage   = iWidth*iHeight*8;

    return ICERR_OK;
  }

  LRESULT compress_query(BITMAPINFO *lpbiOutput, VDXPixmapLayout* layout)
  {
    BITMAPINFOHEADER *outhdr = &lpbiOutput->bmiHeader;

    if(layout->format!=compress_input_format(0)) return ICERR_BADFORMAT;
    int iWidth  = layout->w;
    int iHeight = layout->h;

    if(iWidth <= 0 || iHeight <= 0) return ICERR_BADFORMAT;
    if((iWidth % 16)!=0) return ICERR_BADPARAM;

    if(!lpbiOutput) return ICERR_OK;

    if(iWidth != outhdr->biWidth || iHeight != outhdr->biHeight)
      return ICERR_BADFORMAT;

    if(outhdr->biCompression!=CFHD_TAG)
      return ICERR_BADFORMAT;

    return ICERR_OK;
  }

  LRESULT compress_get_size(BITMAPINFO *lpbiOutput)
  {
    return lpbiOutput->bmiHeader.biWidth*lpbiOutput->bmiHeader.biHeight*8;
  }

  LRESULT compress_frames_info(ICCOMPRESSFRAMES *icf)
  {
    frame_total = icf->lFrameCount;
    time_base.den = icf->dwRate;
    time_base.num = icf->dwScale;
    keyint = icf->lKeyRate;
    return ICERR_OK;
  }

  LRESULT compress_begin(BITMAPINFO *lpbiOutput, VDXPixmapLayout* layout)
  {
    if(compress_query(lpbiOutput, layout) != ICERR_OK){
      return ICERR_BADFORMAT;
    }

    CFHD_PixelFormat pixelFormat = CFHD_PIXEL_FORMAT_UNKNOWN;
    CFHD_EncodedFormat encodedFormat = CFHD_ENCODED_FORMAT_UNKNOWN;
    CFHD_EncodingFlags encodingFlags = CFHD_ENCODING_FLAGS_NONE;
    int channels = 0;
    int subw = layout->w;

    switch(config.format){
    case format_rgb:
      encodedFormat = CFHD_ENCODED_FORMAT_RGB_444;
      if(config.bits==8) pixelFormat = CFHD_PIXEL_FORMAT_RG24;
      if(config.bits==10) pixelFormat = CFHD_PIXEL_FORMAT_R210;
      if(config.bits==16) pixelFormat = CFHD_PIXEL_FORMAT_B64A;
      channels = 3;
      break;
    case format_rgba:
      encodedFormat = CFHD_ENCODED_FORMAT_RGBA_4444;
      if(config.bits==8) pixelFormat = CFHD_PIXEL_FORMAT_BGRA;
      if(config.bits==16) pixelFormat = CFHD_PIXEL_FORMAT_B64A;
      channels = 4;
      break;
    case format_yuv422:
      if(!use709) encodingFlags |= CFHD_ENCODING_FLAGS_YUV_601;
      encodedFormat = CFHD_ENCODED_FORMAT_YUV_422;
      if(config.bits==8) pixelFormat = CFHD_PIXEL_FORMAT_YUY2;
      if(config.bits==10) pixelFormat = CFHD_PIXEL_FORMAT_V210;
      if(config.bits==16) pixelFormat = CFHD_PIXEL_FORMAT_YU64;
      channels = 3;
      subw = (layout->w+1)/2;
      break;
    case format_bayer_rg:
    case format_bayer_gr:
    case format_bayer_bg:
    case format_bayer_gb:
      encodedFormat = CFHD_ENCODED_FORMAT_BAYER;
      pixelFormat = CFHD_PIXEL_FORMAT_BYR4;
      channels = 1;
      break;
    }

    topdown = true;
    switch(pixelFormat){
    case CFHD_PIXEL_FORMAT_RG24:
    case CFHD_PIXEL_FORMAT_BGRA:
      topdown = false;
    }

    CFHD_EncodingQuality quality = quality_list[config.quality];

    CFHD_MetadataOpen(&metadataRef);
    if(encodedFormat==CFHD_ENCODED_FORMAT_BAYER){
      uint32_t bayer_format = BAYER_FORMAT_RED_GRN + config.format - format_bayer_rg;
      CFHD_MetadataAdd(metadataRef, TAG_BAYER_FORMAT, METADATATYPE_UINT32, 4, &bayer_format, false);

      //CFHD_MetadataAdd(metadataRef, TAG_COLOR_MATRIX, METADATATYPE_FLOAT, 48, (uint32*)matrix, false);
      //uint32_t curve = CURVE_LINEAR;
      //CFHD_MetadataAdd(metadataRef, TAG_ENCODE_CURVE, METADATATYPE_UINT32, 4, &curve, false);
      //CFHD_MetadataAdd(metadataRef, TAG_DECODE_CURVE, METADATATYPE_UINT32, 4, &curve, false);
    }

    // rough estimation of required memory
    int thread_size = 0;
    // input buffer (below)
    thread_size += abs(layout->pitch)*layout->h;
    // output sample (maxed)
    thread_size += layout->w*layout->h*8;
    // transform buffer
    thread_size += layout->w*layout->h*2;
    {for(int i=1; i<channels; i++) thread_size += subw*layout->h*2; }
    // wavelet stack
    thread_size += layout->w*layout->h*21/8;
    {for(int i=1; i<channels; i++) thread_size += subw*layout->h*21/8; }
    int max_threads = 0;
    #ifndef _WIN64
    // lets think it is ok to use at most 600M
    max_threads = 600*1024*1024/thread_size;
    if(max_threads<2) max_threads = 2;
    #endif

    int threads = config.threads;
    if(threads!=1){
      if(threads==0){
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        threads = info.dwNumberOfProcessors;
      }
      if(threads>frame_total) threads = frame_total;
      if(max_threads && threads>max_threads) threads = max_threads;
    }
    if(layout->format==nsVDXPixmap::kPixFormat_XRGB64) buffer_count = 1;
    if(threads>1) buffer_count = threads;
    if(buffer_count){
      buffer = (Buffer*)malloc(sizeof(Buffer)*buffer_count);
      if(!buffer){ compress_end(); return ICERR_MEMORY; }
      memset(buffer,0,sizeof(Buffer)*buffer_count);
      int row_size = (int)layout->pitch;
      if(row_size<0) row_size = -row_size;
      int buffer_size = row_size*layout->h+16;
      {for(int i=0; i<buffer_count; i++){
        void* p = malloc(buffer_size);
        if(!p){ compress_end(); return ICERR_MEMORY; }
        buffer[i].p = p;
      }}
    }

    CFHD_Error error;
    if(threads==1){
      error = CFHD_OpenEncoder(&encRef, 0);
      error = CFHD_PrepareToEncode(encRef, layout->w, layout->h, pixelFormat, encodedFormat, encodingFlags, quality);
      CFHD_MetadataAttach(encRef, metadataRef);
    } else {
      error = CFHD_CreateEncoderPool(&poolRef, threads, buffer_count, 0);
      error = CFHD_PrepareEncoderPool(poolRef, layout->w, layout->h, pixelFormat, encodedFormat, encodingFlags, quality);
      CFHD_AttachEncoderPoolMetadata(poolRef, metadataRef);
      CFHD_StartEncoderPool(poolRef);
    }
    if(error) return ICERR_BADPARAM;

    frameNumber = 0;
    pool_depth = 0;

    return ICERR_OK;
  }

  LRESULT compress_end()
  {
    if(encRef){
      CFHD_CloseEncoder(encRef);
      encRef = 0;
    }
    if(poolRef){
      CFHD_ReleaseEncoderPool(poolRef);
      poolRef = 0;
    }
    if(metadataRef){
      CFHD_MetadataClose(metadataRef);
      metadataRef = 0;
    }
    if(buffer){
      for(int i=0; i<buffer_count; i++) free(buffer[i].p);
      free(buffer);
      buffer = 0;
      buffer_count = 0;
    }
    return ICERR_OK;
  }

  void copy_image(Buffer* buf, char* data, VDXPixmapLayout* layout)
  {
    char* d = (char*)((((ptrdiff_t)buf->p)+15) & ~15);
    char* s = data;
    int row_size = (int)layout->pitch;
    if(row_size<0){
      data += layout->pitch*(layout->h-1);
      row_size = -row_size;
    }
    buf->data = d;
    buf->pitch = layout->pitch;

    {for(int y=0; y<layout->h; y++){
      if(layout->format==nsVDXPixmap::kPixFormat_XRGB64){
        uint16_t* src = (uint16_t*)s;
        uint16_t* dst = (uint16_t*)d;
        for(int x=0; x<layout->w; x++){
          uint16_t b = src[0];
          uint16_t g = src[1];
          uint16_t r = src[2];
          uint16_t a = src[3];

          dst[0] = a<<4;
          dst[1] = r<<4;
          dst[2] = g<<4;
          dst[3] = b<<4;
          
          src += 4;
          dst += 4;
        }
      } else {
        memcpy(d,s,row_size);
      }
      d += buf->pitch;
      s += layout->pitch;
    }}
  }

  CFHD_Error send_frame(char* data, VDXPixmapLayout* layout)
  {
    ptrdiff_t pitch = layout->pitch;
    if(buffer){
      copy_image(buffer,data,layout);
      data = (char*)buffer->data;
      ptrdiff_t pitch = buffer->pitch;
    }

    if((pitch>0) ^ topdown){
      data += pitch*(layout->h-1);
      pitch = -pitch;
    }

    return CFHD_EncodeSample(encRef, data, pitch);
  }

  CFHD_Error send_frame_async(char* data, VDXPixmapLayout* layout)
  {
    Buffer* buf = 0;
    {for(int i=0; i<buffer_count; i++)
      if(buffer[i].ref==0){ buf = &buffer[i]; break; } }

    pool_depth++;
    buf->ref = 1;
    buf->frameNumber = frameNumber;
    copy_image(buf,data,layout);

    data = (char*)buf->data;
    ptrdiff_t pitch = buf->pitch;

    if((pitch>0) ^ topdown){
      data += pitch*(layout->h-1);
      pitch = -pitch;
    }

    CFHD_Error error = CFHD_EncodeAsyncSample(poolRef, frameNumber, data, pitch, 0);
    frameNumber++;
    return error;
  }

  LRESULT compress1(ICCOMPRESS *icc, VDXPixmapLayout* layout)
  {
    bool flush = frameNumber>=(uint32_t)frame_total;
    CFHD_Error error;

    if(encRef){
      error = send_frame((char*)icc->lpInput,layout);
      if(error!=CFHD_ERROR_OKAY) return ICERR_ERROR;
      void* cdata;
      size_t csize;
      CFHD_GetSampleData(encRef, &cdata, &csize);
      memcpy(icc->lpOutput,cdata,csize);
      icc->lpbiOutput->biSizeImage = csize;
      *icc->lpdwFlags = AVIIF_KEYFRAME;

    } else {
      icc->lpbiOutput->biSizeImage = 0;
      *icc->lpdwFlags = VDCOMPRESS_WAIT;

      if(pool_depth){
        uint32_t frame_number;
        CFHD_SampleBufferRef sampleRef = 0;
        if(flush || pool_depth==buffer_count)
          error = CFHD_WaitForSample(poolRef, &frame_number, &sampleRef);
        else
          error = CFHD_TestForSample(poolRef, &frame_number, &sampleRef);

        if(error==CFHD_ERROR_OKAY){
          void* cdata;
          size_t csize;
          CFHD_GetEncodedSample(sampleRef, &cdata, &csize);
          memcpy(icc->lpOutput,cdata,csize);
          icc->lpbiOutput->biSizeImage = csize;
          *icc->lpdwFlags = AVIIF_KEYFRAME;
          CFHD_ReleaseSampleBuffer(poolRef, sampleRef);
          pool_depth--;
          {for(int i=0; i<buffer_count; i++){
            Buffer* buf = &buffer[i];
            if(buf->ref && buf->frameNumber==frame_number){ buf->ref = 0; break; }
          }}
        }
      }

      if(!flush){
        error = send_frame_async((char*)icc->lpInput,layout);
        if(error!=CFHD_ERROR_OKAY) return ICERR_ERROR;
      }
    }

    return ICERR_OK;
  }

  virtual LRESULT configure(HWND parent)
  {
    ConfigCF dlg;
    dlg.Show(parent,this);
    return ICERR_OK;
  }
};

void ConfigCF::Show(HWND parent, CodecCF* codec)
{
  this->codec = codec;
  int rsize = codec->config_size();
  old_param = malloc(rsize);
  memcpy(old_param,&codec->config,rsize);
  VDXVideoFilterDialog::Show(hInstance, MAKEINTRESOURCE(IDD_ENC_CINEFORM), parent);
}

void ConfigCF::init_format()
{
  const char* color_names[] = {
    "RGB 12-bit", 
    "RGBA 12-bit",
    "YUV 4:2:2 10-bit",
    "bayer 12-bit : RG",
    "bayer 12-bit : GR",
    "bayer 12-bit : GB",
    "bayer 12-bit : BG",
  };

  SendDlgItemMessage(mhdlg, IDC_COLORSPACE, CB_RESETCONTENT, 0, 0);
  for(int i=0; i<7; i++)
    SendDlgItemMessage(mhdlg, IDC_COLORSPACE, CB_ADDSTRING, 0, (LPARAM)color_names[i]);
  SendDlgItemMessage(mhdlg, IDC_COLORSPACE, CB_SETCURSEL, codec->config.format-1, 0);
}

void ConfigCF::change_format(int sel)
{
  codec->config.format = sel+1;
  if(codec->config.format==CodecCF::format_rgba && codec->config.bits==10) codec->config.bits = 16;
  if(codec->config.format>=CodecCF::format_bayer_rg) codec->config.bits = 16;
  init_bits();
  change_bits();
}

void ConfigCF::init_bits()
{
  int format = codec->config.format;
  bool use8 = true;
  bool use10 = true;
  bool use16 = true;
  switch(codec->config.format){
  case CodecCF::format_rgba:
    use10 = false;
    break;
  case CodecCF::format_bayer_rg:
  case CodecCF::format_bayer_gr:
  case CodecCF::format_bayer_bg:
  case CodecCF::format_bayer_gb:
    use8 = false;
    use10 = false;
    break;
  }
  EnableWindow(GetDlgItem(mhdlg,IDC_8_BIT),use8);
  EnableWindow(GetDlgItem(mhdlg,IDC_10_BIT),use10);
  EnableWindow(GetDlgItem(mhdlg,IDC_16_BIT),use16);
  CheckDlgButton(mhdlg,IDC_8_BIT,codec->config.bits==8 ? BST_CHECKED:BST_UNCHECKED);
  CheckDlgButton(mhdlg,IDC_10_BIT,codec->config.bits==10 ? BST_CHECKED:BST_UNCHECKED);
  CheckDlgButton(mhdlg,IDC_16_BIT,codec->config.bits==16 ? BST_CHECKED:BST_UNCHECKED);
}

INT_PTR ConfigCF::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam){
  switch(msg){
  case WM_INITDIALOG:
    {
      SetDlgItemTextW(mhdlg,IDC_ENCODER_LABEL,CINEFORM_DESCRIPTION_L);
      init_format();
      init_bits();
      SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_SETRANGEMIN, FALSE, 0);
      SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_SETRANGEMAX, TRUE, 5);
      SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_SETPOS, TRUE, codec->config.quality);
      SetDlgItemText(mhdlg, IDC_QUALITY_VALUE, quality_names[codec->config.quality]);
      CheckDlgButton(mhdlg, IDC_THREADING, codec->config.threads!=1 ? BST_CHECKED:BST_UNCHECKED);
      return true;
    }

  case WM_HSCROLL:
    if((HWND)lParam==GetDlgItem(mhdlg,IDC_QUALITY)){
      codec->config.quality = SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_GETPOS, 0, 0);
      SetDlgItemText(mhdlg, IDC_QUALITY_VALUE, quality_names[codec->config.quality]);
      break;
    }
    return false;

  case WM_COMMAND:
    switch(LOWORD(wParam)){
    case IDOK:
      EndDialog(mhdlg, true);
      return TRUE;

    case IDCANCEL:
      memcpy(&codec->config, old_param, codec->config_size());
      EndDialog(mhdlg, false);
      return TRUE;

    case IDC_COLORSPACE:
      if(HIWORD(wParam)==LBN_SELCHANGE){
        int sel = (int)SendDlgItemMessage(mhdlg, IDC_COLORSPACE, CB_GETCURSEL, 0, 0);
        change_format(sel);
        return TRUE;
      }
      break;

    case IDC_8_BIT:
    case IDC_10_BIT:
    case IDC_16_BIT:
      if(IsDlgButtonChecked(mhdlg,IDC_8_BIT)) codec->config.bits = 8;
      if(IsDlgButtonChecked(mhdlg,IDC_10_BIT)) codec->config.bits = 10;
      if(IsDlgButtonChecked(mhdlg,IDC_16_BIT)) codec->config.bits = 16;
      change_bits();
      break;

    case IDC_THREADING:
      if(IsDlgButtonChecked(mhdlg,IDC_THREADING)) codec->config.threads = 0; else codec->config.threads = 1;
      break;
    }
    break;

  case WM_NOTIFY:
    {
      NMHDR* nm = (NMHDR*)lParam;
      if(nm->idFrom==IDC_LINK) switch (nm->code){
      case NM_CLICK:
      case NM_RETURN:
        {
          NMLINK* link = (NMLINK*)lParam;
          ShellExecuteW(NULL, L"open", link->item.szUrl, NULL, NULL, SW_SHOW);
        }
      }
    }
    break;
  }

  return FALSE;
}


LRESULT WINAPI DriverProc_CF(DWORD_PTR dwDriverId, HDRVR hDriver, UINT uMsg, LPARAM lParam1, LPARAM lParam2)
{
  CodecCF* codec = (CodecCF*)dwDriverId;

  switch (uMsg){
  case DRV_LOAD:
    return DRV_OK;

  case DRV_FREE:
    return DRV_OK;

  case DRV_OPEN:
    {
      ICOPEN* icopen = (ICOPEN*)lParam2;
      if(icopen && icopen->fccType != ICTYPE_VIDEO) return 0;

      CodecCF* codec = new CodecCF;
      if(!codec){
        if(icopen) icopen->dwError = ICERR_MEMORY;
        return 0;
      }

      if(icopen) icopen->dwError = ICERR_OK;
      return (LRESULT)codec;
    }

  case DRV_CLOSE:
    delete codec;
    return DRV_OK;

  case ICM_GETSTATE:
    {
      int rsize = codec->config_size();
      if(lParam1==0) return rsize;
      if(lParam2!=rsize) return ICERR_BADSIZE;
      memcpy((void*)lParam1, &codec->config, rsize);
      return ICERR_OK;
    }

  case ICM_SETSTATE:
    if(lParam1==0){
      codec->reset_config();
      return 0;
    }
    if(!codec->load_config((void*)lParam1,lParam2)) return 0;
    return codec->config_size();

  case ICM_GETINFO:
    {
      ICINFO* icinfo = (ICINFO*)lParam1;
      if(lParam2<sizeof(ICINFO)) return 0;
      icinfo->dwSize = sizeof(ICINFO);
      icinfo->fccType = ICTYPE_VIDEO;
      icinfo->dwVersion = 0;
      icinfo->dwVersionICM = ICVERSION;
      codec->getinfo(*icinfo);
      return sizeof(ICINFO);
    }

  case ICM_CONFIGURE:
    if(lParam1!=-1)
      return codec->configure((HWND)lParam1);
    return ICERR_OK;

  case ICM_ABOUT:
    return ICERR_UNSUPPORTED;

  case ICM_COMPRESS_END:
    return codec->compress_end();

  case ICM_COMPRESS_FRAMES_INFO:
    return codec->compress_frames_info((ICCOMPRESSFRAMES *)lParam1);
  }

  return ICERR_UNSUPPORTED;
}

LRESULT WINAPI VDDriverProc_CF(DWORD_PTR dwDriverId, HDRVR hDriver, UINT uMsg, LPARAM lParam1, LPARAM lParam2)
{
  CodecCF* codec = (CodecCF*)dwDriverId;

  switch(uMsg){
  case VDICM_ENUMFORMATS:
    if(lParam1==0) return CFHD_TAG;
    return 0;
  
  case VDICM_COMPRESS_INPUT_FORMAT:
    return codec->compress_input_format((FilterModPixmapInfo*)lParam1);

  case VDICM_COMPRESS_INPUT_INFO:
    return codec->compress_input_info((VDXPixmapLayout*)lParam1);

  case VDICM_COMPRESS_QUERY:
    return codec->compress_query((BITMAPINFO *)lParam2, (VDXPixmapLayout*)lParam1);
  
  case VDICM_COMPRESS_GET_FORMAT:
    return codec->compress_get_format((BITMAPINFO *)lParam2, (VDXPixmapLayout*)lParam1);
  
  case VDICM_COMPRESS_GET_SIZE:
    return codec->compress_get_size((BITMAPINFO *)lParam2);
  
  case VDICM_COMPRESS_BEGIN:
    return codec->compress_begin((BITMAPINFO *)lParam2, (VDXPixmapLayout*)lParam1);

  case VDICM_COMPRESS:
    return codec->compress1((ICCOMPRESS *)lParam1, (VDXPixmapLayout*)lParam2);

  case VDICM_COMPRESS_TRUNCATE:
    codec->frame_total = 0;
    return 0;

  case VDICM_COMPRESS_MATRIX_INFO:
    {
      int colorSpaceMode = (int)lParam1;
      int colorRangeMode = (int)lParam2;

      switch(colorSpaceMode){
      case nsVDXPixmap::kColorSpaceMode_709:
        codec->use709 = true;
        break;
      default:
        codec->use709 = false;
      }

      return ICERR_OK;
    }
  }

  return ICERR_UNSUPPORTED;
}
