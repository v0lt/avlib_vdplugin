#include <tchar.h>
#include "FileInfo2.h"
#include "InputFile2.h"
#include "VideoSource2.h"
#include "AudioSource2.h"
#include "resource.h"
#include "cineform.h"
#include "gopro.h"

#include <string>

static const char* vsnstr = "Version 2.1";

extern HINSTANCE hInstance;
 
bool VDFFInputFileInfoDialog::Show(VDXHWND parent, VDFFInputFile* pInput)
{
  source = pInput;
  segment_pos = 0;
  segment_count = 1;
  VDFFInputFile* f = pInput;
  while(1){
    f = f->next_segment;
    if(!f) break;
    segment_count++;
  }

  /*
  pFormatCtx = pInput->getContext();
  int vs = pInput->find_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO);
  if(vs!=-1) pVideoStream = pFormatCtx->streams[vs];
  int as = pInput->find_stream(pFormatCtx, AVMEDIA_TYPE_AUDIO);
  if(as!=-1) pAudioStream = pFormatCtx->streams[as];
  */

  LRESULT ret = VDXVideoFilterDialog::Show(hInstance, MAKEINTRESOURCEW(IDD_FF_INFO), (HWND)parent);
  return ret!=0;
}

void VDFFInputFileInfoDialog::load_segment()
{
  int pos = 0;
  int start_frame = 0;
  VDFFInputFile* f = source;
  while(1){
    if(pos==segment_pos) break;
    if(f->video_source) start_frame += f->video_source->sample_count;
    f = f->next_segment;
    pos++;
  }

  segment = f;

  wchar_t* p0 = wcsrchr(segment->path,'\\');
  wchar_t* p1 = wcsrchr(segment->path,'/');
  wchar_t* name = segment->path;
  if(p0>name) name = p0+1;
  if(p1>name) name = p1+1;

  const int buf_size = 80+MAX_PATH;
  wchar_t buf[buf_size];
  swprintf(buf,buf_size,L"Segment %d of %d: %ls",segment_pos+1,segment_count,name);
  SetDlgItemTextW(mhdlg, IDC_SEGMENT, buf);

  int end_frame = start_frame;
  if(segment->video_source) end_frame += segment->video_source->sample_count-1;
  swprintf(buf,buf_size,L"Timeline: frame %d to %d",start_frame,end_frame);
  SetDlgItemTextW(mhdlg, IDC_SEGMENT_TIMELINE, buf);

  HWND prev = GetDlgItem(mhdlg,IDC_SEGMENT_PREV);
  HWND next = GetDlgItem(mhdlg,IDC_SEGMENT_NEXT);
  bool bprev = segment_pos>0;
  bool bnext = segment_pos<segment_count-1;
  EnableWindow(prev,true);
  EnableWindow(next,true);
  if(GetFocus()==prev && !bprev) SetFocus(next);
  if(GetFocus()==next && !bnext) SetFocus(prev);
  EnableWindow(prev,bprev);
  EnableWindow(next,bnext);
  SendMessage(prev, BM_SETSTYLE, (WPARAM)BS_FLAT, TRUE);
  SendMessage(next, BM_SETSTYLE, (WPARAM)BS_FLAT, TRUE);

  print_format();
  print_metadata();

  if(segment->video_source) print_video();
  if(segment->audio_source) print_audio();
}

INT_PTR VDFFInputFileInfoDialog::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch( msg ){
  case WM_INITDIALOG:
    load_segment();
    print_performance();
    return TRUE;

  case WM_COMMAND:
    switch(LOWORD(wParam)){
    case IDOK:
      EndDialog(mhdlg, TRUE);
      return TRUE;

    case IDCANCEL:
      EndDialog(mhdlg, FALSE);
      return TRUE;

    case IDC_SEGMENT_PREV:
      if(segment_pos>0){
        segment_pos--;
        load_segment();
      }
      return TRUE;

    case IDC_SEGMENT_NEXT:
      if(segment_pos<segment_count-1){
        segment_pos++;
        load_segment();
      }
      return TRUE;

    }
  }

  return FALSE;
}

