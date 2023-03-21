#include "cineform.h"
#include <stdint.h>

#include "cineform/common/CFHDDecoder.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
}

// this enables cfhd to operate within ffmpeg

typedef struct AVCodecTag {
  enum AVCodecID id;
  unsigned int tag;
} AVCodecTag;

//----------------------------------------------------------------------------------------------

struct DecoderObj{
  void* buf;
  CFHD_DecoderRef dec;
  CFHD_MetadataRef meta;
  CFHD_EncodedFormat encoded_format;
  COLOR_FORMAT input_format;
  int input_depth;
  CFHD_PixelFormat fmt;
  int samples;
  bool use_am;
};

av_cold int cfhd_init_decoder(AVCodecContext* avctx)
{
  DecoderObj* obj = (DecoderObj*)avctx->priv_data;
  obj->samples = 0;
  obj->dec = 0;
  obj->meta = 0;
  obj->encoded_format = CFHD_ENCODED_FORMAT_UNKNOWN;
  obj->input_format = COLOR_FORMAT_UNKNOWN;
  obj->input_depth = 0;
  obj->fmt = CFHD_PIXEL_FORMAT_UNKNOWN;
  avctx->pix_fmt = AV_PIX_FMT_RGBA64; // hack for max size
  CFHD_OpenDecoder(&obj->dec,0);
  CFHD_OpenMetadata(&obj->meta);
  obj->use_am = true;

  return 0;
}

av_cold int cfhd_close_decoder(AVCodecContext* avctx)
{
  DecoderObj* obj = (DecoderObj*)avctx->priv_data;
  if(obj->dec) CFHD_CloseDecoder(obj->dec);
  if(obj->meta) CFHD_CloseMetadata(obj->meta);
  return 0;
}

void cfhd_set_buffer(AVCodecContext* avctx, void* buf)
{
  DecoderObj* obj = (DecoderObj*)avctx->priv_data;
  obj->buf = buf;
}

void cfhd_set_use_am(AVCodecContext* avctx, bool v)
{
  DecoderObj* obj = (DecoderObj*)avctx->priv_data;
  obj->use_am = v;
}

void cfhd_set_encoded_format(AVCodecContext* avctx)
{
  DecoderObj* obj = (DecoderObj*)avctx->priv_data;
  CFHD_PixelFormat fmt;
  switch(obj->encoded_format){
  case CFHD_ENCODED_FORMAT_UNKNOWN:
    return;
  case CFHD_ENCODED_FORMAT_YUV_422:
    fmt = CFHD_PIXEL_FORMAT_YUY2;
    avctx->pix_fmt = AV_PIX_FMT_YUYV422;
    break;
  case CFHD_ENCODED_FORMAT_RGB_444:
    if(obj->input_depth==8){
      //fmt = CFHD_PIXEL_FORMAT_RG24;
      //avctx->pix_fmt = AV_PIX_FMT_BGR24;
      fmt = CFHD_PIXEL_FORMAT_BGRA;
      avctx->pix_fmt = AV_PIX_FMT_BGRA;
    } else {
      fmt = CFHD_PIXEL_FORMAT_B64A;
      avctx->pix_fmt = AV_PIX_FMT_BGRA64;
    }
    break;
  case CFHD_ENCODED_FORMAT_RGBA_4444:
    if(obj->input_depth==8){
      fmt = CFHD_PIXEL_FORMAT_BGRA;
      avctx->pix_fmt = AV_PIX_FMT_BGRA;
    } else {
      fmt = CFHD_PIXEL_FORMAT_B64A;
      avctx->pix_fmt = AV_PIX_FMT_BGRA64;
    }
    break;
  case CFHD_ENCODED_FORMAT_BAYER:
    fmt = CFHD_PIXEL_FORMAT_B64A;
    avctx->pix_fmt = AV_PIX_FMT_BGRA64;
    break;
  default:
    //fmt = CFHD_PIXEL_FORMAT_RG24;
    //avctx->pix_fmt = AV_PIX_FMT_BGR24;
    fmt = CFHD_PIXEL_FORMAT_BGRA;
    avctx->pix_fmt = AV_PIX_FMT_BGRA;
  }

  if(fmt!=obj->fmt){
    obj->samples = 0;
    obj->buf = 0;
    obj->fmt = fmt;
  }
}

