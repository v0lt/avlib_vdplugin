#include "vd2\plugin\vdplugin.h"
#include <vd2/VDXFrame/VideoFilterDialog.h>
#include <memory.h>
#include <windows.h>
#include <vfw.h>
#include <commctrl.h>
#include "vd2\plugin\vdinputdriver.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}
#include "resource.h"
#include "compress.h"
#include "cineform.h"

#pragma warning(disable:4996) // silence deprecated stuff

void init_av();
extern HINSTANCE hInstance;

void copy_rgb24(AVFrame* frame, const VDXPixmapLayout* layout, const void* data)
{
  if(frame->format==AV_PIX_FMT_RGB24){for(int y=0; y<layout->h; y++){
    uint8* s = (uint8*)data + layout->data + layout->pitch*y;
    uint8* d = frame->data[0] + frame->linesize[0]*y;

    {for(int x=0; x<layout->w; x++){
      d[0] = s[2];
      d[1] = s[1];
      d[2] = s[0];

      s+=3;
      d+=3;
    }}
  }}
  if(frame->format==AV_PIX_FMT_GBRP){for(int y=0; y<layout->h; y++){
    uint8* s = (uint8*)data + layout->data + layout->pitch*y;

    uint8* g = frame->data[0] + frame->linesize[0]*y;
    uint8* b = frame->data[1] + frame->linesize[1]*y;
    uint8* r = frame->data[2] + frame->linesize[2]*y;

    {for(int x=0; x<layout->w; x++){
      b[0] = s[0];
      g[0] = s[1];
      r[0] = s[2];

      r++; g++; b++;
      s+=3;
    }}
  }}
}

void copy_rgb32(AVFrame* frame, const VDXPixmapLayout* layout, const void* data)
{
  {for(int y=0; y<layout->h; y++){
    uint8* s = (uint8*)data + layout->data + layout->pitch*y;
    uint8* d = frame->data[0] + frame->linesize[0]*y;
    memcpy(d,s,layout->w*4);
  }}
}

bool planar_rgb16(int format){
  switch(format){
  case AV_PIX_FMT_GBRP16LE:
  case AV_PIX_FMT_GBRP14LE:
  case AV_PIX_FMT_GBRP12LE:
  case AV_PIX_FMT_GBRP10LE:
  case AV_PIX_FMT_GBRP9LE:
    return true;
  }
  return false;
}

bool planar_rgba16(int format){
  switch(format){
  case AV_PIX_FMT_GBRAP16LE:
  case AV_PIX_FMT_GBRAP12LE:
  case AV_PIX_FMT_GBRAP10LE:
    return true;
  }
  return false;
}

void copy_rgb64(AVFrame* frame, const VDXPixmapLayout* layout, const void* data)
{
  if(frame->format==AV_PIX_FMT_BGRA64){for(int y=0; y<layout->h; y++){
    uint16* s = (uint16*)data + layout->data + layout->pitch*y/2;
    uint16* d = (uint16*)frame->data[0] + frame->linesize[0]*y/2;
    memcpy(d,s,layout->w*8);
  }}
  if(planar_rgb16(frame->format)){for(int y=0; y<layout->h; y++){
    uint16* s = (uint16*)data + layout->data/2 + layout->pitch*y/2;

    uint16* g = (uint16*)frame->data[0] + frame->linesize[0]*y/2;
    uint16* b = (uint16*)frame->data[1] + frame->linesize[1]*y/2;
    uint16* r = (uint16*)frame->data[2] + frame->linesize[2]*y/2;

    {for(int x=0; x<layout->w; x++){
      b[0] = s[0];
      g[0] = s[1];
      r[0] = s[2];

      r++; g++; b++;
      s+=4;
    }}
  }}
  if(planar_rgba16(frame->format)){for(int y=0; y<layout->h; y++){
    uint16* s = (uint16*)data + layout->data/2 + layout->pitch*y/2;

    uint16* g = (uint16*)frame->data[0] + frame->linesize[0]*y/2;
    uint16* b = (uint16*)frame->data[1] + frame->linesize[1]*y/2;
    uint16* r = (uint16*)frame->data[2] + frame->linesize[2]*y/2;
    uint16* a = (uint16*)frame->data[3] + frame->linesize[3]*y/2;

    {for(int x=0; x<layout->w; x++){
      b[0] = s[0];
      g[0] = s[1];
      r[0] = s[2];
      a[0] = s[3];

      r++; g++; b++; a++;
      s+=4;
    }}
  }}
}

void copy_yuv(AVFrame* frame, const VDXPixmapLayout* layout, const void* data, int bpp)
{
  int w2 = layout->w;
  int h2 = layout->h;
  switch(layout->format){
  case nsVDXPixmap::kPixFormat_YUV420_Planar:
  case nsVDXPixmap::kPixFormat_YUV420_Planar16:
    w2 = (w2+1)/2;
    h2 = h2/2;
    break;
  case nsVDXPixmap::kPixFormat_YUV422_Planar:
  case nsVDXPixmap::kPixFormat_YUV422_Planar16:
    w2 = (w2+1)/2;
    break;
  }

  {for(int y=0; y<layout->h; y++){
    uint8* s = (uint8*)data + layout->data + layout->pitch*y;
    uint8* d = frame->data[0] + frame->linesize[0]*y;
    memcpy(d,s,layout->w*bpp);
  }}
  {for(int y=0; y<h2; y++){
    uint8* s = (uint8*)data + layout->data2 + layout->pitch2*y;
    uint8* d = frame->data[1] + frame->linesize[1]*y;
    memcpy(d,s,w2*bpp);
  }}
  {for(int y=0; y<h2; y++){
    uint8* s = (uint8*)data + layout->data3 + layout->pitch3*y;
    uint8* d = frame->data[2] + frame->linesize[2]*y;
    memcpy(d,s,w2*bpp);
  }}

  switch(layout->format){
  case nsVDXPixmap::kPixFormat_YUV420_Alpha_Planar:
  case nsVDXPixmap::kPixFormat_YUV422_Alpha_Planar:
  case nsVDXPixmap::kPixFormat_YUV444_Alpha_Planar:
  case nsVDXPixmap::kPixFormat_YUV420_Alpha_Planar16:
  case nsVDXPixmap::kPixFormat_YUV422_Alpha_Planar16:
  case nsVDXPixmap::kPixFormat_YUV444_Alpha_Planar16:
    {
      const VDXPixmapLayoutAlpha* layout2 = (const VDXPixmapLayoutAlpha*)layout;
      {for(int y=0; y<layout->h; y++){
        uint8* s = (uint8*)data + layout2->data4 + layout2->pitch4*y;
        uint8* d = frame->data[3] + frame->linesize[3]*y;
        memcpy(d,s,layout->w*bpp);
      }}
      break;
    }
  }
}

void copy_gray(AVFrame* frame, const VDXPixmapLayout* layout, const void* data, int bpp)
{
  int w2 = layout->w;
  int h2 = layout->h;
  {for(int y=0; y<layout->h; y++){
    uint8* s = (uint8*)data + layout->data + layout->pitch*y;
    uint8* d = frame->data[0] + frame->linesize[0]*y;
    memcpy(d,s,layout->w*bpp);
  }}
}

struct CodecBase: public CodecClass{
  enum {
    format_rgb = 1,
    format_rgba = 2,
    format_yuv420 = 3,
    format_yuv422 = 4,
    format_yuv444 = 5,
    format_yuva420 = 6,
    format_yuva422 = 7,
    format_yuva444 = 8,
    format_gray = 9,
  };

  struct Config{
    int version;
    int format;
    int bits;

    Config(){ clear(); }
    void clear(){
      version = 0;
      format = format_rgb;
      bits = 8;
    }
  }* config;

  AVRational time_base;
  int keyint;
  LONG frame_total;
  AVColorRange color_range;
  AVColorSpace colorspace;

  AVCodecID codec_id;
  const char* codec_name;
  int codec_tag;
  AVCodec* codec;
  AVCodecContext* ctx;
  AVFrame* frame;
  VDLogProc logProc;
  bool global_header;

  CodecBase(){
    codec=0; ctx=0; frame=0;
    keyint=1;
    color_range=AVCOL_RANGE_UNSPECIFIED;
    colorspace=AVCOL_SPC_UNSPECIFIED;
    codec_id = AV_CODEC_ID_NONE;
    codec_name = 0;
    codec_tag = 0;
    config = 0;
    logProc = 0;
    global_header = false;
  }

  virtual ~CodecBase(){
    compress_end();
  }

  bool init(){
    if(codec_id!=AV_CODEC_ID_NONE) codec = avcodec_find_encoder(codec_id);
    else codec = avcodec_find_encoder_by_name(codec_name);
    return codec!=0;
  }

  virtual void getinfo(ICINFO& info) = 0;

  virtual int config_size(){ return sizeof(Config); }

  virtual void reset_config(){ config->clear(); }

  virtual bool load_config(void* data, size_t size){
    size_t rsize = config_size();
    if(size!=rsize) return false;
    int src_version = *(int*)data;
    if(src_version!=config->version) return false;
    memcpy(config, data, rsize);
    return true;
  }

  virtual bool test_av_format(AVPixelFormat format){
    if(!codec->pix_fmts) return false;
    for(int i=0; ; i++){
      AVPixelFormat f = codec->pix_fmts[i];
      if(f==-1) break;
      if(f==format) return true;
    }
    return false; 
  }