void VDFFInputFileInfoDialog::print_format()
{
  AVFormatContext* pFormatCtx = segment->getContext();
  AVInputFormat* pInputFormat = pFormatCtx->iformat;

  SetDlgItemTextA(mhdlg, IDC_STATICVerNumber, vsnstr);
  char buf[128];

  SetDlgItemTextA(mhdlg, IDC_FORMATNAME, pInputFormat->long_name);

  if(segment->is_image){
    int n = segment->video_source->sample_count;
    if(n>1){
      sprintf(buf, "%d images", n);
      SetDlgItemText(mhdlg, IDC_DURATION, buf);
    } else {
      SetDlgItemText(mhdlg, IDC_DURATION, "single image");
    }
  } else {
    double seconds = 0;
    if(segment->is_anim_image){
      VDXFraction fr = segment->video_source->m_streamInfo.mInfo.mSampleRate;
      seconds = double(segment->video_source->sample_count)*fr.mDenominator/fr.mNumerator;
    } else {
      if(pFormatCtx->duration==AV_NOPTS_VALUE)
        seconds = -1;
      else
        seconds = pFormatCtx->duration/(double)AV_TIME_BASE;
    }

    if(seconds==-1){
      SetDlgItemText(mhdlg, IDC_DURATION, "unknown");
    } else {
      int hours = (int)(seconds/3600);
      seconds -= (hours * 3600);
      int minutes = (int)(seconds/60);
      seconds -= (minutes * 60);

      sprintf(buf, "%d h : %d min : %.2f sec", hours, minutes, seconds);
      SetDlgItemText(mhdlg, IDC_DURATION, buf);
    }
  }

  sprintf(buf, "%I64d kb/sec", pFormatCtx->bit_rate/1000);
  SetDlgItemText(mhdlg, IDC_BITRATE, buf);

  sprintf(buf, "%u", pFormatCtx->nb_streams);
  SetDlgItemText(mhdlg, IDC_STREAMSCOUNT, buf);
}

void VDFFInputFileInfoDialog::print_video()
{
  AVStream* pVideoStream = segment->video_source->m_pStreamCtx;
  AVCodecContext* pVideoCtx = segment->video_source->m_pCodecCtx;
  AVCodecParameters* codecpar = pVideoStream->codecpar;
  if(!pVideoCtx) return;

  char buf[256];

  AVCodec* pCodec = avcodec_find_decoder(codecpar->codec_id);
  const char* codec_name = "N/A";
  if (pCodec) codec_name = pCodec->name; else {
    if (codecpar->codec_id == AV_CODEC_ID_MPEG2TS) codec_name = "mpeg2ts";
  }

  SetDlgItemTextA(mhdlg, IDC_VIDEO_CODECNAME, codec_name); 

  if ( pVideoCtx->pix_fmt != AV_PIX_FMT_NONE )
  {
    strncpy(buf, av_get_pix_fmt_name(pVideoCtx->pix_fmt), 128);
    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(pVideoCtx->pix_fmt);
    bool is_rgb = (desc->flags & AV_PIX_FMT_FLAG_RGB)!=0;

    if(segment->video_source->direct_cfhd){
      std::string name = cfhd_format_name(pVideoCtx);
      strcpy(buf, name.c_str());
      is_rgb = false;
    }

    if(!is_rgb){
      const char* spc = "?";
      const char* r = 0;
      if ( pVideoCtx->colorspace == AVCOL_SPC_UNSPECIFIED ) spc = 0;
      if ( pVideoCtx->colorspace == AVCOL_SPC_BT709 ) spc = "709";
      if ( pVideoCtx->colorspace == AVCOL_SPC_BT470BG) spc = "601";
      if ( pVideoCtx->colorspace == AVCOL_SPC_SMPTE170M) spc = "601";
      if ( pVideoCtx->colorspace == AVCOL_SPC_SMPTE240M) spc = "601";
      if ( pVideoCtx->colorspace == AVCOL_SPC_FCC) spc = "FCC";
      if ( pVideoCtx->colorspace == AVCOL_SPC_YCOCG) spc = "SG16";
      if ( pVideoCtx->colorspace == AVCOL_SPC_BT2020_NCL) spc = "2020";
      if ( pVideoCtx->colorspace == AVCOL_SPC_BT2020_CL) spc = "2020";
      if ( pVideoCtx->color_range == AVCOL_RANGE_JPEG ) r = "FR";
      if(spc){
        strcat(buf, " (");
        strcat(buf, spc);
        if(r){
          strcat(buf, ":");
          strcat(buf, r);
        }
        strcat(buf, ")");
      }
    }

    SetDlgItemTextA(mhdlg, IDC_VIDEO_PIXFMT, buf);

  } else {
    SetDlgItemTextA(mhdlg, IDC_VIDEO_PIXFMT, "N/A");
  }

  if(segment->is_image){
    sprintf(buf, "%u x %u", pVideoCtx->width, pVideoCtx->height);
    SetDlgItemText(mhdlg, IDC_VIDEO_WXH, buf);
  } else {
    VDXFraction fr = segment->video_source->m_streamInfo.mInfo.mSampleRate;
    sprintf(buf, "%u x %u, %.2f fps", pVideoCtx->width, pVideoCtx->height, fr.mNumerator/double(fr.mDenominator));
    if(segment->video_source->average_fr) strcat(buf," (average)");
    SetDlgItemText(mhdlg, IDC_VIDEO_WXH, buf);
  }

  AVRational ar = av_make_q(1,1);
  if ( pVideoStream->sample_aspect_ratio.num ){
    ar = pVideoStream->sample_aspect_ratio;
  } else if ( pVideoCtx->sample_aspect_ratio.num ){
    ar = pVideoCtx->sample_aspect_ratio;
  }
  AVRational ar1;
  av_reduce(&ar1.num, &ar1.den, ar.num, ar.den, INT_MAX);
  sprintf(buf, "%u : %u", ar1.num, ar1.den);
  SetDlgItemText(mhdlg, IDC_VIDEO_ASPECTRATIO, buf);

  if ( pVideoCtx->bit_rate ){
    sprintf(buf, "%I64d kb/sec", pVideoCtx->bit_rate/1000);
    SetDlgItemText(mhdlg, IDC_VIDEO_BITRATE, buf);
  } else {
    SetDlgItemText(mhdlg, IDC_VIDEO_BITRATE, "N/A");
  }
}