void cfhd_set_format(AVCodecContext* avctx, nsVDXPixmap::VDXPixmapFormat vdfmt)
{
  DecoderObj* obj = (DecoderObj*)avctx->priv_data;
  CFHD_PixelFormat fmt;
  switch(vdfmt){
  case nsVDXPixmap::kPixFormat_YUV422_V210:
    fmt = CFHD_PIXEL_FORMAT_V210;
    avctx->pix_fmt = AV_PIX_FMT_RGB24; // best match
    break;
  case nsVDXPixmap::kPixFormat_YUV422_YU64:
    fmt = CFHD_PIXEL_FORMAT_YU64;
    avctx->pix_fmt = AV_PIX_FMT_RGB32; // best match
    break;
  case nsVDXPixmap::kPixFormat_RGB888:
    //fmt = CFHD_PIXEL_FORMAT_RG24;
    //avctx->pix_fmt = AV_PIX_FMT_BGR24;
    fmt = CFHD_PIXEL_FORMAT_BGRA;
    avctx->pix_fmt = AV_PIX_FMT_BGRA;
    break;
  case nsVDXPixmap::kPixFormat_XRGB8888:
    fmt = CFHD_PIXEL_FORMAT_BGRA;
    if(obj->encoded_format==CFHD_ENCODED_FORMAT_RGBA_4444)
      avctx->pix_fmt = AV_PIX_FMT_BGRA;
    else
      avctx->pix_fmt = AV_PIX_FMT_BGR0;
    break;
  case nsVDXPixmap::kPixFormat_R210:
    fmt = CFHD_PIXEL_FORMAT_R210;
    avctx->pix_fmt = AV_PIX_FMT_BGRA; // best match
    break;
  case nsVDXPixmap::kPixFormat_XRGB64:
    fmt = CFHD_PIXEL_FORMAT_B64A;
    avctx->pix_fmt = AV_PIX_FMT_BGRA64;
    break;
  case nsVDXPixmap::kPixFormat_Y16:
    fmt = CFHD_PIXEL_FORMAT_BYR4;
    avctx->pix_fmt = AV_PIX_FMT_GRAY16;
    break;
  default:
    cfhd_set_encoded_format(avctx);
    return;
  }

  if(fmt!=obj->fmt){
    obj->samples = 0;
    obj->buf = 0;
    obj->fmt = fmt;
  }
}

void cfhd_get_info(AVCodecContext* avctx, FilterModPixmapInfo& info)
{
  DecoderObj* obj = (DecoderObj*)avctx->priv_data;
  info.alpha_type = FilterModPixmapInfo::kAlphaInvalid;
  info.colorRangeMode = nsVDXPixmap::kColorRangeMode_None;
  if(obj->encoded_format==CFHD_ENCODED_FORMAT_RGBA_4444) info.alpha_type = FilterModPixmapInfo::kAlphaMask;
  if(obj->fmt==CFHD_PIXEL_FORMAT_B64A){
    info.ref_r = 0xFFF0;
    info.ref_g = 0xFFF0;
    info.ref_b = 0xFFF0;
    info.ref_a = 0xFFFF;

    if(obj->encoded_format==CFHD_ENCODED_FORMAT_RGB_444 || obj->encoded_format==CFHD_ENCODED_FORMAT_RGBA_4444){
      if(obj->input_depth==8){
        info.ref_r = 0xFF00;
        info.ref_g = 0xFF00;
        info.ref_b = 0xFF00;
      }
      if(obj->input_depth==10){
        info.ref_r = 0xFFC0;
        info.ref_g = 0xFFC0;
        info.ref_b = 0xFFC0;
      }
    }
  }
  if(obj->fmt==CFHD_PIXEL_FORMAT_YU64){
    info.ref_r = 0xFFFF;
  }
  if(obj->fmt==CFHD_PIXEL_FORMAT_BYR4){
    info.ref_r = 0xFFFF;
    info.colorRangeMode = nsVDXPixmap::kColorRangeMode_Full;
  }
}