  bool test_bits(int format, int bits){
    switch(format){
    case format_rgba:
      if(bits==8) return test_av_format(AV_PIX_FMT_RGB32);
      if(bits==10) return test_av_format(AV_PIX_FMT_GBRAP10LE);
      if(bits==12) return test_av_format(AV_PIX_FMT_GBRAP12LE);
      if(bits==16) return test_av_format(AV_PIX_FMT_BGRA64) || test_av_format(AV_PIX_FMT_GBRAP16LE);
      break;
    case format_rgb:
      if(bits==8) return test_av_format(AV_PIX_FMT_0RGB32) || test_av_format(AV_PIX_FMT_RGB24) || test_av_format(AV_PIX_FMT_GBRP);
      if(bits==9) return test_av_format(AV_PIX_FMT_GBRP9LE);
      if(bits==10) return test_av_format(AV_PIX_FMT_GBRP10LE);
      if(bits==12) return test_av_format(AV_PIX_FMT_GBRP12LE);
      if(bits==14) return test_av_format(AV_PIX_FMT_GBRP14LE);
      if(bits==16) return test_av_format(AV_PIX_FMT_GBRP16LE);
      break;
    case format_yuv420:
      if(bits==8) return test_av_format(AV_PIX_FMT_YUV420P);
      if(bits==9) return test_av_format(AV_PIX_FMT_YUV420P9LE);
      if(bits==10) return test_av_format(AV_PIX_FMT_YUV420P10LE);
      if(bits==12) return test_av_format(AV_PIX_FMT_YUV420P12LE);
      if(bits==14) return test_av_format(AV_PIX_FMT_YUV420P14LE);
      if(bits==16) return test_av_format(AV_PIX_FMT_YUV420P16LE);
      break;
    case format_yuv422:
      if(bits==8) return test_av_format(AV_PIX_FMT_YUV422P);
      if(bits==9) return test_av_format(AV_PIX_FMT_YUV422P9LE);
      if(bits==10) return test_av_format(AV_PIX_FMT_YUV422P10LE);
      if(bits==12) return test_av_format(AV_PIX_FMT_YUV422P12LE);
      if(bits==14) return test_av_format(AV_PIX_FMT_YUV422P14LE);
      if(bits==16) return test_av_format(AV_PIX_FMT_YUV422P16LE);
      break;
    case format_yuv444:
      if(bits==8) return test_av_format(AV_PIX_FMT_YUV444P);
      if(bits==9) return test_av_format(AV_PIX_FMT_YUV444P9LE);
      if(bits==10) return test_av_format(AV_PIX_FMT_YUV444P10LE);
      if(bits==12) return test_av_format(AV_PIX_FMT_YUV444P12LE);
      if(bits==14) return test_av_format(AV_PIX_FMT_YUV444P14LE);
      if(bits==16) return test_av_format(AV_PIX_FMT_YUV444P16LE);
      break;
    case format_yuva420:
      if(bits==8) return test_av_format(AV_PIX_FMT_YUVA420P);
      if(bits==9) return test_av_format(AV_PIX_FMT_YUVA420P9LE);
      if(bits==10) return test_av_format(AV_PIX_FMT_YUVA420P10LE);
      if(bits==16) return test_av_format(AV_PIX_FMT_YUVA420P16LE);
      break;
    case format_yuva422:
      if(bits==8) return test_av_format(AV_PIX_FMT_YUVA422P);
      if(bits==9) return test_av_format(AV_PIX_FMT_YUVA422P9LE);
      if(bits==10) return test_av_format(AV_PIX_FMT_YUVA422P10LE);
      if(bits==16) return test_av_format(AV_PIX_FMT_YUVA422P16LE);
      break;
    case format_yuva444:
      if(bits==8) return test_av_format(AV_PIX_FMT_YUVA444P);
      if(bits==9) return test_av_format(AV_PIX_FMT_YUVA444P9LE);
      if(bits==10) return test_av_format(AV_PIX_FMT_YUVA444P10LE);
      if(bits==16) return test_av_format(AV_PIX_FMT_YUVA444P16LE);
      break;
    case format_gray:
      if(bits==8) return test_av_format(AV_PIX_FMT_GRAY8);
      if(bits==9) return test_av_format(AV_PIX_FMT_GRAY9LE);
      if(bits==10) return test_av_format(AV_PIX_FMT_GRAY10LE);
      if(bits==12) return test_av_format(AV_PIX_FMT_GRAY12LE);
      if(bits==16) return test_av_format(AV_PIX_FMT_GRAY16LE);
      break;
    }
    return false;
  }

  virtual int compress_input_info(VDXPixmapLayout* src){
    return 0;
  }

  LRESULT compress_input_format(FilterModPixmapInfo* info){
    if(config->format==format_rgba){
      if(config->bits==8){
        return nsVDXPixmap::kPixFormat_XRGB8888;
      }
      if(config->bits>8){
        int max_value = (1 << config->bits)-1;
        if(info){
          info->ref_r = max_value;
          info->ref_g = max_value;
          info->ref_b = max_value;
          info->ref_a = max_value;
        }
        return nsVDXPixmap::kPixFormat_XRGB64;
      }
    }

    if(config->format==format_rgb){
      if(config->bits==8){
        if(test_av_format(AV_PIX_FMT_0RGB32)) return nsVDXPixmap::kPixFormat_XRGB8888;
        if(test_av_format(AV_PIX_FMT_RGB24)) return nsVDXPixmap::kPixFormat_RGB888;
        if(test_av_format(AV_PIX_FMT_GBRP)) return nsVDXPixmap::kPixFormat_RGB888;
      }

      if(config->bits>8){
        int max_value = (1 << config->bits)-1;
        if(info){
          info->ref_r = max_value;
          info->ref_g = max_value;
          info->ref_b = max_value;
          info->ref_a = max_value;
        }
        return nsVDXPixmap::kPixFormat_XRGB64;
      }
    }

    if(config->format==format_yuv420){
      if(config->bits==8){
        return nsVDXPixmap::kPixFormat_YUV420_Planar;
      }
      if(config->bits>8){
        int max_value = (1 << config->bits)-1;
        if(info){
          info->ref_r = max_value;
        }
        return nsVDXPixmap::kPixFormat_YUV420_Planar16;
      }
    }

    if(config->format==format_yuv422){
      if(config->bits==8){
        return nsVDXPixmap::kPixFormat_YUV422_Planar;
      }
      if(config->bits>8){
        int max_value = (1 << config->bits)-1;
        if(info){
          info->ref_r = max_value;
        }
        return nsVDXPixmap::kPixFormat_YUV422_Planar16;
      }
    }

    if(config->format==format_yuv444){
      if(config->bits==8){
        return nsVDXPixmap::kPixFormat_YUV444_Planar;
      }
      if(config->bits>8){
        int max_value = (1 << config->bits)-1;
        if(info){
          info->ref_r = max_value;
        }
        return nsVDXPixmap::kPixFormat_YUV444_Planar16;
      }
    }

    if(config->format==format_yuva420){
      if(config->bits==8){
        return nsVDXPixmap::kPixFormat_YUV420_Alpha_Planar;
      }
      if(config->bits>8){
        int max_value = (1 << config->bits)-1;
        if(info){
          info->ref_r = max_value;
          info->ref_a = max_value;
        }
        return nsVDXPixmap::kPixFormat_YUV420_Alpha_Planar16;
      }
    }

    if(config->format==format_yuva422){
      if(config->bits==8){
        return nsVDXPixmap::kPixFormat_YUV422_Alpha_Planar;
      }
      if(config->bits>8){
        int max_value = (1 << config->bits)-1;
        if(info){
          info->ref_r = max_value;
          info->ref_a = max_value;
        }
        return nsVDXPixmap::kPixFormat_YUV422_Alpha_Planar16;
      }
    }

    if(config->format==format_yuva444){
      if(config->bits==8){
        return nsVDXPixmap::kPixFormat_YUV444_Alpha_Planar;
      }
      if(config->bits>8){
        int max_value = (1 << config->bits)-1;
        if(info){
          info->ref_r = max_value;
          info->ref_a = max_value;
        }
        return nsVDXPixmap::kPixFormat_YUV444_Alpha_Planar16;
      }
    }

    if(config->format==format_gray){
      if(config->bits==8){
        return nsVDXPixmap::kPixFormat_Y8;
      }
      if(config->bits>8){
        int max_value = (1 << config->bits)-1;
        if(info){
          info->ref_r = max_value;
        }
        return nsVDXPixmap::kPixFormat_Y16;
      }
    }

    return 0;
  }

  LRESULT compress_get_format(BITMAPINFO *lpbiOutput, VDXPixmapLayout* layout)
  {
    BITMAPINFOHEADER *outhdr = &lpbiOutput->bmiHeader;
    int extra_size = 0;
    if(ctx) extra_size = (ctx->extradata_size+1) & ~1;

    if(!lpbiOutput) return sizeof(BITMAPINFOHEADER)+extra_size;

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
    if(layout->format==nsVDXPixmap::kPixFormat_XRGB64) outhdr->biBitCount = 48;
    outhdr->biCompression = codec_tag;
    outhdr->biSizeImage   = iWidth*iHeight*8;
    if(ctx){
      outhdr->biSize = sizeof(BITMAPINFOHEADER) + ctx->extradata_size;
      if(ctx->codec_tag) outhdr->biCompression = ctx->codec_tag;
      uint8* p = ((uint8*)outhdr)+sizeof(BITMAPINFOHEADER);
      memset(p,0,extra_size);
      memcpy(p,ctx->extradata,ctx->extradata_size);
    }

    return ICERR_OK;
  }

  LRESULT compress_query(BITMAPINFO *lpbiOutput, VDXPixmapLayout* layout)
  {
    BITMAPINFOHEADER *outhdr = &lpbiOutput->bmiHeader;

    if(layout->format!=compress_input_format(0)) return ICERR_BADFORMAT;
    int iWidth  = layout->w;
    int iHeight = layout->h;

    if(iWidth <= 0 || iHeight <= 0) return ICERR_BADFORMAT;

    if(!lpbiOutput) return ICERR_OK;

    if(iWidth != outhdr->biWidth || iHeight != outhdr->biHeight)
      return ICERR_BADFORMAT;

    if(outhdr->biCompression!=codec_tag)
      return ICERR_BADFORMAT;

    return ICERR_OK;
  }

  LRESULT compress_get_size(BITMAPINFO *lpbiOutput)
  {
    return lpbiOutput->bmiHeader.biWidth*lpbiOutput->bmiHeader.biHeight*8 + 4096;
  }

  LRESULT compress_frames_info(ICCOMPRESSFRAMES *icf)
  {
    frame_total = icf->lFrameCount;
    time_base.den = icf->dwRate;
    time_base.num = icf->dwScale;
    keyint = icf->lKeyRate;
    return ICERR_OK;
  }

  virtual bool init_ctx(VDXPixmapLayout* layout)
  {
    return true;
  }

