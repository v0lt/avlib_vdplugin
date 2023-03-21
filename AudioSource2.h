#pragma once

#include <vd2/plugin/vdinputdriver.h>
#include <vd2/VDXFrame/Unknown.h>
#include <windows.h>
#include <mmreg.h>
#include "stdint.h"

extern "C"
{
#include <libavutil/frame.h>
#include <libavutil/opt.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

class VDFFInputFile;

class VDFFAudioSource : public vdxunknown<IVDXStreamSource>, public IVDXAudioSource {
public:
  VDFFAudioSource(const VDXInputDriverContext& context);
  ~VDFFAudioSource();

  int VDXAPIENTRY AddRef();
  int VDXAPIENTRY Release();
  void *VDXAPIENTRY AsInterface(uint32_t iid);

  void		VDXAPIENTRY GetStreamSourceInfo(VDXStreamSourceInfo& srcInfo) { srcInfo = m_streamInfo; }
  bool		VDXAPIENTRY Read(int64_t lStart, uint32_t lCount, void *lpBuffer, uint32_t cbBuffer, uint32_t *lBytesRead, uint32_t *lSamplesRead);

  const void *VDXAPIENTRY GetDirectFormat() { return &mRawFormat; }
  int			VDXAPIENTRY GetDirectFormatLen() { return sizeof(WAVEFORMATEXTENSIBLE); }
  void VDXAPIENTRY SetTargetFormat(const VDXWAVEFORMATEX* format);

  ErrorMode VDXAPIENTRY GetDecodeErrorMode() { return  IVDXStreamSource::kErrorModeReportAll; }
  void VDXAPIENTRY SetDecodeErrorMode(ErrorMode mode) {}
  bool VDXAPIENTRY IsDecodeErrorModeSupported(ErrorMode mode) { return mode == IVDXStreamSource::kErrorModeReportAll; }

  bool VDXAPIENTRY IsVBR(){ return false; }
  int64_t VDXAPIENTRY TimeToPositionVBR(int64_t us){ return 0; }
  int64_t VDXAPIENTRY PositionToTimeVBR(int64_t samples){ return 0; }

  void VDXAPIENTRY GetAudioSourceInfo(VDXAudioSourceInfo& info) { info.mFlags = 0; }

public:
  const VDXInputDriverContext& mContext;
  WAVEFORMATEXTENSIBLE mRawFormat;
  VDXStreamSourceInfo	m_streamInfo;

  VDFFInputFile* m_pSource;
  int m_streamIndex;
  int64_t sample_count;
  AVRational time_base;
  int64_t start_time;
  int64_t time_adjust;

  AVFormatContext* m_pFormatCtx;
  AVStream* m_pStreamCtx;
  AVCodecContext* m_pCodecCtx;
  SwrContext* swr;
  AVFrame* frame;
  int src_linesize;
  uint64_t out_layout;
  AVSampleFormat out_fmt;
  uint64_t swr_layout;
  int swr_rate;
  AVSampleFormat swr_fmt;

  struct BufferPage{
    enum {size=0x8000}; // max usable value 0xFFFF

    uint16_t a0,a1; // range a
    uint16_t b0,b1; // range b
    uint8_t* p;

    void reset(){ a0=0; a1=0; b0=0; b1=0; p=0; }
    int copy(int s0, uint32_t count, void* dst, int sample_size);
    int alloc(int s0, uint32_t count, int& changed);
    int empty(int s0, uint32_t count);
  };

  BufferPage* buffer;
  int buffer_size; // in pages
  int used_pages;
  int used_pages_max;
  int first_page;
  int last_page;

  int64_t next_sample;
  int64_t first_sample;
  int discard_samples;
  bool trust_sample_pos;
  bool use_keys;

  struct ReadInfo{
    int64_t first_sample;
    int64_t last_sample;

    ReadInfo(){ first_sample=-1; last_sample=-1; }
  };

  int initStream( VDFFInputFile* pSource, int streamIndex );
  void init_start_time();
  int read_packet(AVPacket& pkt, ReadInfo& ri);
  void insert_silence(int64_t start, uint32_t count);
  void write_silence(void* dst, uint32_t count);
  void invalidate(int64_t start, uint32_t count);
  void alloc_page(int i);
  void reset_cache();
  void reset_swr();
  int64_t frame_to_pts(sint64 start, AVStream* video);
};
