#include "InputFile2.h"
#include "VideoSource2.h"
#include "cineform.h"
#include "export.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

const int line_align = 16; // should be ok with any usable filter down the pipeline
extern bool config_force_thread;
extern float config_cache_size;

uint8_t* align_buf(uint8_t* p)
{
  return (uint8_t*)(ptrdiff_t(p+line_align-1) & ~(line_align-1));
}

VDFFVideoSource::VDFFVideoSource(const VDXInputDriverContext& context)
  :mContext(context)
{
  m_pFormatCtx = 0;
  m_pStreamCtx = 0;
  m_pCodecCtx = 0;
  m_pSwsCtx = 0;
  direct_format = 0;
  direct_format_len = 0;
  frame = 0;
  memset(&copy_pkt,0,sizeof(copy_pkt));
  errorMode = kErrorModeReportAll; // still not supported by host anyway
  m_pixmap_data = 0;
  m_pixmap_frame = -1;
  last_request = -1;
  next_frame = -1;
  last_seek_frame = -1;
  buffer = 0;
  buffer_count = 0;
  small_buffer_count = 0;
  buffer_reserve = 0;
  frame_array = 0;
  frame_type = 0;
  flip_image = false;
  direct_buffer = false;
  direct_cfhd = false;
  is_image_list = false;
  avi_drop_index = false;
  copy_mode = false;
  decode_mode = true;
  small_cache_mode = false;
  enable_prefetch = false;
  decoded_count = 0;
  buffer = 0;
  mem = 0;

  /*
  kPixFormat_XRGB64 = 0;
  IFilterModPixmap* fmpixmap = (IFilterModPixmap*)context.mpCallbacks->GetExtendedAPI("IFilterModPixmap");
  if(fmpixmap){
    kPixFormat_XRGB64 = fmpixmap->GetFormat_XRGB64();
  }
  */
}

VDFFVideoSource::~VDFFVideoSource() 
{
  av_packet_unref(&copy_pkt);
  free(direct_format);
  if(frame) av_frame_free(&frame);
  if(m_pCodecCtx) avcodec_free_context(&m_pCodecCtx);
  if(m_pSwsCtx) sws_freeContext(m_pSwsCtx);

  if(buffer) {for(int i=0; i<buffer_count; i++){
    BufferPage& p = buffer[i];
    dealloc_page(&p);
  }}
  if(mem) CloseHandle(mem);
  free(buffer);
  free(frame_array);
  free(frame_type);
  free(m_pixmap_data);
}

int VDFFVideoSource::AddRef() {
  return vdxunknown<IVDXStreamSource>::AddRef();
}

int VDFFVideoSource::Release() {
  return vdxunknown<IVDXStreamSource>::Release();
}

void *VDXAPIENTRY VDFFVideoSource::AsInterface(uint32_t iid)
{
  if (iid == IVDXVideoSource::kIID)
    return static_cast<IVDXVideoSource *>(this);

  if (iid == IVDXVideoDecoder::kIID)
    return static_cast<IVDXVideoDecoder *>(this);

  if (iid == IFilterModVideoDecoder::kIID)
    return static_cast<IFilterModVideoDecoder *>(this);

  if (iid == IVDXStreamSourceV3::kIID)
    return static_cast<IVDXStreamSourceV3 *>(this);

  if (iid == IVDXStreamSourceV5::kIID)
    return static_cast<IVDXStreamSourceV5 *>(this);

  return vdxunknown<IVDXStreamSource>::AsInterface(iid);
}

int VDFFVideoSource::init_duration(const AVRational fr)
{
  AVRational tb = m_pStreamCtx->time_base;
  av_reduce(&time_base.num, &time_base.den, int64_t(fr.num)*tb.num, int64_t(fr.den)*tb.den, INT_MAX);

  int sample_count_error = 2;

  if(m_pSource->is_image_list){
    sample_count = (int)m_pStreamCtx->nb_frames;
    sample_count_error = 0;

  } else {
    int64_t duration = m_pStreamCtx->duration;

    if(m_pStreamCtx->duration == AV_NOPTS_VALUE){
      if(m_pFormatCtx->duration != AV_NOPTS_VALUE){
        duration = av_rescale_q_rnd(m_pFormatCtx->duration, av_make_q(1,AV_TIME_BASE), m_pStreamCtx->time_base, AV_ROUND_NEAR_INF);
      }
    }

    if(duration != AV_NOPTS_VALUE){
      int rndd = time_base.den/2;
      //! stream duration really means last timestamp
      // found on "10 bit.mp4"
      // also works with mkv (derived from file duration)
      int64_t start_pts = m_pStreamCtx->start_time;
      if(start_pts==AV_NOPTS_VALUE) start_pts = 0;
      duration -= start_pts;
      //! above idea fails on Hilary.0000.ts

      bool mts = false;
      AVInputFormat* mts_format = av_find_input_format("mpegts");
      if(m_pFormatCtx->iformat==mts_format) mts = true;

      if(mts || duration<=0){
        // undo previous step
        duration += start_pts;
        start_pts = 0; // ignore count_error
      }
      sample_count = (int)((duration * time_base.num + rndd) / time_base.den);
      int e = (int)((start_pts * time_base.num + rndd) / time_base.den);
      e = abs(e);
      if(e>sample_count_error) sample_count_error = e;

      // found in some mp4 produced with GPAC 0.5.1
      if(m_pStreamCtx->nb_index_entries>0 && (m_pStreamCtx->index_entries[0].flags & AVINDEX_DISCARD_FRAME)!=0) sample_count_error++;

    } else {
      if(m_pSource->is_image){
        sample_count = 1;
      } else {
        mContext.mpCallbacks->SetError("FFMPEG: Cannot figure stream duration. Unsupported.");
        return -1;
      }
    }
  }

  if(sample_count==0){
    // found in 1-frame nut
    AVInputFormat* nut_format = av_find_input_format("nut");
    if(m_pFormatCtx->iformat==nut_format) sample_count = 1;
  }

  m_streamInfo.mInfo.mSampleRate.mNumerator = fr.num;
  m_streamInfo.mInfo.mSampleRate.mDenominator = fr.den;

  return sample_count_error;
}