  LRESULT compress_begin(BITMAPINFO *lpbiOutput, VDXPixmapLayout* layout)
  {
    if(compress_query(lpbiOutput, layout) != ICERR_OK){
      return ICERR_BADFORMAT;
    }

    ctx = avcodec_alloc_context3(codec);

    if(config->format==format_rgba){
      switch(config->bits){
      case 16:
        ctx->pix_fmt = AV_PIX_FMT_GBRAP16LE;
        break;
      case 12:
        ctx->pix_fmt = AV_PIX_FMT_GBRAP12LE;
        break;
      case 10:
        ctx->pix_fmt = AV_PIX_FMT_GBRAP10LE;
        break;
      case 8:
        ctx->pix_fmt = AV_PIX_FMT_RGB32;
        break;
      }
    }

    if(config->format==format_rgb){
      switch(config->bits){
      case 16:
        ctx->pix_fmt = AV_PIX_FMT_GBRP16LE;
        break;
      case 14:
        ctx->pix_fmt = AV_PIX_FMT_GBRP14LE;
        break;
      case 12:
        ctx->pix_fmt = AV_PIX_FMT_GBRP12LE;
        break;
      case 10:
        ctx->pix_fmt = AV_PIX_FMT_GBRP10LE;
        break;
      case 9:
        ctx->pix_fmt = AV_PIX_FMT_GBRP9LE;
        break;
      case 8:
        if(test_av_format(AV_PIX_FMT_0RGB32)){
          ctx->pix_fmt = AV_PIX_FMT_0RGB32;
          break;
        }
        if(test_av_format(AV_PIX_FMT_RGB24)){
          ctx->pix_fmt = AV_PIX_FMT_RGB24;
          break;
        }
        if(test_av_format(AV_PIX_FMT_GBRP)){
          ctx->pix_fmt = AV_PIX_FMT_GBRP;
          break;
        }
        break;
      }
    }

    if(config->format==format_yuv420){
      switch(config->bits){
      case 16:
        ctx->pix_fmt = AV_PIX_FMT_YUV420P16LE;
        break;
      case 14:
        ctx->pix_fmt = AV_PIX_FMT_YUV420P14LE;
        break;
      case 12:
        ctx->pix_fmt = AV_PIX_FMT_YUV420P12LE;
        break;
      case 10:
        ctx->pix_fmt = AV_PIX_FMT_YUV420P10LE;
        break;
      case 9:
        ctx->pix_fmt = AV_PIX_FMT_YUV420P9LE;
        break;
      case 8:
        ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        break;
      }
    }

    if(config->format==format_yuv422){
      switch(config->bits){
      case 16:
        ctx->pix_fmt = AV_PIX_FMT_YUV422P16LE;
        break;
      case 14:
        ctx->pix_fmt = AV_PIX_FMT_YUV422P14LE;
        break;
      case 12:
        ctx->pix_fmt = AV_PIX_FMT_YUV422P12LE;
        break;
      case 10:
        ctx->pix_fmt = AV_PIX_FMT_YUV422P10LE;
        break;
      case 9:
        ctx->pix_fmt = AV_PIX_FMT_YUV422P9LE;
        break;
      case 8:
        ctx->pix_fmt = AV_PIX_FMT_YUV422P;
        break;
      }
    }

    if(config->format==format_yuv444){
      switch(config->bits){
      case 16:
        ctx->pix_fmt = AV_PIX_FMT_YUV444P16LE;
        break;
      case 14:
        ctx->pix_fmt = AV_PIX_FMT_YUV444P14LE;
        break;
      case 12:
        ctx->pix_fmt = AV_PIX_FMT_YUV444P12LE;
        break;
      case 10:
        ctx->pix_fmt = AV_PIX_FMT_YUV444P10LE;
        break;
      case 9:
        ctx->pix_fmt = AV_PIX_FMT_YUV444P9LE;
        break;
      case 8:
        ctx->pix_fmt = AV_PIX_FMT_YUV444P;
        break;
      }
    }

    if(config->format==format_yuva420){
      switch(config->bits){
      case 16:
        ctx->pix_fmt = AV_PIX_FMT_YUVA420P16LE;
        break;
      case 10:
        ctx->pix_fmt = AV_PIX_FMT_YUVA420P10LE;
        break;
      case 9:
        ctx->pix_fmt = AV_PIX_FMT_YUVA420P9LE;
        break;
      case 8:
        ctx->pix_fmt = AV_PIX_FMT_YUVA420P;
        break;
      }
    }

    if(config->format==format_yuva422){
      switch(config->bits){
      case 16:
        ctx->pix_fmt = AV_PIX_FMT_YUVA422P16LE;
        break;
      case 10:
        ctx->pix_fmt = AV_PIX_FMT_YUVA422P10LE;
        break;
      case 9:
        ctx->pix_fmt = AV_PIX_FMT_YUVA422P9LE;
        break;
      case 8:
        ctx->pix_fmt = AV_PIX_FMT_YUVA422P;
        break;
      }
    }

    if(config->format==format_yuva444){
      switch(config->bits){
      case 16:
        ctx->pix_fmt = AV_PIX_FMT_YUVA444P16LE;
        break;
      case 10:
        ctx->pix_fmt = AV_PIX_FMT_YUVA444P10LE;
        break;
      case 9:
        ctx->pix_fmt = AV_PIX_FMT_YUVA444P9LE;
        break;
      case 8:
        ctx->pix_fmt = AV_PIX_FMT_YUVA444P;
        break;
      }
    }

    if(config->format==format_gray){
      switch(config->bits){
      case 16:
        ctx->pix_fmt = AV_PIX_FMT_GRAY16LE;
        break;
      case 12:
        ctx->pix_fmt = AV_PIX_FMT_GRAY12LE;
        break;
      case 10:
        ctx->pix_fmt = AV_PIX_FMT_GRAY10LE;
        break;
      case 9:
        ctx->pix_fmt = AV_PIX_FMT_GRAY9LE;
        break;
      case 8:
        ctx->pix_fmt = AV_PIX_FMT_GRAY8;
        break;
      }
    }

    ctx->bit_rate = 400000;
    ctx->width = layout->w;
    ctx->height = layout->h;
    ctx->time_base = time_base;
    ctx->gop_size = 1;
    ctx->max_b_frames = 1;
    ctx->framerate = av_make_q(time_base.den,time_base.num);

    ctx->color_range = color_range;
    ctx->colorspace = colorspace;
    if(global_header) ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if(!init_ctx(layout)) return ICERR_BADPARAM;

    if(keyint>1) ctx->gop_size = keyint;

    if(avcodec_open2(ctx, codec, NULL)<0){ compress_end(); return ICERR_BADPARAM; }

    frame = av_frame_alloc();
    frame->format = ctx->pix_fmt;
    frame->color_range = ctx->color_range;
    frame->colorspace = ctx->colorspace;
    frame->width  = ctx->width;
    frame->height = ctx->height;

    int ret = av_image_alloc(frame->data, frame->linesize, ctx->width, ctx->height, ctx->pix_fmt, 32);
    if(ret<0){ compress_end(); return ICERR_MEMORY; }

    return ICERR_OK;
  }

  void streamControl(VDXStreamControl* sc)
  {
    global_header = sc->global_header;
  }

  void getStreamInfo(VDXStreamInfo* si)
  {
    AVCodecParameters* par = avcodec_parameters_alloc();
    avcodec_parameters_from_context(par,ctx);

    si->avcodec_version = LIBAVCODEC_VERSION_INT;

    si->format = par->format;
    si->bit_rate = par->bit_rate;
    si->bits_per_coded_sample = par->bits_per_coded_sample;
    si->bits_per_raw_sample = par->bits_per_raw_sample;
    si->profile = par->profile;
    si->level = par->level;
    si->sar_width = par->sample_aspect_ratio.num;
    si->sar_height = par->sample_aspect_ratio.den;
    si->field_order = par->field_order;
    si->color_range = par->color_range;
    si->color_primaries = par->color_primaries;
    si->color_trc = par->color_trc;
    si->color_space = par->color_space;
    si->chroma_location = par->chroma_location;
    si->video_delay = par->video_delay;

    avcodec_parameters_free(&par);
  }

  LRESULT compress_end()
  {
    if(ctx){
      avcodec_close(ctx);
      av_free(ctx);
      ctx = 0;
    }
    if(frame){
      av_freep(&frame->data[0]);
      av_frame_free(&frame);
      frame = 0;
    }
    return ICERR_OK;
  }

  LRESULT compress1(ICCOMPRESS *icc, VDXPixmapLayout* layout)
  {
    VDXPictureCompress pc;
    pc.px = layout;
    return compress2(icc,&pc);
  }

  LRESULT compress2(ICCOMPRESS *icc, VDXPictureCompress* pc)
  {
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = 0;
    pkt.size = 0;

    int got_output;
    int ret;

    const VDXPixmapLayout* layout = pc->px;

    if(icc->lFrameNum<frame_total){
      frame->pts = icc->lFrameNum;

      switch(layout->format){
      case nsVDXPixmap::kPixFormat_RGB888:
        copy_rgb24(frame,layout,icc->lpInput);
        break;
      case nsVDXPixmap::kPixFormat_XRGB8888:
        copy_rgb32(frame,layout,icc->lpInput);
        break;
      case nsVDXPixmap::kPixFormat_XRGB64:
        copy_rgb64(frame,layout,icc->lpInput);
        break;
      case nsVDXPixmap::kPixFormat_YUV420_Planar:
      case nsVDXPixmap::kPixFormat_YUV422_Planar:
      case nsVDXPixmap::kPixFormat_YUV444_Planar:
      case nsVDXPixmap::kPixFormat_YUV420_Alpha_Planar:
      case nsVDXPixmap::kPixFormat_YUV422_Alpha_Planar:
      case nsVDXPixmap::kPixFormat_YUV444_Alpha_Planar:
        copy_yuv(frame,layout,icc->lpInput,1);
        break;
      case nsVDXPixmap::kPixFormat_YUV420_Planar16:
      case nsVDXPixmap::kPixFormat_YUV422_Planar16:
      case nsVDXPixmap::kPixFormat_YUV444_Planar16:
      case nsVDXPixmap::kPixFormat_YUV420_Alpha_Planar16:
      case nsVDXPixmap::kPixFormat_YUV422_Alpha_Planar16:
      case nsVDXPixmap::kPixFormat_YUV444_Alpha_Planar16:
        copy_yuv(frame,layout,icc->lpInput,2);
        break;
      case nsVDXPixmap::kPixFormat_Y8:
        copy_gray(frame,layout,icc->lpInput,1);
        break;
      case nsVDXPixmap::kPixFormat_Y16:
        copy_gray(frame,layout,icc->lpInput,2);
        break;
      }

      ret = avcodec_encode_video2(ctx, &pkt, frame, &got_output);
    } else {
      ret = avcodec_encode_video2(ctx, &pkt, 0, &got_output);
    }

    if(ret<0){ compress_end(); return ICERR_MEMORY; }

    if(got_output){
      if(pkt.size>(int)icc->lpbiOutput->biSizeImage){
        av_packet_unref(&pkt);
        return ICERR_MEMORY;
      }

      DWORD flags = 0;
      if(pkt.flags & AV_PKT_FLAG_KEY) flags = AVIIF_KEYFRAME;
      *icc->lpdwFlags = flags;

      memcpy(icc->lpOutput,pkt.data,pkt.size);
      icc->lpbiOutput->biSizeImage = pkt.size;
      pc->pts = pkt.pts;
      pc->dts = pkt.dts;
      pc->duration = pkt.duration;
      av_packet_unref(&pkt);
    } else {
      icc->lpbiOutput->biSizeImage = 0;
      *icc->lpdwFlags = VDCOMPRESS_WAIT;
    }

    return ICERR_OK;
  }

