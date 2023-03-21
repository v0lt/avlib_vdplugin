#ifndef a_compress_header
#define a_compress_header

#include <vd2/plugin/vdinputdriver.h>
#include <vd2/VDXFrame/Unknown.h>
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include "libavutil/audio_fifo.h"
#include <libswresample/swresample.h>
}
#include <windows.h>
#include <mmreg.h>

struct WAVEFORMATEX_VDFF: public WAVEFORMATEXTENSIBLE {
  enum AVCodecID codec_id;
};

const GUID KSDATAFORMAT_SUBTYPE_VDFF = {0x54939996, 0xF549, 0x42d7, 0xA8, 0x73, 0xBD, 0xB0, 0x63, 0x85, 0xE5, 0x9F};

class VDFFAudio: public vdxunknown<IVDXAudioEnc>{
public:
  const VDXInputDriverContext &mContext;

  enum {flag_constant_rate=1};

  struct Config{
    int version;
    int quality;
    int bitrate;
    int flags;

    Config(){ clear(); }
    void clear(){ version=0; quality=0; bitrate=0; flags=0; }
  }* config;

  AVCodec* codec;
  AVCodecContext* ctx;
  AVFrame* frame;
  SwrContext* swr;
  uint8_t** sample_buf;
  uint8_t* in_buf;
  unsigned frame_size;
  int frame_pos;
  unsigned in_pos;
  int src_linesize;
  AVPacket pkt;
  sint64 total_in;
  sint64 total_out;
  int max_packet;

  WAVEFORMATEXTENSIBLE* out_format;
  int out_format_size;
  bool wav_compatible;

  VDFFAudio(const VDXInputDriverContext &pContext);
  ~VDFFAudio();
  void cleanup();

  void export_wav();

  virtual void CreateCodec() = 0;
  virtual void InitContext();
  virtual void reset_config(){ config->clear(); }

  virtual bool HasAbout(){ return false; }
  virtual bool HasConfig(){ return false; }
  virtual void ShowAbout(VDXHWND parent){}
  virtual void ShowConfig(VDXHWND parent){}
  virtual size_t GetConfigSize(){ return sizeof(Config); }
  virtual void* GetConfig(){ return config; }
  virtual void SetConfig(void* data, size_t size);

  virtual void SetInputFormat(VDXWAVEFORMATEX* format);
  virtual void Shutdown(){}
  void select_fmt(AVSampleFormat* list);

  virtual bool IsEnded() const { return false; }

  virtual unsigned	GetInputLevel() const;
  virtual unsigned	GetInputSpace() const;
  virtual unsigned	GetOutputLevel() const;
  virtual const VDXWAVEFORMATEX *GetOutputFormat() const { return (VDXWAVEFORMATEX*)out_format; }
  virtual unsigned	GetOutputFormatSize() const { return out_format_size; }
  virtual void	GetStreamInfo(VDXStreamInfo& si) const;

  virtual void		Restart(){}
  virtual bool		Convert(bool flush, bool requireOutput);

  virtual void		*LockInputBuffer(unsigned& bytes);
  virtual void		UnlockInputBuffer(unsigned bytes);
  virtual const void	*LockOutputBuffer(unsigned& bytes);
  virtual void		UnlockOutputBuffer(unsigned bytes);
  virtual unsigned	CopyOutput(void *dst, unsigned bytes, sint64& duration);
  virtual int SuggestFileFormat(const char* name);
};

class VDFFAudio_aac: public VDFFAudio{
public:
  struct Config:public VDFFAudio::Config{
  } codec_config;

  VDFFAudio_aac(const VDXInputDriverContext &pContext):VDFFAudio(pContext){
    config = &codec_config;
    reset_config();
  }
  virtual const char* GetElementaryFormat(){ return "aac"; }
  virtual void CreateCodec();
  virtual void InitContext();
  virtual size_t GetConfigSize(){ return sizeof(Config); }
  virtual void reset_config();
  virtual bool HasConfig(){ return true; }
  virtual void ShowConfig(VDXHWND parent);
};

class VDFFAudio_mp3: public VDFFAudio{
public:
  enum {flag_jointstereo=2};
  struct Config:public VDFFAudio::Config{
  } codec_config;

  VDFFAudio_mp3(const VDXInputDriverContext &pContext):VDFFAudio(pContext){
    config = &codec_config;
    reset_config();
  }
  virtual const char* GetElementaryFormat(){ return "mp3"; }
  virtual void CreateCodec();
  virtual void InitContext();
  virtual size_t GetConfigSize(){ return sizeof(Config); }
  virtual void reset_config();
  virtual bool HasConfig(){ return true; }
  virtual void ShowConfig(VDXHWND parent);
};

class VDFFAudio_flac: public VDFFAudio{
public:
  enum {flag_jointstereo=2};
  struct Config:public VDFFAudio::Config{
  } codec_config;

  VDFFAudio_flac(const VDXInputDriverContext &pContext):VDFFAudio(pContext){
    config = &codec_config;
    reset_config();
  }
  virtual const char* GetElementaryFormat(){ return "flac"; }
  virtual void CreateCodec();
  virtual void InitContext();
  virtual size_t GetConfigSize(){ return sizeof(Config); }
  virtual void reset_config();
  virtual bool HasConfig(){ return true; }
  virtual void ShowConfig(VDXHWND parent);
};

class VDFFAudio_alac: public VDFFAudio{
public:
  struct Config:public VDFFAudio::Config{
  } codec_config;

  VDFFAudio_alac(const VDXInputDriverContext &pContext):VDFFAudio(pContext){
    config = &codec_config;
    reset_config();
  }
  virtual const char* GetElementaryFormat(){ return "caf"; }
  virtual void CreateCodec();
  virtual void InitContext();
  virtual size_t GetConfigSize(){ return sizeof(Config); }
  virtual void reset_config();
  virtual bool HasConfig(){ return true; }
  virtual void ShowConfig(VDXHWND parent);
};

class VDFFAudio_vorbis: public VDFFAudio{
public:
  struct Config:public VDFFAudio::Config{
  } codec_config;

  VDFFAudio_vorbis(const VDXInputDriverContext &pContext):VDFFAudio(pContext){
    config = &codec_config;
    reset_config();
  }
  virtual const char* GetElementaryFormat(){ return "ogg"; }
  int SuggestFileFormat(const char* name);
  virtual void CreateCodec();
  virtual void InitContext();
  virtual size_t GetConfigSize(){ return sizeof(Config); }
  virtual void reset_config();
  virtual bool HasConfig(){ return true; }
  virtual void ShowConfig(VDXHWND parent);
};

class VDFFAudio_opus: public VDFFAudio{
public:
  enum {flag_limited_rate=4};
  struct Config:public VDFFAudio::Config{
  } codec_config;

  VDFFAudio_opus(const VDXInputDriverContext &pContext):VDFFAudio(pContext){
    config = &codec_config;
    reset_config();
  }
  virtual const char* GetElementaryFormat(){ return "ogg"; }
  virtual void CreateCodec();
  virtual void InitContext();
  virtual size_t GetConfigSize(){ return sizeof(Config); }
  virtual void reset_config();
  virtual bool HasConfig(){ return true; }
  virtual void ShowConfig(VDXHWND parent);
};

extern VDXPluginInfo ff_aacenc_info;
extern VDXPluginInfo ff_mp3enc_info;
extern VDXPluginInfo ff_flacenc_info;
extern VDXPluginInfo ff_alacenc_info;
extern VDXPluginInfo ff_vorbisenc_info;
extern VDXPluginInfo ff_opusenc_info;

#endif