int VDFFVideoSource::initStream( VDFFInputFile* pSource, int streamIndex )
{
  m_pSource = pSource;
  m_streamIndex = streamIndex;

  m_pFormatCtx = pSource->getContext();  
  m_pStreamCtx = m_pFormatCtx->streams[m_streamIndex];

  has_vfr = false;
  average_fr = false;

  if(m_pStreamCtx->codecpar->codec_tag==CFHD_TAG && !pSource->cfg_skip_cfhd){
    // use our native thunk instead of internal decoder
    if(avcodec_find_decoder(CFHD_ID)){
      m_pStreamCtx->codecpar->codec_id = CFHD_ID;
      direct_cfhd = true;
    }
  }
  AVCodec* pDecoder = avcodec_find_decoder(m_pStreamCtx->codecpar->codec_id);
  if(m_pStreamCtx->codecpar->codec_id==AV_CODEC_ID_VP8){
    // on2 vp8 does not extract alpha
    AVCodec* pDecoder2 = avcodec_find_decoder_by_name("libvpx");
    if(pDecoder2) pDecoder = pDecoder2;
  }
  if(m_pStreamCtx->codecpar->codec_id==AV_CODEC_ID_VP9){
    // on2 vp9 does not extract alpha
    AVCodec* pDecoder2 = avcodec_find_decoder_by_name("libvpx-vp9");
    if(pDecoder2) pDecoder = pDecoder2;
  }
  if(!pDecoder){
    char buf[AV_FOURCC_MAX_STRING_SIZE];
    av_fourcc_make_string(buf,m_pStreamCtx->codecpar->codec_tag);
    mContext.mpCallbacks->SetError("FFMPEG: Unsupported codec (%s)", buf);
    return -1;
  }
  m_pCodecCtx = avcodec_alloc_context3(pDecoder);
  if(!m_pCodecCtx){
    return -1;
  }
  m_pCodecCtx->flags2 = AV_CODEC_FLAG2_SHOW_ALL;
  avcodec_parameters_to_context(m_pCodecCtx,m_pStreamCtx->codecpar);

  AVRational r_fr = av_stream_get_r_frame_rate(m_pStreamCtx);
  if(m_pStreamCtx->codecpar->field_order>AV_FIELD_PROGRESSIVE){
    // interlaced seems to double r_framerate
    // example: 00005.MTS
    // however other samples do not show this
    // example: amanda_excerpt.m2t
    // idea of this: r_fr cannot be lower than average
    AVRational avg_fr = m_pStreamCtx->avg_frame_rate;
    if(int64_t(r_fr.num)*avg_fr.den>=int64_t(avg_fr.num)*r_fr.den*2) r_fr.den *= 2;
  }
  int sample_count_error = init_duration(r_fr);
  if(sample_count_error==-1) return -1;

  if(pSource->is_image){
    is_image_list = true;
    trust_index = false;
    sparse_index = false;
    keyframe_gap = 1;
    fw_seek_threshold = 1;

  } else {
    if(m_pStreamCtx->nb_index_entries<2){
      // try to force loading index
      // works for 2017-04-07 08-53-48.flv
      int64_t pos = m_pStreamCtx->duration;
      if(pos==AV_NOPTS_VALUE) pos = int64_t(sample_count)*time_base.den / time_base.num;
      seek_frame(m_pFormatCtx,m_streamIndex,pos,AVSEEK_FLAG_BACKWARD);
      seek_frame(m_pFormatCtx,m_streamIndex,AV_SEEK_START,AVSEEK_FLAG_BACKWARD);
    }
    trust_index = false;
    sparse_index = false;

    if(m_pStreamCtx->nb_index_entries>2){
      // when avi has dropped frames ffmpeg removes these entries from index - find way to avoid this?
      AVInputFormat* avi_format = av_find_input_format("avi");
      if(m_pFormatCtx->iformat==avi_format){
        //sample_count = m_pStreamCtx->nb_index_entries;
        //trust_index = true;
        trust_index = (sample_count==m_pStreamCtx->nb_index_entries);
      } else if(abs(m_pStreamCtx->nb_index_entries - sample_count)<=sample_count_error){
        sample_count = m_pStreamCtx->nb_index_entries;
        trust_index = true;
      } else {
        // maybe vfr
        // works for 30.mov
        // works for device-2017-02-13-115329.mp4
        AVRational avg_fr = m_pStreamCtx->avg_frame_rate;
        // 0 found in some wmv
        if(avg_fr.num!=0){
          sample_count_error = init_duration(avg_fr);
          if(abs(m_pStreamCtx->nb_index_entries - sample_count)<=sample_count_error){
            sample_count = m_pStreamCtx->nb_index_entries;
            trust_index = true;
            average_fr = true;
          } else {
            // becomes worse, step back (guess, no sample)
            sample_count_error = init_duration(r_fr);
          }
        }
      }
    }

    if(trust_index){
      int64_t exp_dt = time_base.den/time_base.num;
      int64_t min_dt = exp_dt;
      {for(int i=1; i<m_pStreamCtx->nb_index_entries; i++){
        AVIndexEntry& i0 = m_pStreamCtx->index_entries[i-1];
        AVIndexEntry& i1 = m_pStreamCtx->index_entries[i];
        if(i0.flags & AVINDEX_DISCARD_FRAME) continue;
        if(i1.flags & AVINDEX_DISCARD_FRAME) continue;
        int64_t dt = i1.timestamp - i0.timestamp;
        if(dt<(exp_dt-1) || dt>(exp_dt+1)){
          has_vfr = true;
        }
        if(dt<min_dt){
          min_dt = dt;
        }
      }}

      /*
      increasing framerate could work but has huge memory impact
      if(has_vfr){
        trust_index = false;
        AVRational tb = m_pStreamCtx->time_base;
        AVRational fr;
        av_reduce(&fr.num, &fr.den, tb.den, int64_t(tb.num)*min_dt, INT_MAX);

        init_duration(fr);
      }
      */
    }

    keyframe_gap = 0;
    if(trust_index){
      int d = 1;
      {for(int i=0; i<m_pStreamCtx->nb_index_entries; i++){
        if(m_pStreamCtx->index_entries[i].flags & AVINDEX_KEYFRAME){
          if(d>keyframe_gap) keyframe_gap = d;
          d = 1;
        } else d++;
      }}
      if(d>keyframe_gap) keyframe_gap = d;
    } else if(m_pStreamCtx->nb_index_entries>1){
      sparse_index = true;
      int p0 = 0;
      {for(int i=0; i<m_pStreamCtx->nb_index_entries; i++){
        if(m_pStreamCtx->index_entries[i].flags & AVINDEX_KEYFRAME){
          int64_t ts = m_pStreamCtx->index_entries[i].timestamp;
          ts -= m_pStreamCtx->start_time;
          int rndd = time_base.den/2;
          int pos = int((ts*time_base.num + rndd) / time_base.den);
          int d = pos-p0;
          p0 = pos;
          if(d>keyframe_gap) keyframe_gap = d;
        }
      }}
      int d = sample_count-p0;
      if(d>keyframe_gap) keyframe_gap = d;
    }
  }

  // threading has big negative impact on random access within all-keyframe files
  if(allow_copy()){
    //m_pCodecCtx->thread_count = 1;
    m_pCodecCtx->thread_count = 0;
    m_pCodecCtx->thread_type = FF_THREAD_SLICE;
    if(config_force_thread)
      m_pCodecCtx->thread_type = FF_THREAD_SLICE|FF_THREAD_FRAME;
  } else {
    m_pCodecCtx->thread_count = 0;
    m_pCodecCtx->thread_type = FF_THREAD_SLICE|FF_THREAD_FRAME;
  }

  fw_seek_threshold = 10;
  if(keyframe_gap==1) fw_seek_threshold = 0; // assume seek is free with all-keyframe

  m_pCodecCtx->refcounted_frames = 1;

  if(avcodec_open2(m_pCodecCtx, pDecoder, 0)<0){
    mContext.mpCallbacks->SetError("FFMPEG: Decoder error.");
    return -1;
  }

  if(direct_cfhd){
    flip_image = true;
    if(trust_index) direct_buffer = true;
    cfhd_set_use_am(m_pCodecCtx, !pSource->cfg_skip_cfhd_am);
  }

  frame = av_frame_alloc();
  dead_range_start = -1;
  dead_range_end = -1;
  first_frame = 0;
  last_frame = 0;
  used_frames = 0;
  if(keyframe_gap>1)
    buffer_reserve = keyframe_gap*2;
  else
    buffer_reserve = 1;
  if(buffer_reserve<pSource->cfg_frame_buffers) buffer_reserve = pSource->cfg_frame_buffers;
  if(buffer_reserve>sample_count) buffer_reserve = sample_count;

  int fa_size = sample_count*sizeof(void*);
  frame_array = (BufferPage**)malloc(fa_size);
  memset(frame_array,0,fa_size);
  int ft_size = sample_count;
  frame_type = (char*)malloc(ft_size);
  memset(frame_type,' ',ft_size);

  init_format();
  next_frame = 0;
  if(frame_fmt==-1){
    //! unable to reserve buffers for unknown format
    mContext.mpCallbacks->SetError("FFMPEG: Unknown picture format.");
    return -1;
  }

  MEMORYSTATUSEX ms = {sizeof(MEMORYSTATUSEX)};
  GlobalMemoryStatusEx(&ms);

  uint64_t max_virtual = ms.ullTotalPhys;
  uint64_t gb1 = 0x40000000;
  if(max_virtual<2*gb1) max_virtual = 0; else max_virtual -= 2*gb1;
  uint64_t max2 = (uint64_t)(config_cache_size*gb1);
  if(max2<max_virtual) max_virtual = max2;

  uint64_t mem_other = 0;
  if(m_pSource->head_segment){
    VDFFInputFile* f1 = m_pSource->head_segment;
    while(1){
      if(!f1->video_source) break;
      mem_other += uint64_t(frame_size)*f1->video_source->buffer_reserve;
      f1 = f1->next_segment;
      if(!f1) break;
    }
  }

  uint64_t mem_size = uint64_t(frame_size)*buffer_reserve;
  if(mem_size+mem_other>max_virtual || pSource->cfg_disable_cache){
    buffer_reserve = int((max_virtual-mem_other)/frame_size);
    if(buffer_reserve<pSource->cfg_frame_buffers || pSource->cfg_disable_cache) buffer_reserve = pSource->cfg_frame_buffers;
    mem_size = uint64_t(frame_size)*buffer_reserve;
  }

  #ifndef _WIN64
  uint64_t max_heap = 0x20000000;
  if(mem_size+mem_other>max_heap){
    mem_size = (mem_size + 0xFFFF) & ~0xFFFF;
    mem = CreateFileMapping(INVALID_HANDLE_VALUE,0,PAGE_READWRITE,mem_size>>32,(DWORD)mem_size,0);
    if(!mem){
      if(buffer_reserve>pSource->cfg_frame_buffers){
        buffer_reserve = pSource->cfg_frame_buffers;
        mem_size = uint64_t(frame_size)*buffer_reserve;
        mem_size = (mem_size + 0xFFFF) & ~0xFFFF;
        mem = CreateFileMapping(INVALID_HANDLE_VALUE,0,PAGE_READWRITE,mem_size>>32,(DWORD)mem_size,0);
      }
    }
    if(!mem){
      mContext.mpCallbacks->SetErrorOutOfMemory();
      return -1;
    }
  }
  #endif

  buffer = (BufferPage*)malloc(sizeof(BufferPage)*buffer_reserve);
  memset(buffer,0,sizeof(BufferPage)*buffer_reserve);
  buffer_count = buffer_reserve;

  m_streamInfo.mFlags = 0;
  m_streamInfo.mfccHandler = export_avi_fcc(m_pStreamCtx);

  m_streamInfo.mInfo.mSampleCount = sample_count;

  AVRational ar = av_make_q(1,1);
  if(m_pCodecCtx->sample_aspect_ratio.num) ar = m_pCodecCtx->sample_aspect_ratio;
  if(m_pStreamCtx->sample_aspect_ratio.num) ar = m_pStreamCtx->sample_aspect_ratio;
  AVRational ar1;
  av_reduce(&ar1.num, &ar1.den, ar.num, ar.den, INT_MAX);
  m_streamInfo.mInfo.mPixelAspectRatio.mNumerator = ar1.num;
  m_streamInfo.mInfo.mPixelAspectRatio.mDenominator = ar1.den;

  if(allow_copy()){
    direct_format_len = sizeof(BITMAPINFOHEADER);
    direct_format_len += (m_pCodecCtx->extradata_size+1) & ~1;
    direct_format = malloc(direct_format_len);
    memset(direct_format, 0, direct_format_len);
    BITMAPINFOHEADER* outhdr = (BITMAPINFOHEADER*)direct_format;
    outhdr->biSize        = sizeof(BITMAPINFOHEADER) + m_pCodecCtx->extradata_size;
    outhdr->biWidth       = m_pCodecCtx->width;
    outhdr->biHeight      = m_pCodecCtx->height;
    outhdr->biCompression = m_streamInfo.mfccHandler;
    memcpy(((uint8*)direct_format)+sizeof(BITMAPINFOHEADER),m_pCodecCtx->extradata,m_pCodecCtx->extradata_size);
  }

  AVInputFormat* avi_format = av_find_input_format("avi");
  if(m_pFormatCtx->iformat==avi_format){
    avi_drop_index = true;
    memset(frame_type,'D',ft_size);

    {for(int i=0; i<m_pStreamCtx->nb_index_entries; i++){
      int64_t ts = m_pStreamCtx->index_entries[i].timestamp;
      ts -= m_pStreamCtx->start_time;
      int rndd = time_base.den/2;
      int pos = int((ts*time_base.num + rndd) / time_base.den);
      if(pos>=0 && pos<sample_count) frame_type[pos] = ' ';
    }}
  }

  // start_time rarely known before actually decoding, init from here
  read_frame(0,true);
  // workaround for unspecified delay
  // found in MVI_4722.MP4
  if(sample_count>1 && !m_pCodecCtx->has_b_frames && possible_delay()){
    read_frame(1,false);
    if(m_pCodecCtx->has_b_frames){
      free_buffers();
      avcodec_flush_buffers(m_pCodecCtx);
      seek_frame(m_pFormatCtx, m_streamIndex, AV_SEEK_START, AVSEEK_FLAG_BACKWARD);
      read_frame(0,true);
    }
  }

  if(frame_fmt!=m_pCodecCtx->pix_fmt){
    init_format();
  }

  return 0;
}

bool VDFFVideoSource::possible_delay()
{
  if(is_intra()) return false;

  AVCodecID codec_id = m_pStreamCtx->codecpar->codec_id;
  const AVCodecDescriptor* desc = avcodec_descriptor_get(codec_id);
  if(desc && (desc->props & AV_CODEC_PROP_REORDER)) return true;
  return false;
}