  virtual LRESULT configure(HWND parent) = 0;
};

//---------------------------------------------------------------------------

class ConfigBase: public VDXVideoFilterDialog{
public:
  CodecBase* codec;
  void* old_param;
  int dialog_id;
  int idc_message;

  ConfigBase()
  {
    codec = 0;
    old_param = 0;
    idc_message = -1;
  }

  virtual ~ConfigBase()
  {
    free(old_param);
  }

  void Show(HWND parent, CodecBase* codec)
  {
    this->codec = codec;
    int rsize = codec->config_size();
    old_param = malloc(rsize);
    memcpy(old_param,codec->config,rsize);
    VDXVideoFilterDialog::Show(hInstance, MAKEINTRESOURCE(dialog_id), parent);
  }

  virtual INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
  virtual void init_format();
  virtual void change_format(int sel);
  virtual void change_bits(){}
  void init_bits();
  void adjust_bits();
  void notify_bits_change(int bits_new, int bits_old);
  void notify_hide();
};

void ConfigBase::init_format()
{
  const char* color_names[] = {
    "RGB", 
    "RGB + Alpha",
    "YUV 4:2:0",
    "YUV 4:2:2",
    "YUV 4:4:4",
    "YUV 4:2:0 + Alpha",
    "YUV 4:2:2 + Alpha",
    "YUV 4:4:4 + Alpha",
    "Gray",
  };

  SendDlgItemMessage(mhdlg, IDC_COLORSPACE, CB_RESETCONTENT, 0, 0);
  for(int i=0; i<9; i++)
    SendDlgItemMessage(mhdlg, IDC_COLORSPACE, CB_ADDSTRING, 0, (LPARAM)color_names[i]);
  SendDlgItemMessage(mhdlg, IDC_COLORSPACE, CB_SETCURSEL, codec->config->format-1, 0);
}

void ConfigBase::adjust_bits()
{
  int format = codec->config->format;
  int bits = codec->config->bits;
  if(!codec->test_bits(format,bits)){
    int option[6] = {
      codec->test_bits(format,8) ? 8:0,
      codec->test_bits(format,9) ? 9:0,
      codec->test_bits(format,10)? 10:0,
      codec->test_bits(format,12)? 12:0,
      codec->test_bits(format,14)? 14:0,
      codec->test_bits(format,16)? 16:0,
    };

    int bits1=0;
    {for(int i=0; i<6; i++){
      int x = option[i];
      if(x) bits1=x;
      if(x>=bits) break;
    }}

    notify_bits_change(bits1,codec->config->bits);
    codec->config->bits = bits1;
  }
}

void ConfigBase::change_format(int sel)
{
  codec->config->format = sel+1;
  adjust_bits();
  init_bits();
  change_bits();
}

void ConfigBase::init_bits()
{
  int format = codec->config->format;
  EnableWindow(GetDlgItem(mhdlg,IDC_8_BIT), codec->test_bits(format,8));
  EnableWindow(GetDlgItem(mhdlg,IDC_9_BIT), codec->test_bits(format,9));
  EnableWindow(GetDlgItem(mhdlg,IDC_10_BIT),codec->test_bits(format,10));
  EnableWindow(GetDlgItem(mhdlg,IDC_12_BIT),codec->test_bits(format,12));
  EnableWindow(GetDlgItem(mhdlg,IDC_14_BIT),codec->test_bits(format,14));
  EnableWindow(GetDlgItem(mhdlg,IDC_16_BIT),codec->test_bits(format,16));
  CheckDlgButton(mhdlg,IDC_8_BIT,codec->config->bits==8 ? BST_CHECKED:BST_UNCHECKED);
  CheckDlgButton(mhdlg,IDC_9_BIT,codec->config->bits==9 ? BST_CHECKED:BST_UNCHECKED);
  CheckDlgButton(mhdlg,IDC_10_BIT,codec->config->bits==10 ? BST_CHECKED:BST_UNCHECKED);
  CheckDlgButton(mhdlg,IDC_12_BIT,codec->config->bits==12 ? BST_CHECKED:BST_UNCHECKED);
  CheckDlgButton(mhdlg,IDC_14_BIT,codec->config->bits==14 ? BST_CHECKED:BST_UNCHECKED);
  CheckDlgButton(mhdlg,IDC_16_BIT,codec->config->bits==16 ? BST_CHECKED:BST_UNCHECKED);
}

void ConfigBase::notify_hide(){
  if(idc_message!=-1) SetDlgItemText(mhdlg,idc_message,0);
}

void ConfigBase::notify_bits_change(int bits_new, int bits_old){
  if(idc_message!=-1){
    wchar_t buf[80];
    swprintf(buf,L"(!) Bit depth adjusted (was %d)",bits_old);
    SetDlgItemTextW(mhdlg,idc_message,buf);
  }
}