bool cfhd_test_format(AVCodecContext* avctx, nsVDXPixmap::VDXPixmapFormat vdfmt)
{
  DecoderObj* obj = (DecoderObj*)avctx->priv_data;
  if(obj->encoded_format==CFHD_ENCODED_FORMAT_YUV_422){
    switch(vdfmt){
    case nsVDXPixmap::kPixFormat_YUV422_YUYV:
    case nsVDXPixmap::kPixFormat_YUV422_V210:
    case nsVDXPixmap::kPixFormat_YUV422_YU64:
    //case nsVDXPixmap::kPixFormat_RGB888:
    case nsVDXPixmap::kPixFormat_XRGB8888:
    case nsVDXPixmap::kPixFormat_XRGB64:
      return true;
    }
  }
  if(obj->encoded_format==CFHD_ENCODED_FORMAT_RGB_444){
    switch(vdfmt){
    //case nsVDXPixmap::kPixFormat_RGB888:
    case nsVDXPixmap::kPixFormat_XRGB8888:
    case nsVDXPixmap::kPixFormat_R210:
    case nsVDXPixmap::kPixFormat_XRGB64:
      return true;
    }
  }
  if(obj->encoded_format==CFHD_ENCODED_FORMAT_RGBA_4444){
    switch(vdfmt){
    case nsVDXPixmap::kPixFormat_XRGB8888:
    //case nsVDXPixmap::kPixFormat_RGB888:
    case nsVDXPixmap::kPixFormat_R210:
    case nsVDXPixmap::kPixFormat_XRGB64:
      return true;
    }
  }
  if(obj->encoded_format==CFHD_ENCODED_FORMAT_BAYER){
    switch(vdfmt){
    case nsVDXPixmap::kPixFormat_XRGB8888:
    //case nsVDXPixmap::kPixFormat_RGB888:
    case nsVDXPixmap::kPixFormat_R210:
    case nsVDXPixmap::kPixFormat_XRGB64:
      return true;
    case nsVDXPixmap::kPixFormat_Y16:
      return true;
    }
  }

  return false;
}

std::string cfhd_format_name(AVCodecContext* avctx)
{
  DecoderObj* obj = (DecoderObj*)avctx->priv_data;
  switch(obj->encoded_format){
  case CFHD_ENCODED_FORMAT_YUV_422:
    return "yuv422 10-bit";
  case CFHD_ENCODED_FORMAT_RGB_444:
    if(obj->input_depth==8) return "rgb 8/12-bit";
    if(obj->input_depth==10) return "rgb 10/12-bit";
    return "rgb 12-bit";
  case CFHD_ENCODED_FORMAT_RGBA_4444:
    if(obj->input_depth==8) return "rgba 8/12-bit";
    if(obj->input_depth==10) return "rgba 10/12-bit";
    return "rgba 12-bit";
  case CFHD_ENCODED_FORMAT_BAYER:
    return "bayer 12-bit";
  default:
    return "unknown";
  }
}

void swap_b64a(AVCodecContext* avctx, void* data, int linesize)
{
  {for(int y=0; y<avctx->height; y++){
    uint16_t* src = (uint16_t*)data + y*linesize/2;

    {for(int x=0; x<avctx->width; x++){
      uint16_t a = src[0];
      uint16_t r = src[1];
      uint16_t g = src[2];
      uint16_t b = src[3];

      src[0] = b;
      src[1] = g;
      src[2] = r;
      src[3] = a;

      src += 4;
    }}
  }}
}