bool VDFFVideoSource::is_intra()
{
  if(is_image_list) return false;
  if(trust_index && keyframe_gap==1) return true;

  AVCodecID codec_id = m_pStreamCtx->codecpar->codec_id;
  // various intra codecs
  switch(codec_id){
  case AV_CODEC_ID_CLLC:
  case AV_CODEC_ID_DNXHD:
  case AV_CODEC_ID_DVVIDEO:
  case AV_CODEC_ID_MJPEG:
  case AV_CODEC_ID_RAWVIDEO:
  case AV_CODEC_ID_HUFFYUV:
  case AV_CODEC_ID_FFV1:
  case AV_CODEC_ID_PNG:
  case AV_CODEC_ID_FFVHUFF:
  case AV_CODEC_ID_FRAPS:
  case AV_CODEC_ID_JPEG2000:
  case AV_CODEC_ID_DIRAC:
  case AV_CODEC_ID_V210:
  case AV_CODEC_ID_R210:
  case AV_CODEC_ID_R10K:
  case AV_CODEC_ID_LAGARITH:
  case AV_CODEC_ID_PRORES:
  case AV_CODEC_ID_UTVIDEO:
  case AV_CODEC_ID_V410:
  case AV_CODEC_ID_HQ_HQA:
  case AV_CODEC_ID_HQX:
  case AV_CODEC_ID_SNOW:
  case AV_CODEC_ID_CFHD:
  case AV_CODEC_ID_MAGICYUV:
  case AV_CODEC_ID_SHEERVIDEO:
    return true;
  }

  if(codec_id==CFHD_ID) return true;

  const AVCodecDescriptor* desc = avcodec_descriptor_get(codec_id);
  if(desc && (desc->props & AV_CODEC_PROP_INTRA_ONLY)) return true;

  return false;
}

bool VDFFVideoSource::allow_copy()
{
  if(is_intra()) return true;
  return false;
}

void VDFFVideoSource::init_format()
{
  if(direct_cfhd){
    cfhd_set_format(m_pCodecCtx,convertInfo.ext_format);
  }
  frame_fmt = m_pCodecCtx->pix_fmt;
  frame_width = m_pCodecCtx->width;
  frame_height = m_pCodecCtx->height;
  frame_size = av_image_get_buffer_size(frame_fmt, frame_width, frame_height, line_align);
  if(frame_fmt==-1) frame_size = 0;
  if(direct_buffer){
    // 6 px per 16 bytes, 128 byte aligned
    int row = (frame_width+47)/48*128;
    int frame_size2 = row*frame_height;
    if(frame_size2>frame_size) frame_size = frame_size2;
  }
  free_buffers();
  {for(int i=0; i<buffer_count; i++) dealloc_page(&buffer[i]); }
}

void VDXAPIENTRY VDFFVideoSource::GetStreamSourceInfo(VDXStreamSourceInfo& srcInfo)
{
  srcInfo = m_streamInfo.mInfo;
}

void VDXAPIENTRY VDFFVideoSource::GetStreamSourceInfoV3(VDXStreamSourceInfoV3& srcInfo)
{
  srcInfo = m_streamInfo;
}

void VDXAPIENTRY VDFFVideoSource::ApplyStreamMode(uint32 flags)
{
  if((flags & kStreamModeDirectCopy)!=0 && !QueryStreamMode(kStreamModeDirectCopy)) return;
  bool copy_mode = false;
  bool decode_mode = false;
  bool cache_mode = true;
  if(flags & kStreamModeDirectCopy) copy_mode = true;
  if(flags & kStreamModeUncompress) decode_mode = true;
  if(flags & kStreamModePlayForward) cache_mode = false;
  setCopyMode(copy_mode);
  setDecodeMode(decode_mode);
  setCacheMode(cache_mode);

  if(m_pSource->next_segment) m_pSource->next_segment->video_source->ApplyStreamMode(flags);
}

bool VDXAPIENTRY VDFFVideoSource::QueryStreamMode(uint32 flags)
{
  if(flags==kStreamModeDirectCopy){
    if(direct_format_len==0) return false;
    if(m_pSource->head_segment){
      VDFFVideoSource* head = m_pSource->head_segment->video_source;
      if(direct_format_len!=head->direct_format_len) return false;
      if(memcmp(direct_format,head->direct_format,direct_format_len)!=0) return false;
    }
    if(m_pSource->next_segment && !m_pSource->next_segment->video_source->QueryStreamMode(flags)) return false;
    return true;
  }
  return false;
}

const void *VDFFVideoSource::GetDirectFormat()
{
  return copy_mode ? direct_format : 0;
}

int VDFFVideoSource::GetDirectFormatLen() 
{
  return copy_mode ? direct_format_len: 0;
}

void VDFFVideoSource::setCopyMode(bool v)
{
  if(copy_mode==v) return;
  copy_mode = v;
  if(v){
    free_buffers();
    next_frame = -1;
    last_seek_frame = -1;
    enable_prefetch = false;
  }
  av_packet_unref(&copy_pkt);
}

void VDFFVideoSource::setDecodeMode(bool v)
{
  if(decode_mode==v) return;
  decode_mode = v;
  if(v){
    // must begin from IDR
    next_frame = -1;
    last_seek_frame = -1;
    enable_prefetch = false;
  }
}

void VDFFVideoSource::setCacheMode(bool v)
{
  small_cache_mode = !v;
  if(!v){
    enable_prefetch = false;
    int buffer_max = m_pSource->cfg_frame_buffers;
    // 1 required +1 to handle dups
    // and somewhere 10 to soften display triple-buffering, filter process-ahead or whatever
    // (not strictly required but helps to recover from mode switching)
    if(buffer_max<16) buffer_max = 16;
    if(buffer_max>buffer_count) buffer_max = buffer_count;
    small_buffer_count = buffer_max;

    if(mem && used_frames>buffer_max){
      free_buffers();
      CloseHandle(mem);
      int64_t mem_size = uint64_t(frame_size)*buffer_reserve;
      mem_size = (mem_size + 0xFFFF) & ~0xFFFF;
      mem = CreateFileMapping(INVALID_HANDLE_VALUE,0,PAGE_READWRITE,mem_size>>32,(DWORD)mem_size,0);

    } else {
      int anchor = next_frame-1;
      while(1){
        BufferPage* p = remove_page(anchor,false,true);
        if(!p) break;
        if(used_frames>buffer_max) dealloc_page(p);
      }
      while(used_frames>buffer_max){
        BufferPage* p = remove_page(anchor);
        dealloc_page(p);
      }
    }
  }
}

IVDXStreamSource::ErrorMode VDFFVideoSource::GetDecodeErrorMode() 
{
  return errorMode;
}

void VDFFVideoSource::SetDecodeErrorMode(IVDXStreamSource::ErrorMode mode) 
{
  errorMode = mode;
}

bool VDFFVideoSource::IsDecodeErrorModeSupported(IVDXStreamSource::ErrorMode mode)
{
  return mode == kErrorModeReportAll || mode == kErrorModeDecodeAnyway;
}

void VDFFVideoSource::GetVideoSourceInfo(VDXVideoSourceInfo& info)
{
  info.mFlags = VDXVideoSourceInfo::kFlagSyncDecode;
  info.mWidth = m_pCodecCtx->width;
  info.mHeight = m_pCodecCtx->height;
  info.mDecoderModel = VDXVideoSourceInfo::kDecoderModelCustom;
}

bool VDFFVideoSource::CreateVideoDecoder(IVDXVideoDecoder **ppDecoder)
{
  this->AddRef();
  *ppDecoder = this;
  return true;
}

bool VDFFVideoSource::CreateVideoDecoderModel(IVDXVideoDecoderModel **ppModel) 
{
	this->AddRef();
	*ppModel = this;
	return true;
}

void VDFFVideoSource::GetSampleInfo(sint64 sample, VDXVideoFrameInfo& frameInfo) 
{
  if(sample>=sample_count){
    VDFFVideoSource* v1 = 0;
    if(m_pSource->next_segment) v1 = m_pSource->next_segment->video_source;
    v1->GetSampleInfo(sample-sample_count,frameInfo);
    return;
  }

  frameInfo.mBytePosition = -1;
  frameInfo.mFrameType = kVDXVFT_Independent;
  if(keyframe_gap==1)
    frameInfo.mTypeChar = 'K';
  else if(IsKey(sample))
    frameInfo.mTypeChar = 'K';
  else
    frameInfo.mTypeChar = frame_type[sample];
}

bool VDFFVideoSource::IsKey(int64_t sample)
{
  if(sample>=sample_count){
    VDFFVideoSource* v1 = 0;
    if(m_pSource->next_segment) v1 = m_pSource->next_segment->video_source;
    return v1->IsKey(sample-sample_count);
  }

  if(is_image_list) return true;

  if(trust_index){
    return (m_pStreamCtx->index_entries[sample].flags & AVINDEX_KEYFRAME)!=0;
  }
  if(sparse_index){
    int64_t pos;
    int x = calc_sparse_key(sample,pos);
    return x==sample;
  }

  return false;
}

int64_t VDFFVideoSource::GetFrameNumberForSample(int64_t sample_num)
{
  return sample_num;
}

int64_t VDFFVideoSource::GetSampleNumberForFrame(int64_t display_num)
{
  return display_num;
}

int64_t VDFFVideoSource::GetRealFrame(int64_t display_num)
{
  return display_num;
}

int64_t VDFFVideoSource::GetSampleBytePosition(int64_t sample_num)
{
  return -1;
}

void VDFFVideoSource::Reset()
{
}

void VDFFVideoSource::SetDesiredFrame(int64_t frame_num)
{
  desired_frame = frame_num;
  required_count = 1;
}

int64_t VDFFVideoSource::GetNextRequiredSample(bool& is_preroll)
{
  is_preroll = false;
  return required_count ? desired_frame : -1;
}

int VDFFVideoSource::GetRequiredCount() 
{
  return required_count;
}

//////////////////////////////////////////////////////////////////////////
//Decoder
//////////////////////////////////////////////////////////////////////////\

const void* VDFFVideoSource::DecodeFrame(const void* inputBuffer, uint32_t data_len, bool is_preroll, int64_t streamFrame, int64_t targetFrame)
{
  m_pixmap_frame = int(targetFrame);
  m_pixmap_info.frame_num = -1;

  if(targetFrame>=sample_count){
    VDFFVideoSource* v1 = 0;
    if(m_pSource->next_segment) v1 = m_pSource->next_segment->video_source;
    if(!v1) return 0;
    return v1->DecodeFrame(inputBuffer,data_len,is_preroll,streamFrame,targetFrame-sample_count);
  }

  if(is_preroll) return 0;

  if(convertInfo.out_garbage){
    mContext.mpCallbacks->SetError("Segment has incompatible format: try changing decode format to RGBA");
    return 0;
  }

  BufferPage* page = frame_array[targetFrame];
  if(!page){
    // this now must be impossible with help of kFlagSyncDecode
    mContext.mpCallbacks->SetError("Cache overflow: set \"Performance \\ Video buffering\" to 32 or less");
    return 0;
  }
  if(page->error==BufferPage::err_badformat){
    mContext.mpCallbacks->SetError("Frame format is incompatible");
    return 0;
  }

  m_pixmap_info.frame_num = targetFrame;

  open_read(page);
  uint8_t* src = align_buf(page->p);

  if(convertInfo.direct_copy){
    set_pixmap_layout(src);
    return src;

  } else {
    int w = m_pixmap.w;
    int h = m_pixmap.h;

    AVFrame pic = {0};
    av_image_fill_arrays(pic.data, pic.linesize, src, frame_fmt, w, h, line_align);
    if(flip_image){
      pic.data[0] = pic.data[0] + pic.linesize[0]*(h-1);
      pic.linesize[0] = -pic.linesize[0];
    }

    AVFrame pic2 = {0};
    pic2.data[0] = (uint8_t*)m_pixmap.data;
    pic2.data[1] = (uint8_t*)m_pixmap.data2;
    pic2.data[2] = (uint8_t*)m_pixmap.data3;
    pic2.data[3] = (uint8_t*)m_pixmap.data4;
    pic2.linesize[0] = int(m_pixmap.pitch);
    pic2.linesize[1] = int(m_pixmap.pitch2);
    pic2.linesize[2] = int(m_pixmap.pitch3);
    pic2.linesize[3] = int(m_pixmap.pitch4);
    sws_scale(m_pSwsCtx, pic.data, pic.linesize, 0, h, pic2.data, pic2.linesize);
    return align_buf(m_pixmap_data);
  }
}