INT_PTR ConfigBase::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam){
  switch(msg){
  case WM_INITDIALOG:
    {
      SetDlgItemText(mhdlg,IDC_ENCODER_LABEL,LIBAVCODEC_IDENT);
      init_format();
      init_bits();
      notify_hide();
      return true;
    }

  case WM_COMMAND:
    switch(LOWORD(wParam)){
    case IDOK:
      EndDialog(mhdlg, true);
      return TRUE;

    case IDCANCEL:
      memcpy(codec->config, old_param, codec->config_size());
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
    case IDC_9_BIT:
    case IDC_10_BIT:
    case IDC_12_BIT:
    case IDC_14_BIT:
    case IDC_16_BIT:
      notify_hide();
      if(IsDlgButtonChecked(mhdlg,IDC_8_BIT)) codec->config->bits = 8;
      if(IsDlgButtonChecked(mhdlg,IDC_9_BIT)) codec->config->bits = 9;
      if(IsDlgButtonChecked(mhdlg,IDC_10_BIT)) codec->config->bits = 10;
      if(IsDlgButtonChecked(mhdlg,IDC_12_BIT)) codec->config->bits = 12;
      if(IsDlgButtonChecked(mhdlg,IDC_14_BIT)) codec->config->bits = 14;
      if(IsDlgButtonChecked(mhdlg,IDC_16_BIT)) codec->config->bits = 16;
      change_bits();
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

//---------------------------------------------------------------------------

int ffv1_slice_tab[] = {0,4,6,9,12,16,24,30,36,42};

class ConfigFFV1: public ConfigBase{
public:
  ConfigFFV1(){ dialog_id = IDD_ENC_FFV1; idc_message = IDC_MESSAGE; }
  INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
  void apply_level();
  void init_slices();
  void init_coder();
  virtual void change_bits();
};

struct CodecFFV1: public CodecBase{
  enum{ tag=MKTAG('F', 'F', 'V', '1') };
  struct Config: public CodecBase::Config{
    int level;
    int slice;
    int coder;
    int context;
    int slicecrc;

    Config(){ init(); }
    void clear(){ CodecBase::Config::clear(); init(); }
    void init(){
      version = 1;
      level = 3;
      slice = 0;
      coder = 1;
      context = 0;
      slicecrc = 1;
      format = format_yuv422;
      bits = 10;
    }
  } codec_config;

  CodecFFV1(){
    config = &codec_config;
    codec_id = AV_CODEC_ID_FFV1;
    codec_tag = tag;
  }

  int config_size(){ return sizeof(Config); }
  void reset_config(){ codec_config.clear(); }

  virtual bool test_av_format(AVPixelFormat format){
    switch(format){
    case AV_PIX_FMT_GRAY9:
    case AV_PIX_FMT_YUV444P9:
    case AV_PIX_FMT_YUV422P9:
    case AV_PIX_FMT_YUV420P9:
    case AV_PIX_FMT_YUVA444P9:
    case AV_PIX_FMT_YUVA422P9:
    case AV_PIX_FMT_YUVA420P9:
    case AV_PIX_FMT_GRAY10:
    case AV_PIX_FMT_YUV444P10:
    case AV_PIX_FMT_YUV440P10:
    case AV_PIX_FMT_YUV420P10:
    case AV_PIX_FMT_YUV422P10:
    case AV_PIX_FMT_YUVA444P10:
    case AV_PIX_FMT_YUVA422P10:
    case AV_PIX_FMT_YUVA420P10:
    case AV_PIX_FMT_GRAY12:
    case AV_PIX_FMT_YUV444P12:
    case AV_PIX_FMT_YUV440P12:
    case AV_PIX_FMT_YUV420P12:
    case AV_PIX_FMT_YUV422P12:
    case AV_PIX_FMT_YUV444P14:
    case AV_PIX_FMT_YUV420P14:
    case AV_PIX_FMT_YUV422P14:
    case AV_PIX_FMT_GRAY16:
    case AV_PIX_FMT_YUV444P16:
    case AV_PIX_FMT_YUV422P16:
    case AV_PIX_FMT_YUV420P16:
    case AV_PIX_FMT_YUVA444P16:
    case AV_PIX_FMT_YUVA422P16:
    case AV_PIX_FMT_YUVA420P16:
    case AV_PIX_FMT_RGB48:
    case AV_PIX_FMT_RGBA64:
    case AV_PIX_FMT_GBRP9:
    case AV_PIX_FMT_GBRP10:
    case AV_PIX_FMT_GBRAP10:
    case AV_PIX_FMT_GBRP12:
    case AV_PIX_FMT_GBRAP12:
    case AV_PIX_FMT_GBRP14:
    case AV_PIX_FMT_GBRP16:
    case AV_PIX_FMT_GBRAP16:
      if(codec_config.level<1) return false;
    }
    return CodecBase::test_av_format(format);
  }

  virtual int compress_input_info(VDXPixmapLayout* src){
    switch(src->format){
    case nsVDXPixmap::kPixFormat_RGB888:
    case nsVDXPixmap::kPixFormat_XRGB8888:
    case nsVDXPixmap::kPixFormat_XRGB64:
    case nsVDXPixmap::kPixFormat_YUV420_Planar:
    case nsVDXPixmap::kPixFormat_YUV422_Planar:
    case nsVDXPixmap::kPixFormat_YUV444_Planar:
    case nsVDXPixmap::kPixFormat_YUV420_Planar16:
    case nsVDXPixmap::kPixFormat_YUV422_Planar16:
    case nsVDXPixmap::kPixFormat_YUV444_Planar16:
    case nsVDXPixmap::kPixFormat_YUV420_Alpha_Planar:
    case nsVDXPixmap::kPixFormat_YUV422_Alpha_Planar:
    case nsVDXPixmap::kPixFormat_YUV444_Alpha_Planar:
    case nsVDXPixmap::kPixFormat_YUV420_Alpha_Planar16:
    case nsVDXPixmap::kPixFormat_YUV422_Alpha_Planar16:
    case nsVDXPixmap::kPixFormat_YUV444_Alpha_Planar16:
    case nsVDXPixmap::kPixFormat_Y8:
    case nsVDXPixmap::kPixFormat_Y16:
      return 1;
    }
    return 0;
  }

  void getinfo(ICINFO& info){
    info.fccHandler = codec_tag;
    info.dwFlags = VIDCF_COMPRESSFRAMES | VIDCF_FASTTEMPORALC;
    wcscpy(info.szName, L"FFV1");
    wcscpy(info.szDescription, L"FFMPEG FFV1 lossless codec");
  }

  bool init_ctx(VDXPixmapLayout* layout)
  {
    ctx->strict_std_compliance = -2;
    ctx->level = codec_config.level;
    ctx->thread_count = 0;
    ctx->slices = codec_config.slice;
    av_opt_set_int(ctx->priv_data, "slicecrc", codec_config.slicecrc, 0);
    av_opt_set_int(ctx->priv_data, "context", codec_config.context, 0);
    av_opt_set_int(ctx->priv_data, "coder", codec_config.coder, 0);
    return true;
  }

  LRESULT configure(HWND parent)
  {
    ConfigFFV1 dlg;
    dlg.Show(parent,this);
    return ICERR_OK;
  }
};

void ConfigFFV1::apply_level()
{
  CodecFFV1::Config* config = (CodecFFV1::Config*)codec->config;

  EnableWindow(GetDlgItem(mhdlg,IDC_SLICES), config->level>=3);
  EnableWindow(GetDlgItem(mhdlg,IDC_SLICECRC), config->level>=3);
  if(config->level<3){
    config->slice = 0;
    config->slicecrc = 0;
  }
  init_slices();
  adjust_bits();
  init_bits();
  change_bits();
}

void ConfigFFV1::init_slices()
{
  CodecFFV1::Config* config = (CodecFFV1::Config*)codec->config;
  int x = 0;
  {for(int i=1; i<sizeof(ffv1_slice_tab)/sizeof(int); i++)
    if(ffv1_slice_tab[i]==config->slice) x = i; }
  SendDlgItemMessage(mhdlg, IDC_SLICES, CB_SETCURSEL, x, 0);
  CheckDlgButton(mhdlg, IDC_SLICECRC, config->slicecrc==1 ? BST_CHECKED:BST_UNCHECKED);
}

void ConfigFFV1::init_coder()
{
  CodecFFV1::Config* config = (CodecFFV1::Config*)codec->config;
  CheckDlgButton(mhdlg, IDC_CODER0, config->coder==0 ? BST_CHECKED:BST_UNCHECKED);
  CheckDlgButton(mhdlg, IDC_CODER1, config->coder==1 ? BST_CHECKED:BST_UNCHECKED);
}

void ConfigFFV1::change_bits()
{
  CodecFFV1::Config* config = (CodecFFV1::Config*)codec->config;
  EnableWindow(GetDlgItem(mhdlg,IDC_CODER0), config->bits==8);
  if(config->bits>8) config->coder=1;
  init_coder();
  ConfigBase::change_bits();
}

INT_PTR ConfigFFV1::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
  CodecFFV1::Config* config = (CodecFFV1::Config*)codec->config;

  switch(msg){
  case WM_INITDIALOG:
    {
      const char* v_names[] = {
        "FFV1.0", 
        "FFV1.1",
        "FFV1.3",
      };
      SendDlgItemMessage(mhdlg, IDC_LEVEL, CB_RESETCONTENT, 0, 0);
      for(int i=0; i<3; i++)
        SendDlgItemMessage(mhdlg, IDC_LEVEL, CB_ADDSTRING, 0, (LPARAM)v_names[i]);
      int x = 0;
      if(config->level==1) x = 1;
      if(config->level==3) x = 2;
      SendDlgItemMessage(mhdlg, IDC_LEVEL, CB_SETCURSEL, x, 0);

      SendDlgItemMessage(mhdlg, IDC_SLICES, CB_RESETCONTENT, 0, 0);
      SendDlgItemMessage(mhdlg, IDC_SLICES, CB_ADDSTRING, 0, (LPARAM)"default");
      {for(int i=1; i<sizeof(ffv1_slice_tab)/sizeof(int); i++){
        char buf[10];
        sprintf(buf,"%d",ffv1_slice_tab[i]);
        SendDlgItemMessage(mhdlg, IDC_SLICES, CB_ADDSTRING, 0, (LPARAM)buf);
      }}
      CheckDlgButton(mhdlg, IDC_CONTEXT, config->context==1 ? BST_CHECKED:BST_UNCHECKED);
      apply_level();
      break;
    }

  case WM_COMMAND:
    switch(LOWORD(wParam)){
    case IDC_LEVEL:
      if(HIWORD(wParam)==LBN_SELCHANGE){
        int x = (int)SendDlgItemMessage(mhdlg, IDC_LEVEL, CB_GETCURSEL, 0, 0);
        config->level = 0;
        if(x==1) config->level = 1;
        if(x==2) config->level = 3;
        apply_level();
        return TRUE;
      }
      break;
    case IDC_SLICES:
      if(HIWORD(wParam)==LBN_SELCHANGE){
        int x = (int)SendDlgItemMessage(mhdlg, IDC_SLICES, CB_GETCURSEL, 0, 0);
        config->slice = ffv1_slice_tab[x];
        return TRUE;
      }
      break;
    case IDC_SLICECRC:
      config->slicecrc = IsDlgButtonChecked(mhdlg,IDC_SLICECRC) ? 1:0;
      break;
    case IDC_CONTEXT:
      config->context = IsDlgButtonChecked(mhdlg,IDC_CONTEXT) ? 1:0;
      break;
    case IDC_CODER0:
    case IDC_CODER1:
      config->coder = IsDlgButtonChecked(mhdlg,IDC_CODER0) ? 0:1;
      break;
    }
  }
  return ConfigBase::DlgProc(msg,wParam,lParam);
}

//---------------------------------------------------------------------------

class ConfigHUFF: public ConfigBase{
public:
  ConfigHUFF(){ dialog_id = IDD_ENC_FFVHUFF; idc_message = IDC_MESSAGE; }
  INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
};

struct CodecHUFF: public CodecBase{
  enum{ tag=MKTAG('F', 'F', 'V', 'H') };
  struct Config: public CodecBase::Config{
    int prediction;

    Config(){ prediction=0; }
    void clear(){ CodecBase::Config::clear(); prediction=0; }
  } codec_config;

  CodecHUFF(){
    config = &codec_config;
    codec_id = AV_CODEC_ID_FFVHUFF;
    codec_tag = tag;
  }

  int config_size(){ return sizeof(Config); }
  void reset_config(){ codec_config.clear(); }

  void getinfo(ICINFO& info){
    info.fccHandler = codec_tag;
    info.dwFlags = VIDCF_COMPRESSFRAMES;
    wcscpy(info.szName, L"FFVHUFF");
    wcscpy(info.szDescription, L"FFMPEG Huffyuv lossless codec");
  }

  virtual int compress_input_info(VDXPixmapLayout* src){
    switch(src->format){
    case nsVDXPixmap::kPixFormat_RGB888:
    case nsVDXPixmap::kPixFormat_XRGB8888:
    case nsVDXPixmap::kPixFormat_XRGB64:
    case nsVDXPixmap::kPixFormat_YUV420_Planar:
    case nsVDXPixmap::kPixFormat_YUV422_Planar:
    case nsVDXPixmap::kPixFormat_YUV444_Planar:
    case nsVDXPixmap::kPixFormat_YUV420_Planar16:
    case nsVDXPixmap::kPixFormat_YUV422_Planar16:
    case nsVDXPixmap::kPixFormat_YUV444_Planar16:
    case nsVDXPixmap::kPixFormat_YUV420_Alpha_Planar:
    case nsVDXPixmap::kPixFormat_YUV422_Alpha_Planar:
    case nsVDXPixmap::kPixFormat_YUV444_Alpha_Planar:
    case nsVDXPixmap::kPixFormat_YUV420_Alpha_Planar16:
    case nsVDXPixmap::kPixFormat_YUV422_Alpha_Planar16:
    case nsVDXPixmap::kPixFormat_YUV444_Alpha_Planar16:
    case nsVDXPixmap::kPixFormat_Y8:
    case nsVDXPixmap::kPixFormat_Y16:
      return 1;
    }
    return 0;
  }

  bool init_ctx(VDXPixmapLayout* layout)
  {
    ctx->thread_count = 0;
    int pred = codec_config.prediction;
    if(pred==2 && config->format==format_rgb && config->bits==8) pred = 0;
    av_opt_set_int(ctx->priv_data, "pred", pred, 0);
    return true;
  }

  LRESULT configure(HWND parent)
  {
    ConfigHUFF dlg;
    dlg.Show(parent,this);
    return ICERR_OK;
  }
};

INT_PTR ConfigHUFF::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch(msg){
  case WM_INITDIALOG:
    {
      const char* pred_names[] = {
        "left", 
        "plane",
        "median",
      };

      SendDlgItemMessage(mhdlg, IDC_PREDICTION, CB_RESETCONTENT, 0, 0);
      for(int i=0; i<3; i++)
        SendDlgItemMessage(mhdlg, IDC_PREDICTION, CB_ADDSTRING, 0, (LPARAM)pred_names[i]);
      CodecHUFF::Config* config = (CodecHUFF::Config*)codec->config;
      SendDlgItemMessage(mhdlg, IDC_PREDICTION, CB_SETCURSEL, config->prediction, 0);
      break;
    }

  case WM_COMMAND:
    switch(LOWORD(wParam)){
    case IDC_PREDICTION:
      if(HIWORD(wParam)==LBN_SELCHANGE){
        CodecHUFF::Config* config = (CodecHUFF::Config*)codec->config;
        config->prediction = (int)SendDlgItemMessage(mhdlg, IDC_PREDICTION, CB_GETCURSEL, 0, 0);
        return TRUE;
      }
      break;
    }
  }
  return ConfigBase::DlgProc(msg,wParam,lParam);
}

//---------------------------------------------------------------------------

class ConfigProres: public ConfigBase{
public:
  ConfigProres(){ dialog_id = IDD_ENC_PRORES; }
  INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
  virtual void init_format();
  virtual void change_format(int sel);
  void init_profile();
};

enum {
    PRORES_PROFILE_AUTO  = -1,
    PRORES_PROFILE_PROXY = 0,
    PRORES_PROFILE_LT,
    PRORES_PROFILE_STANDARD,
    PRORES_PROFILE_HQ,
    PRORES_PROFILE_4444,
};

struct CodecProres: public CodecBase{
  enum{ tag=MKTAG('a', 'p', 'c', 'h') };
  struct Config: public CodecBase::Config{
    int profile;
    int qscale; // 2-31

    Config(){ set_default(); }
    void clear(){ CodecBase::Config::clear(); set_default(); }
    void set_default(){
      profile = PRORES_PROFILE_HQ;
      qscale = 4;
      format = format_yuv422;
      bits = 10;
    }
  } codec_config;

  CodecProres(){
    config = &codec_config;
    codec_name = "prores_ks";
    codec_tag = tag;
  }

  int config_size(){ return sizeof(Config); }
  void reset_config(){ codec_config.clear(); }

  virtual int compress_input_info(VDXPixmapLayout* src){
    switch(src->format){
    case nsVDXPixmap::kPixFormat_YUV422_Planar16:
    case nsVDXPixmap::kPixFormat_YUV444_Planar16:
    case nsVDXPixmap::kPixFormat_YUV444_Alpha_Planar16:
      return 1;
    }
    return 0;
  }

  void getinfo(ICINFO& info){
    info.fccHandler = codec_tag;
    info.dwFlags = VIDCF_COMPRESSFRAMES;
    wcscpy(info.szName, L"prores_ks");
    wcscpy(info.szDescription, L"FFMPEG / Apple ProRes (iCodec Pro)");
  }

  bool init_ctx(VDXPixmapLayout* layout)
  {
    ctx->thread_count = 0;
    av_opt_set_int(ctx->priv_data, "profile", codec_config.profile, 0);
    if(codec_config.format==format_yuva444){
      av_opt_set_int(ctx->priv_data, "alpha_bits", 16, 0);
    }
    ctx->flags |= AV_CODEC_FLAG_QSCALE;
    ctx->global_quality = FF_QP2LAMBDA * codec_config.qscale;
    return true;
  }

  LRESULT configure(HWND parent)
  {
    ConfigProres dlg;
    dlg.Show(parent,this);
    return ICERR_OK;
  }
};

INT_PTR ConfigProres::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch(msg){
  case WM_INITDIALOG:
    {
      init_profile();
      CodecProres::Config* config = (CodecProres::Config*)codec->config;
      SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_SETRANGEMIN, FALSE, 2);
      SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_SETRANGEMAX, TRUE, 31);
      SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_SETPOS, TRUE, config->qscale);
      SetDlgItemInt(mhdlg, IDC_QUALITY_VALUE, config->qscale, false);
      break;
    }

  case WM_HSCROLL:
    if((HWND)lParam==GetDlgItem(mhdlg,IDC_QUALITY)){
      CodecProres::Config* config = (CodecProres::Config*)codec->config;
      config->qscale = (int)SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_GETPOS, 0, 0);
      SetDlgItemInt(mhdlg, IDC_QUALITY_VALUE, config->qscale, false);
      break;
    }
    return false;

  case WM_COMMAND:
    switch(LOWORD(wParam)){
    case IDC_PROFILE:
      if(HIWORD(wParam)==LBN_SELCHANGE){
        CodecProres::Config* config = (CodecProres::Config*)codec->config;
        int v = (int)SendDlgItemMessage(mhdlg, IDC_PROFILE, CB_GETCURSEL, 0, 0);
        if(config->format==CodecBase::format_yuva444){
          config->profile = v+4;
        } else {
          config->profile = v;
        }
        return TRUE;
      }
      break;
    }
  }
  return ConfigBase::DlgProc(msg,wParam,lParam);
}