void VDFFInputFileInfoDialog::print_audio()
{
  AVStream* pAudioStream = segment->audio_source->m_pStreamCtx;
  AVCodecContext* pAudioCtx = segment->audio_source->m_pCodecCtx;
  if(!pAudioCtx) return;

  char buf[128];
  char buf2[128];

  AVCodec* pCodec = avcodec_find_decoder(pAudioCtx->codec_id);
  const char *codec_name = "N/A";
  if (pCodec) codec_name = pCodec->name;

  SetDlgItemTextA(mhdlg, IDC_AUDIO_CODECNAME, codec_name);  
  
  sprintf(buf, "%u Hz", pAudioCtx->sample_rate);
  SetDlgItemText(mhdlg, IDC_AUDIO_SAMPLERATE, buf);

  uint64_t in_layout = pAudioCtx->channel_layout;
  if(!in_layout) in_layout = av_get_default_channel_layout(pAudioCtx->channels);
  av_get_channel_layout_string(buf2, sizeof buf2, pAudioCtx->channels, in_layout);
  int nch = av_get_channel_layout_nb_channels(in_layout);
  sprintf(buf, "%s (%u), ", buf2, nch);
  if (pAudioCtx->sample_fmt != AV_SAMPLE_FMT_NONE){
    strcat(buf, av_get_sample_fmt_name(pAudioCtx->sample_fmt));
  } else {
    strcat(buf, "N/A");
  }
  SetDlgItemText(mhdlg, IDC_AUDIO_CHANNELS, buf);

  int bits_per_sample = av_get_bits_per_sample(pAudioCtx->codec_id);
  int64_t bit_rate = bits_per_sample ? pAudioCtx->sample_rate * pAudioCtx->channels * bits_per_sample : pAudioCtx->bit_rate;
  if ( bit_rate ){
    sprintf(buf, "%I64d kb/sec", bit_rate/1000);
    SetDlgItemText(mhdlg, IDC_AUDIO_BITRATE, buf);
  } else {
    SetDlgItemText(mhdlg, IDC_AUDIO_BITRATE, "N/A");
  }
}

struct MetaInfo{
  bool creation_time;
};

bool skip_useless_meta(MetaInfo& info, AVDictionaryEntry* t){
  if(strcmp(t->key,"language")==0) return true;
  if(strcmp(t->key,"handler_name")==0) return true;
  if(strcmp(t->key,"creation_time")==0){
    if(info.creation_time) return true;
    info.creation_time = true;
  }
  return false;
}