uint32_t VDFFVideoSource::GetDecodePadding() 
{
  return 0;
}

bool VDFFVideoSource::IsFrameBufferValid()
{
  if(m_pixmap_data) return true;
  if(m_pixmap_frame==-1) return false;

  if(m_pixmap_frame>=sample_count){
    VDFFVideoSource* v1 = 0;
    if(m_pSource->next_segment) v1 = m_pSource->next_segment->video_source;
    if(!v1) return 0;
    return v1->IsFrameBufferValid();
  }

  if(frame_array[m_pixmap_frame]) return true;
  return false;
}

const VDXPixmap& VDFFVideoSource::GetFrameBuffer()
{
  if(m_pixmap_frame>=sample_count){
    VDFFVideoSource* v1 = 0;
    if(m_pSource->next_segment) v1 = m_pSource->next_segment->video_source;
    return v1->GetFrameBuffer();
  }

  return m_pixmap;
}

const FilterModPixmapInfo& VDFFVideoSource::GetFrameBufferInfo()
{
  if(m_pixmap_frame>=sample_count){
    VDFFVideoSource* v1 = 0;
    if(m_pSource->next_segment) v1 = m_pSource->next_segment->video_source;
    return v1->GetFrameBufferInfo();
  }

  return m_pixmap_info;
}

const void* VDFFVideoSource::GetFrameBufferBase()
{
  if(m_pixmap_data) return align_buf(m_pixmap_data);
  if(m_pixmap_frame==-1) return 0;

  if(m_pixmap_frame>=sample_count){
    VDFFVideoSource* v1 = 0;
    if(m_pSource->next_segment) v1 = m_pSource->next_segment->video_source;
    if(!v1) return 0;
    return v1->GetFrameBufferBase();
  }

  return align_buf(frame_array[m_pixmap_frame]->p);
}

bool VDFFVideoSource::SetTargetFormat(int format, bool useDIBAlignment)
{
  nsVDXPixmap::VDXPixmapFormat opt_format = (nsVDXPixmap::VDXPixmapFormat)format;
  bool r = SetTargetFormat(opt_format,useDIBAlignment,0);
  if(!r && opt_format!=0) r = SetTargetFormat(nsVDXPixmap::kPixFormat_Null,useDIBAlignment,0);
  // note: compatibility between segments is enforced in SetTargetFormat
  VDFFInputFile* f1 = m_pSource;
  while(1){
    f1 = f1->next_segment;
    if(!f1) break;
    if(f1->video_source){
      bool r1 = f1->video_source->SetTargetFormat(opt_format,useDIBAlignment,this);
      if(!r1 && opt_format!=0) f1->video_source->SetTargetFormat(nsVDXPixmap::kPixFormat_Null,useDIBAlignment,this);
    }
  }
  return r;
}