void ConfigProres::init_profile()
{
  CodecProres::Config* config = (CodecProres::Config*)codec->config;
  if(config->format==CodecBase::format_yuva444){
    if(config->profile<4) config->profile=4;

    const char* profile_names[] = {
      "4444",
      "4444XQ",
    };

    SendDlgItemMessage(mhdlg, IDC_PROFILE, CB_RESETCONTENT, 0, 0);
    for(int i=0; i<2; i++)
      SendDlgItemMessage(mhdlg, IDC_PROFILE, CB_ADDSTRING, 0, (LPARAM)profile_names[i]);
    SendDlgItemMessage(mhdlg, IDC_PROFILE, CB_SETCURSEL, config->profile-4, 0);

  } else {
    const char* profile_names[] = {
      "proxy", 
      "lt",
      "standard",
      "high quality",
      "4444",
      "4444XQ",
    };

    SendDlgItemMessage(mhdlg, IDC_PROFILE, CB_RESETCONTENT, 0, 0);
    for(int i=0; i<6; i++)
      SendDlgItemMessage(mhdlg, IDC_PROFILE, CB_ADDSTRING, 0, (LPARAM)profile_names[i]);
    SendDlgItemMessage(mhdlg, IDC_PROFILE, CB_SETCURSEL, config->profile, 0);
  }
}

void ConfigProres::init_format()
{
  const char* color_names[] = {
    "YUV 4:2:2",
    "YUV 4:4:4",
    "YUV 4:4:4 + Alpha",
  };

  SendDlgItemMessage(mhdlg, IDC_COLORSPACE, CB_RESETCONTENT, 0, 0);
  for(int i=0; i<3; i++)
    SendDlgItemMessage(mhdlg, IDC_COLORSPACE, CB_ADDSTRING, 0, (LPARAM)color_names[i]);
  int sel = 0;
  if(codec->config->format==CodecBase::format_yuv444) sel = 1;
  if(codec->config->format==CodecBase::format_yuva444) sel = 2;
  SendDlgItemMessage(mhdlg, IDC_COLORSPACE, CB_SETCURSEL, sel, 0);
}

void ConfigProres::change_format(int sel)
{
  int format = CodecBase::format_yuv422;
  if(sel==1) format = CodecBase::format_yuv444;
  if(sel==2) format = CodecBase::format_yuva444;
  codec->config->format = format;
  init_profile();
}

//---------------------------------------------------------------------------

class ConfigH264: public ConfigBase{
public:
  ConfigH264(){ dialog_id = IDD_ENC_PRORES; }
  INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
};

const char* x264_preset_names[] = {
  "fast", 
  "medium",
  "slow",
};

struct CodecH264: public CodecBase{
  enum{ tag=MKTAG('H', '2', '6', '4') };
  struct Config: public CodecBase::Config{
    int preset;
    int crf; // 0-51

    Config(){ set_default(); }
    void clear(){ CodecBase::Config::clear(); set_default(); }
    void set_default(){
      preset = 1;
      crf = 23;
      format = format_yuv420;
      bits = 8;
    }
  } codec_config;

  CodecH264(){
    config = &codec_config;
    codec_name = "libx264";
    codec_tag = tag;
  }

  int config_size(){ return sizeof(Config); }
  void reset_config(){ codec_config.clear(); }

  void getinfo(ICINFO& info){
    info.fccHandler = codec_tag;
    info.dwFlags = VIDCF_COMPRESSFRAMES | VIDCF_FASTTEMPORALC;
    wcscpy(info.szName, L"x264_test");
    wcscpy(info.szDescription, L"FFMPEG / x264");
  }

  bool init_ctx(VDXPixmapLayout* layout)
  {
    ctx->thread_count = 0;
    ctx->gop_size = -1;
    ctx->max_b_frames = -1;
    ctx->bit_rate = -1;

    av_opt_set(ctx->priv_data, "preset", x264_preset_names[codec_config.preset], 0);
    av_opt_set_double(ctx->priv_data, "crf", codec_config.crf, 0);
    av_opt_set(ctx->priv_data, "level", "3", 0);
    return true;
  }

  LRESULT configure(HWND parent)
  {
    ConfigH264 dlg;
    dlg.Show(parent,this);
    return ICERR_OK;
  }
};

INT_PTR ConfigH264::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
  CodecH264::Config* config = (CodecH264::Config*)codec->config;
  switch(msg){
  case WM_INITDIALOG:
    {
      SendDlgItemMessage(mhdlg, IDC_PROFILE, CB_RESETCONTENT, 0, 0);
      for(int i=0; i<3; i++)
        SendDlgItemMessage(mhdlg, IDC_PROFILE, CB_ADDSTRING, 0, (LPARAM)x264_preset_names[i]);
      SendDlgItemMessage(mhdlg, IDC_PROFILE, CB_SETCURSEL, config->preset, 0);
      SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_SETRANGEMIN, FALSE, 0);
      SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_SETRANGEMAX, TRUE, 51);
      SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_SETPOS, TRUE, config->crf);
      SetDlgItemInt(mhdlg, IDC_QUALITY_VALUE, config->crf, false);
      break;
    }

  case WM_HSCROLL:
    if((HWND)lParam==GetDlgItem(mhdlg,IDC_QUALITY)){
      config->crf = (int)SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_GETPOS, 0, 0);
      SetDlgItemInt(mhdlg, IDC_QUALITY_VALUE, config->crf, false);
      break;
    }
    return false;

  case WM_COMMAND:
    switch(LOWORD(wParam)){
    case IDC_PROFILE:
      if(HIWORD(wParam)==LBN_SELCHANGE){
        config->preset = (int)SendDlgItemMessage(mhdlg, IDC_PROFILE, CB_GETCURSEL, 0, 0);
        return TRUE;
      }
      break;
    }
  }
  return ConfigBase::DlgProc(msg,wParam,lParam);
}

//---------------------------------------------------------------------------

class ConfigH265: public ConfigBase{
public:
  ConfigH265(){ dialog_id = IDD_ENC_X265; }
  INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
  virtual void init_format();
  virtual void change_format(int sel);
};

const char* x265_preset_names[] = {
  "ultrafast",
  "superfast",
  "veryfast",
  "faster",
  "fast", 
  "medium",
  "slow",
  "slower",
  "veryslow",
  "placebo",
};