int cfhd_decode(AVCodecContext* avctx, void* data, int* got_frame, AVPacket* avpkt)
{
  DecoderObj* obj = (DecoderObj*)avctx->priv_data;
  if(!obj->fmt){
    CFHD_SampleHeader header;
    if(CFHD_ParseSampleHeader(avpkt->data,avpkt->size,&header)!=CFHD_ERROR_OKAY) return 0;
    CFHD_EncodedFormat format;
    header.GetEncodedFormat(&format);
    obj->encoded_format = format;
    COLOR_FORMAT input_format;
    header.GetInputFormat(&input_format);
    obj->input_format = input_format;
    obj->input_depth = 0;
    switch(format){
    case CFHD_ENCODED_FORMAT_RGB_444:
    case CFHD_ENCODED_FORMAT_RGBA_4444:
      switch(input_format){
      case COLOR_FORMAT_RGB24:
      case COLOR_FORMAT_RGB32:
      case COLOR_FORMAT_RGB32_INVERTED:
      case COLOR_FORMAT_BGRA32:
      case COLOR_FORMAT_QT32:
        obj->input_depth = 8;
        break;
      case COLOR_FORMAT_R210:
      case COLOR_FORMAT_RGB10:
      case COLOR_FORMAT_RG30:
      case COLOR_FORMAT_AR10:
      case COLOR_FORMAT_AB10:
      case COLOR_FORMAT_DPX0:
        obj->input_depth = 10;
        break;
      }
      break;
    case CFHD_ENCODED_FORMAT_BAYER:
      {
        int w,h;
        CFHD_PrepareToDecode(obj->dec,0,0,CFHD_PIXEL_FORMAT_BYR4,CFHD_DECODED_RESOLUTION_FULL,0,avpkt->data,avpkt->size,&w,&h,0);
        avctx->width = w;
        avctx->height = h;
      }
      break;
    }
    cfhd_set_encoded_format(avctx);
  }
  if(obj->samples==0){
    CFHD_PixelFormat fmt_array[128];
    int fmt_count = 0;
    CFHD_GetOutputFormats(obj->dec,avpkt->data,avpkt->size,fmt_array,128,&fmt_count);

    int w=0;
    int h=0;
    CFHD_PrepareToDecode(obj->dec,avctx->width,avctx->height,obj->fmt,CFHD_DECODED_RESOLUTION_FULL,0,avpkt->data,avpkt->size,&w,&h,&obj->fmt);
    if(!obj->use_am){
      CFHD_InitSampleMetadata(obj->meta,METADATATYPE_ORIGINAL,0,0);
      unsigned int processflags = PROCESSING_ALL_OFF;
      CFHD_SetActiveMetadata(obj->dec,obj->meta,TAG_PROCESS_PATH,METADATATYPE_UINT32,&processflags,sizeof(unsigned int));
    }
    //unsigned int maxcpus = 1;
    //CFHD_SetActiveMetadata(obj->dec,obj->meta,TAG_CPU_MAX,METADATATYPE_UINT32,&maxcpus,sizeof(unsigned int));
  }
  obj->samples++;

  int buf_size = avpkt->size;
  AVFrame* frame = (AVFrame*)data;
  bool key = (avpkt->flags & AV_PKT_FLAG_KEY)!=0;
  frame->pict_type = key ? AV_PICTURE_TYPE_I : AV_PICTURE_TYPE_P;
  frame->key_frame = key;

  void* dst;
  int linesize;

  if(obj->buf){
    frame->buf[0] = av_buffer_alloc(0); // needed to mute avcodec assert
    AVFrame pic = {0};
    av_image_fill_arrays(pic.data, pic.linesize, (uint8_t*)obj->buf, avctx->pix_fmt, avctx->width, avctx->height, 16);
    if(obj->fmt==CFHD_PIXEL_FORMAT_V210){
      int row = (avctx->width+47)/48*128;
      pic.linesize[0] = row;
    }
    dst = obj->buf;
    linesize = pic.linesize[0];

  } else {
    int frame_size = av_image_get_buffer_size(avctx->pix_fmt, avctx->width, avctx->height, 16);
    frame->buf[0] = av_buffer_alloc(frame_size);
    if(!frame->buf[0]) return AVERROR(ENOMEM);
    const uint8_t* buf = frame->buf[0]->data;
    av_image_fill_arrays(frame->data, frame->linesize, buf, avctx->pix_fmt, avctx->width, avctx->height, 16);
    if(obj->fmt==CFHD_PIXEL_FORMAT_V210){
      int row = (avctx->width+47)/48*128;
      frame->linesize[0] = row;
    }
    dst = frame->data[0];
    linesize = frame->linesize[0];
  }

  CFHD_DecodeSample(obj->dec,avpkt->data,avpkt->size,dst,linesize);
  if(obj->fmt==CFHD_PIXEL_FORMAT_B64A) swap_b64a(avctx,dst,linesize);

  if(obj->encoded_format==CFHD_ENCODED_FORMAT_YUV_422){
    int flags = CFHD_get_decoder_color_flags(obj->dec);
    if((flags & COLORSPACE_MASK)==COLOR_SPACE_BT_601) avctx->colorspace = AVCOL_SPC_BT470BG;
    if((flags & COLORSPACE_MASK)==COLOR_SPACE_BT_709) avctx->colorspace = AVCOL_SPC_BT709;
  }

  *got_frame = 1;
  return buf_size;
}

AVCodec cfhd_decoder;

void av_register_cfhd()
{
  cfhd_decoder.name = "CineForm HD (native)";
  cfhd_decoder.long_name = "CineForm HD (native)";
  cfhd_decoder.type = AVMEDIA_TYPE_VIDEO;
  cfhd_decoder.id = CFHD_ID;
  cfhd_decoder.priv_data_size = sizeof(DecoderObj);
  cfhd_decoder.init = cfhd_init_decoder;
  cfhd_decoder.close = cfhd_close_decoder;
  cfhd_decoder.decode = cfhd_decode;
  cfhd_decoder.priv_class = 0;
  cfhd_decoder.capabilities = 0;

  avcodec_register(&cfhd_decoder);
}