bool VDFFVideoSource::SetTargetFormat(nsVDXPixmap::VDXPixmapFormat opt_format, bool useDIBAlignment,VDFFVideoSource* head)
{
  // this function will select one of the modes:
  // 1) XRGB - works slow
  // 2) convert to arbitrary rgb
  // 3) direct decoded format - usually some sort of yuv
  // 4) upsample arbitrary yuv to 444

  // yuv->rgb has chance to apply correct source color space matrix, range etc 
  // but! currently I notice huge color upsampling error

  // which is best default? rgb afraid to use; sws can do either fast-bad or slow-good, but vd can do good-fast-enough
  using namespace nsVDXPixmap;

  bool default_rgb = false;
  bool fast_rgb = false;

  if(convertInfo.ext_format && opt_format==kPixFormat_Null){
    convertInfo.ext_format = kPixFormat_Null;
    init_format();
  }

  VDXPixmapFormat base_format = kPixFormat_Null;
  VDXPixmapFormat ext_format = kPixFormat_Null;
  VDXPixmapFormat trigger = kPixFormat_Null;
  VDXPixmapFormat perfect_format = kPixFormat_Null;
  AVPixelFormat perfect_av_fmt = frame_fmt;
  AVPixelFormat src_fmt = frame_fmt;
  bool perfect_bitexact = false;
  bool skip_colorspace = false;

  convertInfo.req_format = opt_format;
  convertInfo.req_dib = useDIBAlignment;
  convertInfo.direct_copy = false;
  convertInfo.out_rgb = false;
  convertInfo.out_garbage = false;

  const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(frame_fmt);
  convertInfo.in_yuv = !(desc->flags & AV_PIX_FMT_FLAG_RGB) && desc->nb_components >= 3;
  convertInfo.in_subs = convertInfo.in_yuv && (desc->log2_chroma_w+desc->log2_chroma_h)>0;
  int src_max_value = (1 << desc->comp[0].depth)-1;

  switch(frame_fmt){
  case AV_PIX_FMT_AYUV64LE:
  case AV_PIX_FMT_AYUV64BE:
    perfect_format = kPixFormat_YUV444_Alpha_Planar;
    perfect_av_fmt = AV_PIX_FMT_YUVA444P;
    trigger = kPixFormat_YUV444_Planar;
    break;

  case AV_PIX_FMT_YUVA444P:
    perfect_format = kPixFormat_YUV444_Alpha_Planar;
    perfect_av_fmt = AV_PIX_FMT_YUVA444P;
    trigger = kPixFormat_YUV444_Planar;
    perfect_bitexact = true;
    break;

  case AV_PIX_FMT_YUVA422P:
    perfect_format = kPixFormat_YUV422_Alpha_Planar;
    perfect_av_fmt = AV_PIX_FMT_YUVA422P;
    trigger = kPixFormat_YUV422_Planar;
    perfect_bitexact = true;
    break;

  case AV_PIX_FMT_YUVA420P:
    perfect_format = kPixFormat_YUV420_Alpha_Planar;
    perfect_av_fmt = AV_PIX_FMT_YUVA420P;
    trigger = kPixFormat_YUV420_Planar;
    perfect_bitexact = true;
    break;

  case AV_PIX_FMT_YUVA444P9LE:
  case AV_PIX_FMT_YUVA444P10LE:
  case AV_PIX_FMT_YUVA444P16LE:
    perfect_format = kPixFormat_YUV444_Alpha_Planar16;
    perfect_av_fmt = AV_PIX_FMT_YUVA444P16LE;
    trigger = kPixFormat_YUV444_Planar16;
    perfect_bitexact = true;
    break;

  case AV_PIX_FMT_YUVA422P9LE:
  case AV_PIX_FMT_YUVA422P10LE:
  case AV_PIX_FMT_YUVA422P16LE:
    perfect_format = kPixFormat_YUV422_Alpha_Planar16;
    perfect_av_fmt = AV_PIX_FMT_YUVA422P16LE;
    trigger = kPixFormat_YUV422_Planar16;
    perfect_bitexact = true;
    break;

  case AV_PIX_FMT_YUVA420P9LE:
  case AV_PIX_FMT_YUVA420P10LE:
  case AV_PIX_FMT_YUVA420P16LE:
    perfect_format = kPixFormat_YUV420_Alpha_Planar16;
    perfect_av_fmt = AV_PIX_FMT_YUVA420P16LE;
    trigger = kPixFormat_YUV420_Planar16;
    perfect_bitexact = true;
    break;

  case AV_PIX_FMT_YUV420P9LE:
  case AV_PIX_FMT_YUV420P10LE:
  case AV_PIX_FMT_YUV420P12LE:
  case AV_PIX_FMT_YUV420P14LE:
  case AV_PIX_FMT_YUV420P16LE:
    perfect_format = kPixFormat_YUV420_Planar16;
    trigger = kPixFormat_YUV420_Planar16;
    perfect_bitexact = true;
    break;

  case AV_PIX_FMT_YUV422P9LE:
  case AV_PIX_FMT_YUV422P10LE:
  case AV_PIX_FMT_YUV422P12LE:
  case AV_PIX_FMT_YUV422P14LE:
  case AV_PIX_FMT_YUV422P16LE:
    perfect_format = kPixFormat_YUV422_Planar16;
    trigger = kPixFormat_YUV422_Planar16;
    perfect_bitexact = true;
    break;

  case AV_PIX_FMT_YUV444P9LE:
  case AV_PIX_FMT_YUV444P10LE:
  case AV_PIX_FMT_YUV444P12LE:
  case AV_PIX_FMT_YUV444P14LE:
  case AV_PIX_FMT_YUV444P16LE:
    perfect_format = kPixFormat_YUV444_Planar16;
    trigger = kPixFormat_YUV444_Planar16;
    perfect_bitexact = true;
    break;

  case AV_PIX_FMT_YUV420P:
  case AV_PIX_FMT_YUVJ420P:
    src_fmt = AV_PIX_FMT_YUV420P;
    perfect_format = kPixFormat_YUV420_Planar;
    trigger = kPixFormat_YUV420_Planar;
    perfect_bitexact = true;
    // examples: xvid
    // examples: gopro avc (FR)
    break;

  case AV_PIX_FMT_YUV422P:
  case AV_PIX_FMT_YUVJ422P:
    src_fmt = AV_PIX_FMT_YUV422P;
    perfect_format = kPixFormat_YUV422_Planar;
    trigger = kPixFormat_YUV422_Planar;
    perfect_bitexact = true;
    // examples: jpeg (FR)
    break;

  case AV_PIX_FMT_YUV440P:
  case AV_PIX_FMT_YUVJ440P:
    src_fmt = AV_PIX_FMT_YUV440P;
    perfect_format = kPixFormat_YUV444_Planar;
    perfect_av_fmt = AV_PIX_FMT_YUV444P;
    trigger = kPixFormat_YUV444_Planar;
    perfect_bitexact = false;
    // examples: 422 jpeg lossless-transposed (FR)
    break;

  case AV_PIX_FMT_UYVY422:
    perfect_format = kPixFormat_YUV422_UYVY;
    trigger = kPixFormat_YUV422_UYVY;
    perfect_bitexact = true;
    //! not tested at all
    break;

  case AV_PIX_FMT_YUYV422:
    perfect_format = kPixFormat_YUV422_YUYV;
    trigger = kPixFormat_YUV422_YUYV;
    perfect_bitexact = true;
    // examples: cineform
    break;

  case AV_PIX_FMT_YUV444P:
  case AV_PIX_FMT_YUVJ444P:
    src_fmt = AV_PIX_FMT_YUV444P;
    perfect_format = kPixFormat_YUV444_Planar;
    perfect_av_fmt = AV_PIX_FMT_YUV444P;
    trigger = kPixFormat_YUV444_Planar;
    perfect_bitexact = true;
    // examples: 444 jpeg by photoshop (FR)
    break;

  case AV_PIX_FMT_YUV411P:
    src_fmt = AV_PIX_FMT_YUV411P;
    perfect_format = kPixFormat_YUV411_Planar;
    perfect_av_fmt = AV_PIX_FMT_YUV411P;
    trigger = kPixFormat_YUV411_Planar;
    perfect_bitexact = true;
    // examples: DV
    break;

  case AV_PIX_FMT_BGR24:
    perfect_format = kPixFormat_RGB888;
    trigger = kPixFormat_RGB888;
    perfect_bitexact = true;
    // examples: tga24
    break;

  case AV_PIX_FMT_BGRA:
  case AV_PIX_FMT_BGR0:
    perfect_format = kPixFormat_XRGB8888;
    trigger = kPixFormat_XRGB8888;
    perfect_bitexact = true;
    // examples: tga32
    break;

  case AV_PIX_FMT_BGRA64:
    perfect_format = (VDXPixmapFormat)kPixFormat_XRGB64;
    perfect_av_fmt = AV_PIX_FMT_BGRA64;
    trigger = kPixFormat_XRGB64;
    perfect_bitexact = true;
    break;

  case AV_PIX_FMT_GRAY8:
    perfect_format = (VDXPixmapFormat)kPixFormat_Y8;
    perfect_av_fmt = AV_PIX_FMT_GRAY8;
    trigger = kPixFormat_Y8;
    perfect_bitexact = true;
    break;

  case AV_PIX_FMT_GRAY16:
    perfect_format = (VDXPixmapFormat)kPixFormat_Y16;
    perfect_av_fmt = AV_PIX_FMT_GRAY16;
    trigger = kPixFormat_Y16;
    perfect_bitexact = true;
    break;

  default:
    perfect_format = kPixFormat_XRGB8888;
    perfect_av_fmt = AV_PIX_FMT_BGRA;
    trigger = kPixFormat_XRGB8888;
    perfect_bitexact = false;
    // examples: utvideo rgb (AV_PIX_FMT_RGB24) 

    if(desc->flags & AV_PIX_FMT_FLAG_RGB && desc->comp[0].depth>8){
      // examples: sgi - RGB48BE, tiff - RGB48LE/BE, RGBA64LE/BE
      perfect_format = (VDXPixmapFormat)kPixFormat_XRGB64;
      perfect_av_fmt = AV_PIX_FMT_BGRA64;
      trigger = (VDXPixmapFormat)kPixFormat_XRGB64;
      perfect_bitexact = false;
    }
  }

  if(opt_format==0){
    if(default_rgb){
      base_format = kPixFormat_XRGB8888;
      convertInfo.av_fmt = AV_PIX_FMT_BGRA;
      if(base_format==perfect_format) convertInfo.direct_copy = perfect_bitexact;
    } else {
      base_format = perfect_format;
      convertInfo.av_fmt = perfect_av_fmt;
      convertInfo.direct_copy = perfect_bitexact;
    }

  } else if(direct_cfhd){
    switch(opt_format){
    case kPixFormat_RGB888:
      if(cfhd_test_format(m_pCodecCtx,opt_format)){
        base_format = kPixFormat_RGB888;
        ext_format = kPixFormat_RGB888;
        convertInfo.av_fmt = AV_PIX_FMT_BGR24;
        convertInfo.direct_copy = true;
        break;
      }
      return false;

    case kPixFormat_XRGB8888:
      if(cfhd_test_format(m_pCodecCtx,opt_format)){
        base_format = kPixFormat_XRGB8888;
        ext_format = kPixFormat_XRGB8888;
        convertInfo.av_fmt = AV_PIX_FMT_BGRA;
        convertInfo.direct_copy = true;
        break;
      }
      return false;

    case kPixFormat_YUV422_YUYV:
    case kPixFormat_YUV422_UYVY:
    case kPixFormat_YUV422_Planar:
      if(cfhd_test_format(m_pCodecCtx,kPixFormat_YUV422_YUYV)){
        base_format = kPixFormat_YUV422_YUYV;
        ext_format = kPixFormat_YUV422_YUYV;
        convertInfo.av_fmt = AV_PIX_FMT_YUYV422;
        convertInfo.direct_copy = true;
        break;
      }
      return false;

    case kPixFormat_YUV422_V210:
      if(cfhd_test_format(m_pCodecCtx,kPixFormat_YUV422_V210)){
        base_format = kPixFormat_YUV422_V210;
        ext_format = kPixFormat_YUV422_V210;
        convertInfo.av_fmt = AV_PIX_FMT_BGR24;
        convertInfo.direct_copy = true;
        break;
      }
      return false;

    case kPixFormat_YUV422_Planar16:
      if(cfhd_test_format(m_pCodecCtx,kPixFormat_YUV422_YU64)){
        base_format = kPixFormat_YUV422_YU64;
        ext_format = kPixFormat_YUV422_YU64;
        convertInfo.av_fmt = AV_PIX_FMT_BGR32;
        convertInfo.direct_copy = true;
        break;
      }
      return false;

    case kPixFormat_R210:
      if(cfhd_test_format(m_pCodecCtx,opt_format)){
        base_format = kPixFormat_R210;
        ext_format = kPixFormat_R210;
        convertInfo.av_fmt = AV_PIX_FMT_BGRA;
        convertInfo.direct_copy = true;
        break;
      }
      return false;

    case kPixFormat_XRGB64:
      if(cfhd_test_format(m_pCodecCtx,opt_format)){
        base_format = kPixFormat_XRGB64;
        ext_format = kPixFormat_XRGB64;
        convertInfo.av_fmt = AV_PIX_FMT_BGRA64;
        convertInfo.direct_copy = true;
        break;
      }
      return false;

    case kPixFormat_Y8:
    case kPixFormat_Y8_FR:
    case kPixFormat_Y16:
      if(cfhd_test_format(m_pCodecCtx,kPixFormat_Y16)){
        base_format = kPixFormat_Y16;
        ext_format = kPixFormat_Y16;
        convertInfo.av_fmt = AV_PIX_FMT_GRAY16;
        convertInfo.direct_copy = true;
        break;
      }
      return false;

    default:
      return false;
    }
  } else {
    switch(opt_format){
    case kPixFormat_YUV420_Planar:
    case kPixFormat_YUV422_Planar:
    case kPixFormat_YUV411_Planar:
    case kPixFormat_YUV422_UYVY:
    case kPixFormat_YUV422_YUYV:
      if(opt_format==trigger){
        base_format = perfect_format;
        convertInfo.av_fmt = perfect_av_fmt;
        convertInfo.direct_copy = perfect_bitexact;
      } else return false;
      break;

    case kPixFormat_YUV444_Planar:
      if(opt_format==trigger){
        base_format = perfect_format;
        convertInfo.av_fmt = perfect_av_fmt;
        convertInfo.direct_copy = perfect_bitexact;

      } else if(convertInfo.in_yuv){
        base_format = kPixFormat_YUV444_Planar;
        convertInfo.av_fmt = AV_PIX_FMT_YUV444P;
      } else return false;
      break;

    case kPixFormat_YUV444_Planar16:
      if(opt_format==trigger){
        base_format = perfect_format;
        convertInfo.av_fmt = perfect_av_fmt;
        convertInfo.direct_copy = perfect_bitexact;

      } else if(convertInfo.in_yuv){
        base_format = kPixFormat_YUV444_Planar16;
        convertInfo.av_fmt = AV_PIX_FMT_YUV444P16;
      } else return false;
      break;

    case kPixFormat_XRGB64:
      if(opt_format==perfect_format){
        base_format = perfect_format;
        convertInfo.av_fmt = perfect_av_fmt;
        convertInfo.direct_copy = perfect_bitexact;
      } else {
        base_format = kPixFormat_XRGB64;
        convertInfo.av_fmt = AV_PIX_FMT_BGRA64;
        convertInfo.direct_copy = false;
      }
      break;

    case kPixFormat_XRGB1555:
      if(opt_format==perfect_format){
        base_format = perfect_format;
        convertInfo.av_fmt = perfect_av_fmt;
        convertInfo.direct_copy = perfect_bitexact;
      } else {
        base_format = kPixFormat_XRGB1555;
        convertInfo.av_fmt = AV_PIX_FMT_RGB555;
        convertInfo.direct_copy = false;
      }
      break;

    case kPixFormat_RGB565:
      if(opt_format==perfect_format){
        base_format = perfect_format;
        convertInfo.av_fmt = perfect_av_fmt;
        convertInfo.direct_copy = perfect_bitexact;
      } else {
        base_format = kPixFormat_RGB565;
        convertInfo.av_fmt = AV_PIX_FMT_RGB565;
        convertInfo.direct_copy = false;
      }
      break;

    case kPixFormat_RGB888:
      if(opt_format==perfect_format){
        base_format = perfect_format;
        convertInfo.av_fmt = perfect_av_fmt;
        convertInfo.direct_copy = perfect_bitexact;
      } else {
        base_format = kPixFormat_RGB888;
        convertInfo.av_fmt = AV_PIX_FMT_BGR24;
        convertInfo.direct_copy = false;
      }
      break;

    case kPixFormat_XRGB8888:
      if(opt_format==perfect_format){
        base_format = perfect_format;
        convertInfo.av_fmt = perfect_av_fmt;
        convertInfo.direct_copy = perfect_bitexact;
      } else {
        base_format = kPixFormat_XRGB8888;
        convertInfo.av_fmt = AV_PIX_FMT_BGRA;
        convertInfo.direct_copy = false;
      }
      break;

    default:
      return false;
    }
  }

  if(convertInfo.direct_copy) convertInfo.req_dib = false;

  const AVPixFmtDescriptor* out_desc = av_pix_fmt_desc_get(convertInfo.av_fmt);
  convertInfo.out_rgb = (out_desc->flags & AV_PIX_FMT_FLAG_RGB) && out_desc->nb_components >= 3;

  // tweak output yuv formats for VD here
  VDXPixmapFormat format = base_format;

  /*if(format==kPixFormat_YUV420_Planar && convertInfo.av_fmt==AV_PIX_FMT_YUV420P && opt_format!=kPixFormat_YUV420_Planar){
    AVFieldOrder fo = m_pCodecCtx->field_order;
    if(fo==AV_FIELD_TT) format = kPixFormat_YUV420it_Planar;
    if(fo==AV_FIELD_BB) format = kPixFormat_YUV420ib_Planar;
  }*/

  if(!skip_colorspace && m_pCodecCtx->color_range==AVCOL_RANGE_JPEG) switch(format){
  case kPixFormat_YUV420_Planar:
    format = kPixFormat_YUV420_Planar_FR;
    break;
  case kPixFormat_YUV420it_Planar:
    format = kPixFormat_YUV420it_Planar_FR;
    break;
  case kPixFormat_YUV420ib_Planar:
    format = kPixFormat_YUV420ib_Planar_FR;
    break;
  case kPixFormat_YUV422_Planar:
    format = kPixFormat_YUV422_Planar_FR;
    break;
  case kPixFormat_YUV444_Planar:
    format = kPixFormat_YUV444_Planar_FR;
    break;
  case kPixFormat_YUV422_UYVY:
    format = kPixFormat_YUV422_UYVY_FR;
    break;
  case kPixFormat_YUV422_YUYV:
    format = kPixFormat_YUV422_YUYV_FR;
    break;
  case kPixFormat_Y8:
    format = kPixFormat_Y8_FR;
    break;
  }

  if(!skip_colorspace && m_pCodecCtx->colorspace==AVCOL_SPC_BT709) switch(format){
  case kPixFormat_YUV420_Planar:
    format = kPixFormat_YUV420_Planar_709;
    break;
  case kPixFormat_YUV420_Planar_FR:
    format = kPixFormat_YUV420_Planar_709_FR;
    break;
  case kPixFormat_YUV420it_Planar:
    format = kPixFormat_YUV420it_Planar_709;
    break;
  case kPixFormat_YUV420it_Planar_FR:
    format = kPixFormat_YUV420it_Planar_709_FR;
    break;
  case kPixFormat_YUV420ib_Planar:
    format = kPixFormat_YUV420ib_Planar_709;
    break;
  case kPixFormat_YUV420ib_Planar_FR:
    format = kPixFormat_YUV420ib_Planar_709_FR;
    break;
  case kPixFormat_YUV422_Planar:
    format = kPixFormat_YUV422_Planar_709;
    break;
  case kPixFormat_YUV422_Planar_FR:
    format = kPixFormat_YUV422_Planar_709_FR;
    break;
  case kPixFormat_YUV444_Planar:
    format = kPixFormat_YUV444_Planar_709;
    break;
  case kPixFormat_YUV444_Planar_FR:
    format = kPixFormat_YUV444_Planar_709_FR;
    break;
  case kPixFormat_YUV422_UYVY:
    format = kPixFormat_YUV422_UYVY_709;
    break;
  case kPixFormat_YUV422_UYVY_FR:
    format = kPixFormat_YUV422_UYVY_709_FR;
    break;
  case kPixFormat_YUV422_YUYV:
    format = kPixFormat_YUV422_YUYV_709;
    break;
  case kPixFormat_YUV422_YUYV_FR:
    format = kPixFormat_YUV422_YUYV_709_FR;
    break;
  }

  if(head && head->m_pixmap.format!=format){
    format = (VDXPixmapFormat)head->m_pixmap.format;
    ext_format = head->convertInfo.ext_format;
    convertInfo.av_fmt = head->convertInfo.av_fmt;
    convertInfo.direct_copy = false;
    convertInfo.out_garbage = true;
  }

  if(ext_format!=convertInfo.ext_format){
    convertInfo.ext_format = ext_format;
    init_format();
    desc = av_pix_fmt_desc_get(frame_fmt);
  }

  memset(&m_pixmap,0,sizeof(m_pixmap));
  m_pixmap.format = format;
  int32_t w = m_pCodecCtx->width;
  int32_t h = m_pCodecCtx->height;
  m_pixmap.w = w;
  m_pixmap.h = h;

  m_pixmap_info.clear();
  switch(format){
  case kPixFormat_XRGB64:
    m_pixmap_info.ref_r = 0xFFFF;
    m_pixmap_info.ref_g = 0xFFFF;
    m_pixmap_info.ref_b = 0xFFFF;
    m_pixmap_info.ref_a = 0xFFFF;
    break;
  case kPixFormat_Y16:
    m_pixmap_info.ref_r = 0xFFFF;
    if(m_pCodecCtx->color_range==AVCOL_RANGE_MPEG) m_pixmap_info.colorRangeMode = kColorRangeMode_Limited;
    if(m_pCodecCtx->color_range==AVCOL_RANGE_JPEG) m_pixmap_info.colorRangeMode = kColorRangeMode_Full;
    break;
  case kPixFormat_YUV422_V210:
  case kPixFormat_YUV422_YU64:
  case kPixFormat_YUV420_Planar16:
  case kPixFormat_YUV422_Planar16:
  case kPixFormat_YUV444_Planar16:
  case kPixFormat_YUV420_Alpha_Planar:
  case kPixFormat_YUV422_Alpha_Planar:
  case kPixFormat_YUV444_Alpha_Planar:
  case kPixFormat_YUV420_Alpha_Planar16:
  case kPixFormat_YUV422_Alpha_Planar16:
  case kPixFormat_YUV444_Alpha_Planar16:
    m_pixmap_info.ref_r = 0xFFFF;
    m_pixmap_info.ref_a = 0xFFFF;
    if(convertInfo.direct_copy){
      m_pixmap_info.ref_r = src_max_value;
      m_pixmap_info.ref_a = src_max_value;
    }
    if(m_pCodecCtx->colorspace==AVCOL_SPC_BT709) m_pixmap_info.colorSpaceMode = kColorSpaceMode_709;
    if(m_pCodecCtx->color_range==AVCOL_RANGE_MPEG) m_pixmap_info.colorRangeMode = kColorRangeMode_Limited;
    if(m_pCodecCtx->color_range==AVCOL_RANGE_JPEG) m_pixmap_info.colorRangeMode = kColorRangeMode_Full;
    break;
  }
  if((desc->flags & AV_PIX_FMT_FLAG_ALPHA) && (out_desc->flags & AV_PIX_FMT_FLAG_ALPHA)){
    m_pixmap_info.alpha_type = FilterModPixmapInfo::kAlphaMask;
    switch(m_pCodecCtx->codec_id){
    case AV_CODEC_ID_GIF:
    case AV_CODEC_ID_PNG:
    case AV_CODEC_ID_TARGA:
    case AV_CODEC_ID_WEBP:
    case AV_CODEC_ID_PSD:
    case AV_CODEC_ID_VP8:
    case AV_CODEC_ID_VP9:
      m_pixmap_info.alpha_type = FilterModPixmapInfo::kAlphaOpacity;
      break;
    }
  }
  if((frame_fmt==AV_PIX_FMT_PAL8) && (out_desc->flags & AV_PIX_FMT_FLAG_ALPHA)){
    switch(m_pCodecCtx->codec_id){
    case AV_CODEC_ID_GIF:
    case AV_CODEC_ID_PNG:
      m_pixmap_info.alpha_type = FilterModPixmapInfo::kAlphaOpacity;
      break;
    }
  }
  if(direct_cfhd) cfhd_get_info(m_pCodecCtx,m_pixmap_info);

  if(convertInfo.direct_copy || convertInfo.out_garbage){
    free(m_pixmap_data);
    m_pixmap_data = 0;

  } else {
    uint32_t size = av_image_get_buffer_size(convertInfo.av_fmt,w,h,line_align);
    m_pixmap_data = (uint8_t*)realloc(m_pixmap_data, size+line_align-1);
    set_pixmap_layout(align_buf(m_pixmap_data));
    if(m_pSwsCtx) sws_freeContext(m_pSwsCtx);
    int flags = 0;
    if(convertInfo.in_subs){
      // bicubic is needed to best preserve detail, ex color_420.jpg
      // bilinear also not bad if the material is blurry
      flags |= SWS_BICUBIC;
    } else {
      // this is needed to fight smearing when no upsampling is actually used, ex color_444.jpg
      flags |= SWS_POINT;
    }

    // these flags are needed to avoid really bad optimized conversion resulting in corrupt image, ex color_420.jpg
    // however this is also 5x slower!
    flags |= SWS_FULL_CHR_H_INT|SWS_ACCURATE_RND;
    if(fast_rgb) flags = SWS_POINT;
    #ifdef FFDEBUG
    flags |= SWS_PRINT_INFO;
    #endif

    int proxy_max_value = 0;
    AVPixelFormat proxy_fmt = AV_PIX_FMT_NONE;
    if(format==kPixFormat_XRGB64) switch(src_fmt){
    case AV_PIX_FMT_GBRP9LE:
      proxy_max_value = 0x01FF;
      proxy_fmt = AV_PIX_FMT_GBRP16LE;
      break;
    case AV_PIX_FMT_GBRP10LE:
      proxy_max_value = 0x03FF;
      proxy_fmt = AV_PIX_FMT_GBRP16LE;
      break;
    case AV_PIX_FMT_GBRP12LE:
      proxy_max_value = 0x0FFF;
      proxy_fmt = AV_PIX_FMT_GBRP16LE;
      break;
    case AV_PIX_FMT_GBRP14LE:
      proxy_max_value = 0x3FFF;
      proxy_fmt = AV_PIX_FMT_GBRP16LE;
      break;
    case AV_PIX_FMT_GBRP16LE:
      proxy_max_value = 0xFFFF;
      proxy_fmt = AV_PIX_FMT_GBRP16LE;
      break;

    case AV_PIX_FMT_GBRP9BE:
      proxy_max_value = 0x01FF;
      proxy_fmt = AV_PIX_FMT_GBRP16BE;
      break;
    case AV_PIX_FMT_GBRP10BE:
      proxy_max_value = 0x03FF;
      proxy_fmt = AV_PIX_FMT_GBRP16BE;
      break;
    case AV_PIX_FMT_GBRP12BE:
      proxy_max_value = 0x0FFF;
      proxy_fmt = AV_PIX_FMT_GBRP16BE;
      break;
    case AV_PIX_FMT_GBRP14BE:
      proxy_max_value = 0x3FFF;
      proxy_fmt = AV_PIX_FMT_GBRP16BE;
      break;
    case AV_PIX_FMT_GBRP16BE:
      proxy_max_value = 0xFFFF;
      proxy_fmt = AV_PIX_FMT_GBRP16BE;
      break;

    case AV_PIX_FMT_GBRAP10LE:
      proxy_max_value = 0x03FF;
      proxy_fmt = AV_PIX_FMT_GBRAP16LE;
      break;
    case AV_PIX_FMT_GBRAP12LE:
      proxy_max_value = 0x0FFF;
      proxy_fmt = AV_PIX_FMT_GBRAP16LE;
      break;
    case AV_PIX_FMT_GBRAP16LE:
      proxy_max_value = 0xFFFF;
      proxy_fmt = AV_PIX_FMT_GBRAP16LE;
      break;

    case AV_PIX_FMT_GBRAP10BE:
      proxy_max_value = 0x03FF;
      proxy_fmt = AV_PIX_FMT_GBRAP16BE;
      break;
    case AV_PIX_FMT_GBRAP12BE:
      proxy_max_value = 0x0FFF;
      proxy_fmt = AV_PIX_FMT_GBRAP16BE;
      break;
    case AV_PIX_FMT_GBRAP16BE:
      proxy_max_value = 0xFFFF;
      proxy_fmt = AV_PIX_FMT_GBRAP16BE;
      break;
    }
    if(proxy_fmt!=AV_PIX_FMT_NONE){
      m_pixmap_info.ref_r = proxy_max_value;
      m_pixmap_info.ref_g = proxy_max_value;
      m_pixmap_info.ref_b = proxy_max_value;
      m_pixmap_info.ref_a = proxy_max_value;
      src_fmt = proxy_fmt;
    }

    m_pSwsCtx = sws_getContext(w, h, src_fmt, w, h, convertInfo.av_fmt, flags, 0, 0, 0);
    if(convertInfo.in_yuv && convertInfo.out_rgb){
      // range and color space only makes sence for yuv->rgb
      // rgb->rgb is always exact
      // rgb->yuv is useless as input
      // yuv->yuv better keep unchanged to save precision
      // rgb output is always full range
      const int* src_matrix = sws_getCoefficients(m_pCodecCtx->colorspace);
      int src_range = m_pCodecCtx->color_range==AVCOL_RANGE_JPEG ? 1:0;

      int* t1; int* t2; int r1,r2; int p0,p1,p2;
      sws_getColorspaceDetails(m_pSwsCtx,&t1,&r1,&t2,&r2,&p0,&p1,&p2);
      sws_setColorspaceDetails(m_pSwsCtx, src_matrix, src_range, t2, r2, p0,p1,p2);
    }
  }

  return true;
}