const char* x265_tune_names[] = {
  "none",
  "psnr",
  "ssim",
  "grain",
  "fastdecode",
  "zerolatency", 
};

struct CodecH265: public CodecBase{
  enum{ id_tag=MKTAG('h', 'e', 'v', '1') };
  struct Config: public CodecBase::Config{
    int preset;
    int crf; // 0-51
    int tune;
    int flags; // reserved

    Config(){ set_default(); }
    void clear(){ CodecBase::Config::clear(); set_default(); }
    void set_default(){
      version = 1;
      preset = 4;
      crf = 28;
      format = format_yuv420;
      bits = 8;
      tune = 0;
      flags = 0;
    }
  } codec_config;

  CodecH265(){
    config = &codec_config;
    codec_name = "libx265";
    codec_tag = MKTAG('H', 'E', 'V', 'C');
  }

  int config_size(){ return sizeof(Config); }
  void reset_config(){ codec_config.clear(); }

  virtual bool load_config(void* data, size_t size){
    if(size<4) return false;
    int src_version = *(int*)data;
    if(src_version>1) return false;

    codec_config.set_default();
    memcpy(&codec_config, data, size);
    return true;
  }

  void getinfo(ICINFO& info){
    info.fccHandler = id_tag;
    info.dwFlags = VIDCF_COMPRESSFRAMES | VIDCF_FASTTEMPORALC;
    wcscpy(info.szName, L"x265");
    wcscpy(info.szDescription, L"FFMPEG / x265");
  }

  virtual int compress_input_info(VDXPixmapLayout* src){
    switch(src->format){
    case nsVDXPixmap::kPixFormat_RGB888:
    case nsVDXPixmap::kPixFormat_XRGB64:
    case nsVDXPixmap::kPixFormat_YUV420_Planar:
    case nsVDXPixmap::kPixFormat_YUV422_Planar:
    case nsVDXPixmap::kPixFormat_YUV444_Planar:
    case nsVDXPixmap::kPixFormat_YUV420_Planar16:
    case nsVDXPixmap::kPixFormat_YUV422_Planar16:
    case nsVDXPixmap::kPixFormat_YUV444_Planar16:
      return 1;
    }
    return 0;
  }

  bool init_ctx(VDXPixmapLayout* layout)
  {
    ctx->thread_count = 0;
    ctx->gop_size = -1;
    ctx->max_b_frames = -1;
    ctx->bit_rate = -1;

    av_opt_set(ctx->priv_data, "preset", x265_preset_names[codec_config.preset], 0);
    if(codec_config.tune) av_opt_set(ctx->priv_data, "tune", x265_tune_names[codec_config.tune], 0);
    av_opt_set_double(ctx->priv_data, "crf", codec_config.crf, 0);
    return true;
  }

  LRESULT configure(HWND parent)
  {
    ConfigH265 dlg;
    dlg.Show(parent,this);
    return ICERR_OK;
  }
};

struct CodecH265LS: public CodecH265{
  enum{ id_tag=MKTAG('h', 'e', 'v', '0') };
  void reset_config(){ codec_config.clear(); codec_config.crf=0; }

  bool init_ctx(VDXPixmapLayout* layout)
  {
    if(!CodecH265::init_ctx(layout)) return false;
    av_opt_set(ctx->priv_data, "x265-params", "lossless=1", 0);
    return true;
  }

  void getinfo(ICINFO& info){
    info.fccHandler = id_tag;
    info.dwFlags = VIDCF_COMPRESSFRAMES | VIDCF_FASTTEMPORALC;
    wcscpy(info.szName, L"x265 lossless");
    wcscpy(info.szDescription, L"FFMPEG / x265 lossless");
  }

  LRESULT configure(HWND parent)
  {
    ConfigH265 dlg;
    dlg.dialog_id = IDD_ENC_X265LS;
    dlg.Show(parent,this);
    return ICERR_OK;
  }
};

INT_PTR ConfigH265::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
  CodecH265::Config* config = (CodecH265::Config*)codec->config;
  switch(msg){
  case WM_INITDIALOG:
    {
      SendDlgItemMessage(mhdlg, IDC_PROFILE, CB_RESETCONTENT, 0, 0);
      for(int i=0; i<10; i++)
        SendDlgItemMessage(mhdlg, IDC_PROFILE, CB_ADDSTRING, 0, (LPARAM)x265_preset_names[i]);
      SendDlgItemMessage(mhdlg, IDC_PROFILE, CB_SETCURSEL, config->preset, 0);

      SendDlgItemMessage(mhdlg, IDC_TUNE, CB_RESETCONTENT, 0, 0);
      for(int i=0; i<6; i++)
        SendDlgItemMessage(mhdlg, IDC_TUNE, CB_ADDSTRING, 0, (LPARAM)x265_tune_names[i]);
      SendDlgItemMessage(mhdlg, IDC_TUNE, CB_SETCURSEL, config->tune, 0);

      SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_SETRANGEMIN, FALSE, 0);
      SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_SETRANGEMAX, TRUE, 51);
      SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_SETPOS, TRUE, config->crf);
      SetDlgItemInt(mhdlg, IDC_QUALITY_VALUE, config->crf, false);
      break;
    }

  case WM_HSCROLL:
    if((HWND)lParam==GetDlgItem(mhdlg,IDC_QUALITY)){
      config->crf = (int)SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_GETPOS, 0, 0);
      SetDlgItemInt(mhdlg, IDC_QUALITY_VALUE, config->crf, false);
      break;
    }
    return false;

  case WM_COMMAND:
    switch(LOWORD(wParam)){
    case IDC_PROFILE:
      if(HIWORD(wParam)==LBN_SELCHANGE){
        config->preset = (int)SendDlgItemMessage(mhdlg, IDC_PROFILE, CB_GETCURSEL, 0, 0);
        return TRUE;
      }
      break;
    case IDC_TUNE:
      if(HIWORD(wParam)==LBN_SELCHANGE){
        config->tune = (int)SendDlgItemMessage(mhdlg, IDC_TUNE, CB_GETCURSEL, 0, 0);
        return TRUE;
      }
      break;
    }
  }
  return ConfigBase::DlgProc(msg,wParam,lParam);
}

void ConfigH265::init_format()
{
  const char* color_names[] = {
    "RGB",
    "YUV 4:2:0",
    "YUV 4:2:2",
    "YUV 4:4:4",
  };

  SendDlgItemMessage(mhdlg, IDC_COLORSPACE, CB_RESETCONTENT, 0, 0);
  for(int i=0; i<4; i++)
    SendDlgItemMessage(mhdlg, IDC_COLORSPACE, CB_ADDSTRING, 0, (LPARAM)color_names[i]);
  int sel = 0;
  if(codec->config->format==CodecBase::format_yuv420) sel = 1;
  if(codec->config->format==CodecBase::format_yuv422) sel = 2;
  if(codec->config->format==CodecBase::format_yuv444) sel = 3;
  SendDlgItemMessage(mhdlg, IDC_COLORSPACE, CB_SETCURSEL, sel, 0);
}

void ConfigH265::change_format(int sel)
{
  int format = CodecBase::format_rgb;
  if(sel==1) format = CodecBase::format_yuv420;
  if(sel==2) format = CodecBase::format_yuv422;
  if(sel==3) format = CodecBase::format_yuv444;
  codec->config->format = format;
  init_bits();
}

//---------------------------------------------------------------------------

class ConfigVP8: public ConfigBase{
public:
  ConfigVP8(){ dialog_id = IDD_ENC_VP8; }
  INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
  virtual void init_format();
  virtual void change_format(int sel);
};

struct CodecVP8: public CodecBase{
  enum{ tag=MKTAG('V', 'P', '8', '0') };
  struct Config: public CodecBase::Config{
    int crf; // 4-63

    Config(){ set_default(); }
    void clear(){ CodecBase::Config::clear(); set_default(); }
    void set_default(){
      crf = 10;
      format = format_yuv420;
      bits = 8;
    }
  } codec_config;

  CodecVP8(){
    config = &codec_config;
    codec_name = "libvpx";
    codec_tag = tag;
  }

  int config_size(){ return sizeof(Config); }
  void reset_config(){ codec_config.clear(); }

  virtual int compress_input_info(VDXPixmapLayout* src){
    switch(src->format){
    case nsVDXPixmap::kPixFormat_YUV420_Planar:
    case nsVDXPixmap::kPixFormat_YUV420_Alpha_Planar:
      return 1;
    }
    return 0;
  }

  void getinfo(ICINFO& info){
    info.fccHandler = codec_tag;
    info.dwFlags = VIDCF_COMPRESSFRAMES | VIDCF_FASTTEMPORALC;
    wcscpy(info.szName, L"vp8");
    wcscpy(info.szDescription, L"FFMPEG / VP8");
  }

  bool init_ctx(VDXPixmapLayout* layout)
  {
    ctx->thread_count = 0;
    ctx->gop_size = -1;
    ctx->max_b_frames = -1;
    ctx->bit_rate = 0x400000000000;

    av_opt_set_double(ctx->priv_data, "crf", codec_config.crf, 0);
    av_opt_set_int(ctx->priv_data, "max-intra-rate", 0, 0);
    if(codec_config.format==format_yuva420)
      av_opt_set_int(ctx->priv_data, "auto-alt-ref", 0, 0);
    ctx->qmin = codec_config.crf;
    ctx->qmax = codec_config.crf;
    return true;
  }

  LRESULT configure(HWND parent)
  {
    ConfigVP8 dlg;
    dlg.Show(parent,this);
    return ICERR_OK;
  }
};

INT_PTR ConfigVP8::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
  CodecVP8::Config* config = (CodecVP8::Config*)codec->config;
  switch(msg){
  case WM_INITDIALOG:
    {
      SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_SETRANGEMIN, FALSE, 4);
      SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_SETRANGEMAX, TRUE, 63);
      SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_SETPOS, TRUE, config->crf);
      SetDlgItemInt(mhdlg, IDC_QUALITY_VALUE, config->crf, false);
      break;
    }

  case WM_HSCROLL:
    if((HWND)lParam==GetDlgItem(mhdlg,IDC_QUALITY)){
      config->crf = (int)SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_GETPOS, 0, 0);
      SetDlgItemInt(mhdlg, IDC_QUALITY_VALUE, config->crf, false);
      break;
    }
    return false;
  }
  return ConfigBase::DlgProc(msg,wParam,lParam);
}