void VDFFInputFileInfoDialog::print_metadata()
{
  AVFormatContext* pFormatCtx = segment->m_pFormatCtx;
  AVStream* pVideoStream = 0;
  AVStream* pAudioStream = 0;
  if(segment->video_source) pVideoStream = segment->video_source->m_pStreamCtx;
  if(segment->audio_source) pAudioStream = segment->audio_source->m_pStreamCtx;

  const int buf_size = 1024;
  char buf[buf_size];

  std::string s;

  MetaInfo info = {0};

  if(pFormatCtx && pFormatCtx->metadata){
    AVDictionaryEntry* t=0;
    bool header = true;
    while(t=av_dict_get(pFormatCtx->metadata, "", t, AV_DICT_IGNORE_SUFFIX)){
      if(skip_useless_meta(info,t)) continue;
      if(header){
        s += "File:";
        header = false;
      }
      _stprintf_s(buf, buf_size, "\t%s = %s\r\n", t->key, t->value);
      s += buf;
    }
  }
  if(pVideoStream && pVideoStream->metadata){
    AVDictionaryEntry* t=0;
    bool header = true;
    while(t=av_dict_get(pVideoStream->metadata, "", t, AV_DICT_IGNORE_SUFFIX)){
      if(skip_useless_meta(info,t)) continue;
      if(header){
        s += "Video:";
        header = false;
      }
      _stprintf_s(buf, buf_size, "\t%s = %s\r\n", t->key, t->value);
      s += buf;
    }
  }
  if(pAudioStream && pAudioStream->metadata){
    AVDictionaryEntry* t=0;
    bool header = true;
    while(t=av_dict_get(pAudioStream->metadata, "", t, AV_DICT_IGNORE_SUFFIX)){
      if(skip_useless_meta(info,t)) continue;
      if(header){
        s += "Audio:";
        header = false;
      }
      _stprintf_s(buf, buf_size, "\t%s = %s\r\n", t->key, t->value);
      s += buf;
    }
  }

  GoproInfo gi;
  gi.find_info(segment->path);

  if(gi.type){
    _stprintf_s(buf, buf_size, "GoPro info:\t%s\r\n", gi.type->Name);
    s += buf;
    _stprintf_s(buf, buf_size, "\tfirmware = %s\r\n", gi.firmware);
    s += buf;
    _stprintf_s(buf, buf_size, "\tserial# = %s\r\n", gi.cam_serial);
    s += buf;
    s += gi.setup_info;
  }

  int tab[2] = {42,90};
  SendDlgItemMessage(mhdlg, IDC_METADATA, EM_SETTABSTOPS, 2, (LPARAM)tab);
  SetDlgItemText(mhdlg, IDC_METADATA, s.c_str());
}

void VDFFInputFileInfoDialog::print_performance()
{
  VDFFInputFile* f1 = source;

  int buf_count = 0;
  int buf_max = 0;
  int index_quality = 2;
  int decoded_count = 0;
  bool all_key = true;
  bool has_vfr = false;

  while(f1 && f1->video_source){
    VDFFVideoSource* v1 = f1->video_source;
    buf_max += v1->buffer_reserve;
    {for(int i=0; i<v1->buffer_count; i++)
      if(v1->buffer[i].refs) buf_count++; }

    if(!v1->trust_index){
      if(index_quality>1) index_quality = 1;
      if(!v1->sparse_index){
        if(index_quality>0) index_quality = 0;
      }
    }
    if(v1->keyframe_gap!=1) all_key = false;
    if(v1->has_vfr) has_vfr = true;
    decoded_count += v1->decoded_count;

    f1 = f1->next_segment;
  }

  if(buf_max==0){
    SetDlgItemText(mhdlg, IDC_MEMORY_INFO, 0);
    SetDlgItemText(mhdlg, IDC_STATS, 0);
    SetDlgItemText(mhdlg, IDC_INDEX_INFO, 0);
    return;
  }

  int64_t mem_max = 0;
  if(source->video_source) mem_max = source->video_source->frame_size; 
  mem_max *= buf_max;
  mem_max = (mem_max+512*1024)/(1024*1024);

  int buf_used = (buf_count*200+buf_max)/(buf_max*2);

  char buf[1024];
  sprintf(buf,"Memory cache: %d frames / %dM reserved, %d%% used", buf_max, int(mem_max), buf_used);
  SetDlgItemText(mhdlg, IDC_MEMORY_INFO, buf);

  sprintf(buf,"Frames decoded: %d", decoded_count);
  SetDlgItemText(mhdlg, IDC_STATS, buf);

  if(segment->is_image){
    SetDlgItemText(mhdlg, IDC_INDEX_INFO, "Seeking: image list (random access)");
  } else {
    std::string msg;
    if(index_quality>0){
      if(all_key){
        msg = "Seeking: index present, optimal random access";
      } else if(index_quality==2){
        msg = "Seeking: index present, all frames";
      } else {
        msg = "Seeking: index present";
      }
      if(has_vfr) msg += " (vfr)";
    } else {
      msg = "Seeking: index missing, reverse scan may be slow";
    }

    SetDlgItemText(mhdlg, IDC_INDEX_INFO, msg.c_str());
  }
}