void VDFFVideoSource::set_pixmap_layout(uint8_t* p)
{
  using namespace nsVDXPixmap;

  int w = m_pixmap.w;
  int h = m_pixmap.h;

  AVFrame pic = {0};
  av_image_fill_arrays(pic.data, pic.linesize, p, convertInfo.av_fmt, w, h, line_align);

  if(m_pixmap.format==kPixFormat_YUV422_V210){
    int row = (w+47)/48*128;
    pic.linesize[0] = row;
  }

  m_pixmap.palette = 0;
  m_pixmap.data = pic.data[0];
  m_pixmap.data2 = pic.data[1];
  m_pixmap.data3 = pic.data[2];
  m_pixmap.data4 = pic.data[3];
  m_pixmap.pitch = pic.linesize[0];
  m_pixmap.pitch2 = pic.linesize[1];
  m_pixmap.pitch3 = pic.linesize[2];
  m_pixmap.pitch4 = pic.linesize[3];

  if(convertInfo.req_dib^flip_image){
    switch(m_pixmap.format){
    case kPixFormat_XRGB1555:
    case kPixFormat_RGB565:
    case kPixFormat_RGB888:
    case kPixFormat_XRGB8888:
      m_pixmap.data = pic.data[0] + pic.linesize[0]*(h-1);
      m_pixmap.pitch = -pic.linesize[0];
      break;
    }
  }
}