void ConfigVP8::init_format()
{
  const char* color_names[] = {
    "YUV 4:2:0",
    "YUV 4:2:0 + Alpha",
  };

  SendDlgItemMessage(mhdlg, IDC_COLORSPACE, CB_RESETCONTENT, 0, 0);
  for(int i=0; i<2; i++)
    SendDlgItemMessage(mhdlg, IDC_COLORSPACE, CB_ADDSTRING, 0, (LPARAM)color_names[i]);
  int sel = 0;
  if(codec->config->format==CodecBase::format_yuva420) sel = 1;
  SendDlgItemMessage(mhdlg, IDC_COLORSPACE, CB_SETCURSEL, sel, 0);
  EnableWindow(GetDlgItem(mhdlg, IDC_COLORSPACE),false); //! need to pass side_data
}

void ConfigVP8::change_format(int sel)
{
  int format = CodecBase::format_yuv420;
  if(sel==1) format = CodecBase::format_yuva420;
  codec->config->format = format;
}

//---------------------------------------------------------------------------

class ConfigVP9: public ConfigBase{
public:
  ConfigVP9(){ dialog_id = IDD_ENC_VP9; }
  INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
  virtual void init_format();
  virtual void change_format(int sel);
};

struct CodecVP9: public CodecBase{
  enum{ tag=MKTAG('V', 'P', '9', '0') };
  struct Config: public CodecBase::Config{
    int crf; // 0-63
    int profile;

    Config(){ set_default(); }
    void clear(){ CodecBase::Config::clear(); set_default(); }
    void set_default(){
      crf = 15;
      format = format_yuv420;
      bits = 8;
    }
  } codec_config;

  CodecVP9(){
    config = &codec_config;
    codec_name = "libvpx-vp9";
    codec_tag = tag;
  }

  int config_size(){ return sizeof(Config); }
  void reset_config(){ codec_config.clear(); }

  virtual int compress_input_info(VDXPixmapLayout* src){
    switch(src->format){
    case nsVDXPixmap::kPixFormat_YUV420_Planar:
    case nsVDXPixmap::kPixFormat_YUV422_Planar:
    case nsVDXPixmap::kPixFormat_YUV444_Planar:
    case nsVDXPixmap::kPixFormat_YUV420_Planar16:
    case nsVDXPixmap::kPixFormat_YUV422_Planar16:
    case nsVDXPixmap::kPixFormat_YUV444_Planar16:
    case nsVDXPixmap::kPixFormat_XRGB8888:
    case nsVDXPixmap::kPixFormat_XRGB64:
      return 1;
    }
    return 0;
  }

  void getinfo(ICINFO& info){
    info.fccHandler = codec_tag;
    info.dwFlags = VIDCF_COMPRESSFRAMES | VIDCF_FASTTEMPORALC;
    wcscpy(info.szName, L"vp9");
    wcscpy(info.szDescription, L"FFMPEG / VP9");
  }

  bool init_ctx(VDXPixmapLayout* layout)
  {
    ctx->thread_count = 0;
    ctx->gop_size = -1;
    ctx->max_b_frames = -1;
    ctx->bit_rate = 0;

    av_opt_set_double(ctx->priv_data, "crf", codec_config.crf, 0);
    av_opt_set_int(ctx->priv_data, "max-intra-rate", 0, 0);
    ctx->qmin = codec_config.crf;
    ctx->qmax = codec_config.crf;
    return true;
  }

  LRESULT configure(HWND parent)
  {
    ConfigVP9 dlg;
    dlg.Show(parent,this);
    return ICERR_OK;
  }
};

INT_PTR ConfigVP9::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
  CodecVP9::Config* config = (CodecVP9::Config*)codec->config;
  switch(msg){
  case WM_INITDIALOG:
    {
      SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_SETRANGEMIN, FALSE, 0);
      SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_SETRANGEMAX, TRUE, 63);
      SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_SETPOS, TRUE, config->crf);
      SetDlgItemInt(mhdlg, IDC_QUALITY_VALUE, config->crf, false);
      break;
    }

  case WM_HSCROLL:
    if((HWND)lParam==GetDlgItem(mhdlg,IDC_QUALITY)){
      config->crf = (int)SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_GETPOS, 0, 0);
      SetDlgItemInt(mhdlg, IDC_QUALITY_VALUE, config->crf, false);
      break;
    }
    return false;
  }
  return ConfigBase::DlgProc(msg,wParam,lParam);
}

void ConfigVP9::init_format()
{
  const char* color_names[] = {
    "RGB",
    "YUV 4:2:0",
    "YUV 4:2:2",
    "YUV 4:4:4",
  };

  SendDlgItemMessage(mhdlg, IDC_COLORSPACE, CB_RESETCONTENT, 0, 0);
  for(int i=0; i<4; i++)
    SendDlgItemMessage(mhdlg, IDC_COLORSPACE, CB_ADDSTRING, 0, (LPARAM)color_names[i]);
  int sel = 0;
  if(codec->config->format==CodecBase::format_yuv420) sel = 1;
  if(codec->config->format==CodecBase::format_yuv422) sel = 2;
  if(codec->config->format==CodecBase::format_yuv444) sel = 3;
  SendDlgItemMessage(mhdlg, IDC_COLORSPACE, CB_SETCURSEL, sel, 0);
}

void ConfigVP9::change_format(int sel)
{
  int format = CodecBase::format_rgb;
  if(sel==1) format = CodecBase::format_yuv420;
  if(sel==2) format = CodecBase::format_yuv422;
  if(sel==3) format = CodecBase::format_yuv444;
  codec->config->format = format;
  init_bits();
}

//---------------------------------------------------------------------------

extern "C" LRESULT WINAPI DriverProc(DWORD_PTR dwDriverId, HDRVR hDriver, UINT uMsg, LPARAM lParam1, LPARAM lParam2)
{
  CodecClass* cs = (CodecClass*)dwDriverId;
  if(cs && cs->class_id==1) return DriverProc_CF(dwDriverId, hDriver, uMsg, lParam1, lParam2);

  CodecBase* codec = (CodecBase*)dwDriverId;

  switch (uMsg){
  case DRV_LOAD:
    init_av();
    return DRV_OK;

  case DRV_FREE:
    return DRV_OK;

  case DRV_OPEN:
    {
      ICOPEN* icopen = (ICOPEN*)lParam2;
      if(icopen && icopen->fccType != ICTYPE_VIDEO) return 0;

      CodecBase* codec = 0;
      if(icopen->fccHandler==0) codec = new CodecFFV1;
      if(icopen->fccHandler==CodecFFV1::tag) codec = new CodecFFV1;
      if(icopen->fccHandler==CodecHUFF::tag) codec = new CodecHUFF;
      if(icopen->fccHandler==CodecProres::tag) codec = new CodecProres;
      if(icopen->fccHandler==CodecVP8::tag) codec = new CodecVP8;
      if(icopen->fccHandler==CodecVP9::tag) codec = new CodecVP9;
      if(icopen->fccHandler==CodecH265::id_tag) codec = new CodecH265;
      if(icopen->fccHandler==CodecH265LS::id_tag) codec = new CodecH265LS;
      if(icopen->fccHandler==CodecH264::tag) codec = new CodecH264;
      if(icopen->fccHandler==CFHD_TAG) return DriverProc_CF(dwDriverId, hDriver, uMsg, lParam1, lParam2);
      if(codec){
        if(!codec->init()){
          delete codec;
          codec = 0;
        }
      }
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
      memcpy((void*)lParam1, codec->config, rsize);
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

extern "C" LRESULT WINAPI VDDriverProc(DWORD_PTR dwDriverId, HDRVR hDriver, UINT uMsg, LPARAM lParam1, LPARAM lParam2)
{
  CodecClass* cs = (CodecClass*)dwDriverId;
  if(cs && cs->class_id==1) return VDDriverProc_CF(dwDriverId, hDriver, uMsg, lParam1, lParam2);

  CodecBase* codec = (CodecBase*)dwDriverId;

  switch (uMsg){
  case VDICM_ENUMFORMATS:
    if(lParam1==0) return CodecFFV1::tag;
    if(lParam1==CodecFFV1::tag) return CodecHUFF::tag;
    if(lParam1==CodecHUFF::tag) return CodecProres::tag;
    if(lParam1==CodecProres::tag) return CodecVP8::tag;
    if(lParam1==CodecVP8::tag) return CodecVP9::tag;
    if(lParam1==CodecVP9::tag) return CodecH265::id_tag;
    if(lParam1==CodecH265::id_tag) return CodecH265LS::id_tag;
    if(lParam1==CodecH265LS::id_tag) return CFHD_TAG;
    //if(lParam1==CFHD_TAG) return CodecH264::tag;
    return 0;

  case VDICM_GETHANDLER:
    if(codec) return codec->codec_tag;
    return ICERR_UNSUPPORTED;

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

  case VDICM_STREAMCONTROL:
    codec->streamControl((VDXStreamControl *)lParam1);
    return 0;
  
  case VDICM_GETSTREAMINFO:
    codec->getStreamInfo((VDXStreamInfo *)lParam1);
    return 0;
  
  case VDICM_COMPRESS:
    return codec->compress1((ICCOMPRESS *)lParam1, (VDXPixmapLayout*)lParam2);

  case VDICM_COMPRESS2:
    return codec->compress2((ICCOMPRESS *)lParam1, (VDXPictureCompress *)lParam2);

  case VDICM_COMPRESS_TRUNCATE:
    codec->frame_total = 0;
    return 0;

  case VDICM_LOGPROC:
    codec->logProc = (VDLogProc)lParam1;
    return 0;
  
  case VDICM_COMPRESS_MATRIX_INFO:
    {
      int colorSpaceMode = (int)lParam1;
      int colorRangeMode = (int)lParam2;

      switch(colorSpaceMode){
      case nsVDXPixmap::kColorSpaceMode_601:
        codec->colorspace = AVCOL_SPC_BT470BG;
        break;
      case nsVDXPixmap::kColorSpaceMode_709:
        codec->colorspace = AVCOL_SPC_BT709;
        break;
      default:
        codec->colorspace = AVCOL_SPC_UNSPECIFIED;
      }

      switch(colorRangeMode){
      case nsVDXPixmap::kColorRangeMode_Limited:
        codec->color_range = AVCOL_RANGE_MPEG;
        break;
      case nsVDXPixmap::kColorRangeMode_Full:
        codec->color_range = AVCOL_RANGE_JPEG;
        break;
      default:
        codec->color_range = AVCOL_RANGE_UNSPECIFIED;
      }

      return ICERR_OK;
    }
  }

  return ICERR_UNSUPPORTED;
}