bool VDFFVideoSource::SetDecompressedFormat(const VDXBITMAPINFOHEADER *pbih) 
{
  return false;
}

bool VDFFVideoSource::IsDecodable(int64_t sample_num64) 
{
  return true;
}

int64_t VDFFVideoSource::frame_to_pts_next(sint64 start)
{
  if(trust_index){
    int next_key = -1;
    {for(int i=(int)start; i<m_pStreamCtx->nb_index_entries; i++)
      if(m_pStreamCtx->index_entries[i].flags & AVINDEX_KEYFRAME){ next_key=i; break; } }
    if(next_key==-1) return -1;
    int64_t pos = m_pStreamCtx->index_entries[next_key].timestamp;
    return pos;
  } else {
    int64_t pos = start*time_base.den / time_base.num + start_time;
    return pos;
  }
}

// return frame num (guessed)
int VDFFVideoSource::calc_sparse_key(int64_t sample, int64_t& pos)
{
  // somehow start_time is not included in the index timestamps
  // works with 10 bit.mp4: sample*den/num = exactly key timestamp
  // half-frame bias helps with some rounding noise
  // works with 2017-04-07 08-53-48.flv
  int rd = time_base.den/2;
  int64_t pos1 = (sample*time_base.den + rd) / time_base.num;
  int x = av_index_search_timestamp(m_pStreamCtx,pos1,AVSEEK_FLAG_BACKWARD);
  if(x==-1) return -1;
  pos = m_pStreamCtx->index_entries[x].timestamp;
  int frame = int((pos*time_base.num + rd) / time_base.den);
  return frame;
}

int VDFFVideoSource::calc_seek(int jump, int64_t& pos)
{
  if(is_image_list){
    if(next_frame==-1 || jump>next_frame+fw_seek_threshold || jump<next_frame){
      pos = int64_t(jump)*time_base.den / time_base.num + start_time;
      return jump;
    }
    return -1;
  }

  if(trust_index && jump>next_frame){
    int next_key = -1;
    {for(int i=jump; i>next_frame; i--)
      if(m_pStreamCtx->index_entries[i].flags & AVINDEX_KEYFRAME){ next_key=i; break; } }

    if(next_key!=-1 && (next_frame==-1 || next_key>next_frame+fw_seek_threshold)){
      // required to seek forward
      pos = m_pStreamCtx->index_entries[next_key].timestamp;
      return next_key;
    }
  }

  if(trust_index && jump<next_frame){
    // required to seek backward
    int prev_key = 0;
    {for(int i=jump; i>=0; i--)
      if(m_pStreamCtx->index_entries[i].flags & AVINDEX_KEYFRAME){ prev_key=i; break; } }
    pos = m_pStreamCtx->index_entries[prev_key].timestamp;
    return prev_key;
  }

  if(!trust_index && (next_frame==-1 || jump>next_frame+fw_seek_threshold || jump<next_frame)){
    if(jump>=dead_range_start && jump<=dead_range_end) return -1;
    if(jump==last_seek_frame) return -1;

    // required to seek somewhere
    pos = int64_t(jump)*time_base.den / time_base.num + start_time;
    int dst = jump;

    if(!(m_pFormatCtx->iformat->flags & AVFMT_SEEK_TO_PTS)){
      // because seeking works on DTS it needs some unknown offset to work
      pos -= int64_t(8)*time_base.den / time_base.num; // better than nothing
      dst -= 8;
      if(pos<0) pos = 0;
      if(dst<0) dst = 0;
    }
    if(sparse_index){
      // when jumping to keys and we can guess key locations, use that
      // works with 2017-04-07 08-53-48.flv
      int64_t pos1;
      int dst1 = calc_sparse_key(jump,pos1);
      // only apply this correction when moving through keys
      if(dst1==jump){
        pos = pos1;
        dst = dst1;
      }
    }

    if(jump==0){
      pos = AV_SEEK_START;
      dst = 0;
    }

    return dst;
  }

  return -1;
}

int VDFFVideoSource::calc_prefetch(int jump)
{
  if(keyframe_gap==1) return -1;
  if(small_cache_mode || copy_mode) return -1;
  if(jump>=last_request) return -1;
  if(!enable_prefetch) return -1; // do not activate until first explicit backward seek
  if(dead_range_start!=-1) return -1; // in such case we have much worse problem than prefetch is meant so solve
  if(!trust_index && !sparse_index) return -1;

  int x = -1;
  int n = 0;
  {for(int i=jump; i>=0; i--){
    if(i<jump-keyframe_gap) break;
    if(!frame_array[i]){ x=i; break; }
  }}
  if(x==-1) return -1; // all nearby frames are already cached

  int prev_key = 0;
  if(trust_index){
    {for(int i=x; i>=0; i--)
      if(m_pStreamCtx->index_entries[i].flags & AVINDEX_KEYFRAME){ prev_key=i; break; } }
  }
  if(sparse_index){
    int64_t pos;
    prev_key = calc_sparse_key(x,pos);
  }

  if(jump-prev_key>buffer_reserve) return -1; // impossible to hold both ends of cache
  return x;
}

bool VDFFVideoSource::Read(sint64 start, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead)
{
  if(start>=sample_count){
    VDFFVideoSource* v1 = 0;
    if(m_pSource->next_segment) v1 = m_pSource->next_segment->video_source;
    if(v1) return v1->Read(start-sample_count,lCount,lpBuffer,cbBuffer,lBytesRead,lSamplesRead);
  }

  if(start==sample_count){
    *lBytesRead = 0;
    *lSamplesRead = 0;
    return true;
  }

  *lBytesRead = 0;
  *lSamplesRead = 1;

  if(copy_mode && copy_pkt.data){
    *lBytesRead = copy_pkt.size;
    if(!lpBuffer) return true;
    if(cbBuffer<uint32(copy_pkt.size)) return false;
    memcpy(lpBuffer,copy_pkt.data,copy_pkt.size);
    av_packet_unref(&copy_pkt);
    return true;
  }

  if(copy_mode && frame_type[start]=='D'){
    *lBytesRead = 0;
    return true;
  }

  if(!copy_mode){
    // 0 bytes identifies "drop frame"
    int size = 1;
    if(frame_type[start]=='D') size = 0;
    *lBytesRead = size;
    if(lpBuffer) memset(lpBuffer,0,size);
    else return true;
  }

  av_packet_unref(&copy_pkt);
  if(copy_mode && start!=next_frame){
    free_buffers();
  }

  VDFFVideoSource* head = this;
  if(m_pSource->head_segment) head = m_pSource->head_segment->video_source;
  if(head->required_count) head->required_count--;

  int jump = (int)start;
  if(!copy_mode && frame_array[jump]){
    jump = calc_prefetch(jump);
    if(jump==-1) return true;
  }

  last_request = (int)start;

  if(start>=dead_range_start && start<=dead_range_end){
    // we already known this does not work so fail fast
    mContext.mpCallbacks->SetError("requested frame not found; next valid frame = %d", next_frame-1);
    return false;
  }

  int64_t seek_pos;
  int seek_frame = calc_seek(jump,seek_pos);
  if(seek_frame!=-1){
    if(jump<next_frame) enable_prefetch = true;

    avcodec_flush_buffers(m_pCodecCtx);
    ::seek_frame(m_pFormatCtx,m_streamIndex,seek_pos,AVSEEK_FLAG_BACKWARD);
    if(trust_index || is_image_list) next_frame = seek_frame; else next_frame = -1;

    // this helps to prevent seeking again to satisfy same request
    if(!trust_index) last_seek_frame = jump;
  }

  while(1){
    if(!read_frame(start)){
      bool fail = true;
      if(next_frame>0){
        // end of stream, fill with dups
        BufferPage* page = frame_array[next_frame-1];
        if(page){
          copy_page(next_frame, int(start), page);
          next_frame = int(start)+1;
          fail = false;
        }
      }
      if(fail){
        mContext.mpCallbacks->SetError("unexpected end of stream");
        return false;
      }
    }

    if(copy_mode && copy_pkt.data){
      *lBytesRead = copy_pkt.size;
      if(!lpBuffer) return true;
      if(cbBuffer<uint32(copy_pkt.size)) return false;
      memcpy(lpBuffer,copy_pkt.data,copy_pkt.size);
      av_packet_unref(&copy_pkt);
      return true;
    }

    if(!copy_mode && frame_array[start]) return true;

    //! missed seek or bad stream, just fail
    // better idea is to build new corrected index maybe
    if(next_frame>start){
      dead_range_start = start;
      dead_range_end = next_frame-2;
      mContext.mpCallbacks->SetError("requested frame not found; next valid frame = %d", next_frame-1);
      return false;
    }
  }

  return false;
}

bool VDFFVideoSource::read_frame(sint64 desired_frame, bool init)
{
  AVPacket pkt;
  pkt.data = 0;
  pkt.size = 0;

  if(copy_mode && !decode_mode){
    while(1){
      int rf = av_read_frame(m_pFormatCtx, &pkt);
      if(rf<0) return false;
      bool done = false;
      if(pkt.stream_index == m_streamIndex){
        int pos = handle_frame_num(pkt.pts,pkt.dts);
        if(pos==-1 || pos>desired_frame){
          av_packet_unref(&pkt);
          return false;
        }
        next_frame = pos+1;
        if(pos==desired_frame){
          av_packet_unref(&copy_pkt);
          av_packet_ref(&copy_pkt,&pkt);
          done = true;
        }
      }
      av_packet_unref(&pkt);
      if(done) return true;
    }
  }

  while(1){
    int rf = av_read_frame(m_pFormatCtx, &pkt);
    if(rf<0){
      pkt.data = 0;
      pkt.size = 0;
      // end of stream, grab buffered images
      pkt.stream_index = m_streamIndex;
      while(1){
        if(direct_buffer) alloc_direct_buffer();
        avcodec_send_packet(m_pCodecCtx, &pkt);
        int f = avcodec_receive_frame(m_pCodecCtx, frame);
        if(f!=0) return false;
        if(init){
          init = false;
          set_start_time();
        }
        handle_frame();
        av_frame_unref(frame);
        return true;
      }

    } else {
      int done_frames = 0;
      if(pkt.stream_index == m_streamIndex){
        if(direct_buffer) alloc_direct_buffer();
        avcodec_send_packet(m_pCodecCtx, &pkt);
        int f = avcodec_receive_frame(m_pCodecCtx, frame);
        if(f==0){
          if(init){
            init = false;
            set_start_time();
          }
          int pos = handle_frame();
          av_frame_unref(frame);
          done_frames++;
          if(copy_mode && pos==desired_frame){
            av_packet_unref(&copy_pkt);
            av_packet_ref(&copy_pkt,&pkt);
          }
        }
      }
      av_packet_unref(&pkt);
      if(done_frames>0) return true;
    }
  }
}

void VDFFVideoSource::set_start_time()
{
  // this is used for audio sync
  int64_t t1 = frame->pts;
  if(t1==AV_NOPTS_VALUE) t1 = m_pStreamCtx->start_time;
  if(t1!=AV_NOPTS_VALUE) 
    m_pSource->video_start_time = t1;

  // this is used for seeking etc
  int64_t t2 = frame->pts;
  if(t2==AV_NOPTS_VALUE) t2 = frame->pkt_dts;
  start_time = t2;
  if(frame_fmt==-1) init_format();
}

int VDFFVideoSource::handle_frame_num(int64_t pts, int64_t dts)
{
  dead_range_start = -1;
  dead_range_end = -1;
  int64_t ts = pts;
  if(ts==AV_NOPTS_VALUE) ts = dts;
  int pos = next_frame;

  if(avi_drop_index && pos!=-1){
    while(pos<sample_count && frame_type[pos]=='D') pos++;
  } else if(!trust_index && !is_image_list){
    if(ts==AV_NOPTS_VALUE && pos==-1) return -1;

    if(ts!=AV_NOPTS_VALUE){
      // guess where we are
      // timestamp to frame number is at times unreliable
      ts -= start_time;
      int rndd = time_base.den/2;
      pos = int((ts*time_base.num + rndd) / time_base.den);
    }
  }

  if(pos>last_seek_frame) last_seek_frame = -1;
  return pos;
}

int VDFFVideoSource::handle_frame()
{
  decoded_count++;
  int pos = handle_frame_num(frame->pts,frame->pkt_dts);
  if(pos==-1) return -1;

  if(next_frame>0 && pos>next_frame){
    // gap between frames, fill with dups
    // caused by non-constant framerate etc
    BufferPage* page = frame_array[next_frame-1];
    if(page) copy_page(next_frame, pos-1, page);
  }

  next_frame = pos+1;

  // ignore anything outside promised range
  if(pos<0 || pos>=sample_count){
    return -1;
  }

  if(!frame_array[pos]){
    alloc_page(pos);
    frame_type[pos] = av_get_picture_type_char(frame->pict_type);
    BufferPage* page = frame_array[pos];
    open_write(page);
    page->error = 0;

    if(!page->p){
      page->error = BufferPage::err_memory;
    } else if(!check_frame_format()){
      page->error = BufferPage::err_badformat;
    } else {
      uint8_t* dst = align_buf(page->p);
      if(convertInfo.ext_format==nsVDXPixmap::kPixFormat_YUV422_V210)
        memcpy(dst,frame->data[0],frame->linesize[0]*frame->height);
      else
        av_image_copy_to_buffer(dst, frame_size, frame->data, frame->linesize, (AVPixelFormat)frame->format, frame->width, frame->height, line_align);
    }
  } else if(direct_buffer){
    frame_type[pos] = av_get_picture_type_char(frame->pict_type);
  }

  return pos;
}

bool VDFFVideoSource::check_frame_format()
{
  if(frame->format!=frame_fmt) return false;
  if(frame->width!=frame_width) return false;
  if(frame->height!=frame_height) return false;
  return true;
}

void VDFFVideoSource::alloc_direct_buffer()
{
  int pos = next_frame;
  if(!frame_array[pos]) alloc_page(pos);
  BufferPage* page = frame_array[pos];
  open_write(page);
  if(direct_cfhd) cfhd_set_buffer(m_pCodecCtx,align_buf(page->p));
}

void VDFFVideoSource::free_buffers()
{
  {for(int i=0; i<buffer_count; i++){
    BufferPage& page = buffer[i];
    frame_array[page.target] = 0;
    if(page.map_base){
      UnmapViewOfFile(page.map_base);
      page.map_base = 0;
      page.p = 0;
    }
    page.refs = 0;
    page.access = 0;
    page.target = 0;
  }}

  memset(frame_array,0,sample_count*sizeof(void*));

  dead_range_start = -1;
  dead_range_end = -1;
  first_frame = 0;
  last_frame = 0;
  used_frames = 0;
  next_frame = -1;
  last_seek_frame = -1;
}

VDFFVideoSource::BufferPage* VDFFVideoSource::remove_page(int pos, bool before, bool after)
{
  if(!used_frames) return 0;

  BufferPage* r = 0;
  while(1){
    if(last_frame>pos && after){
      if(frame_array[last_frame]){
        if(r) return r;
        BufferPage* p1 = frame_array[last_frame];
        frame_array[last_frame] = 0;
        p1->refs--;
        if(!p1->refs){
          r = p1;
          used_frames--;
        }
      }
      last_frame--;

    } else if(first_frame<pos && before){
      if(frame_array[first_frame]){
        if(r) return r;
        BufferPage* p1 = frame_array[first_frame];
        frame_array[first_frame] = 0;
        p1->refs--;
        if(!p1->refs){
          r = p1;
          used_frames--;
        }
      }
      first_frame++;
    } else return r;
  }
}

void VDFFVideoSource::alloc_page(int pos)
{
  BufferPage* r = 0;
  int buffer_max = buffer_count;
  if(small_cache_mode) buffer_max = small_buffer_count;

  if(used_frames>=buffer_max) r = remove_page(pos);
  if(!r){for(int i=0; i<buffer_count; i++){
    if(!buffer[i].refs){
      r = &buffer[i];
      r->i = i;
      if(!mem && !r->p){
        r->p = (uint8_t*)malloc(frame_size+line_align-1);
        if(!r->p) mContext.mpCallbacks->SetErrorOutOfMemory();
      }
      break;
    }
  }}

  r->target = pos;
  r->refs++;
  used_frames++;
  frame_array[pos] = r;
  if(pos>last_frame) last_frame = pos;
  if(pos<first_frame) first_frame = pos;
}

void VDFFVideoSource::copy_page(int start, int end, BufferPage* p)
{
  {for(int i=start; i<=end; i++){
    if(frame_array[i]) continue;
    p->refs++;
    frame_array[i] = p;
    if(i>last_frame) last_frame = i;
    if(i<first_frame) first_frame = i;

    if(frame_type[i]==' ') frame_type[i] = '+';
  }}
}

void VDFFVideoSource::dealloc_page(BufferPage* p)
{
  if(p->map_base)
    UnmapViewOfFile(p->map_base);
  else
    free(p->p);

  p->map_base = 0;
  p->p = 0;
  p->error = 0;
  p->access = 0;
}

void VDFFVideoSource::open_page(BufferPage* p, int flag)
{
  if(!mem) return;
  if(p->map_base && (p->access & flag)) return;

  {for(int i=0; i<buffer_count; i++){
    BufferPage& p1 = buffer[i];
    if(&p1==p) continue;
    if(!p1.map_base) continue;
    if(p1.access & flag){
      while(1){
        int access0 = p1.access & 3;
        if(InterlockedCompareExchange(&p1.access,4,access0)==access0){
          int access2 = access0 & ~flag;
          if(!access2){
            UnmapViewOfFile(p1.map_base);
            p1.map_base = 0;
            p1.p = 0;
          }
          InterlockedExchange(&p1.access,access2);
          break;
        }
      }
      break;
    }
  }}

  while(1){
    int access0 = p->access & 3;
    if(InterlockedCompareExchange(&p->access,4,access0)==access0){
      if(!p->map_base){
        uint64_t pos = uint64_t(frame_size)*p->i;
        uint64_t pos0 = pos & ~0xFFFF;
        uint64_t pos1 = (pos+frame_size+0xFFFF) & ~0xFFFF;

        p->map_base = MapViewOfFile(mem, FILE_MAP_WRITE, pos0>>32, (DWORD)pos0, (SIZE_T)(pos1-pos0));
        if(p->map_base){
          p->p = (uint8_t*)(ptrdiff_t(p->map_base) + pos-pos0);
        } else {
          mContext.mpCallbacks->SetErrorOutOfMemory();
        }
      }
      InterlockedExchange(&p->access,access0|flag);
      break;
    }
  }
}
