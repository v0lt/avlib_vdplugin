#pragma once

#include <vd2/plugin/vdinputdriver.h>
#include <vd2/VDXFrame/Unknown.h>


extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include <windows.h>

class VDFFInputFile;

class VDFFVideoSource : public vdxunknown<IVDXStreamSource>,
	public IVDXStreamSourceV5,
	public IVDXStreamSourceV3,
	public IVDXVideoSource,
	public IVDXVideoDecoder,
	public IVDXVideoDecoderModel,
	public IFilterModVideoDecoder
{
public:
	VDFFVideoSource(const VDXInputDriverContext& context);
	~VDFFVideoSource();

	int VDXAPIENTRY AddRef() override;
	int VDXAPIENTRY Release() override;
	void* VDXAPIENTRY AsInterface(uint32_t iid) override;

	//Stream Interface
	void VDXAPIENTRY GetStreamSourceInfo(VDXStreamSourceInfo&);
	void VDXAPIENTRY GetStreamSourceInfoV3(VDXStreamSourceInfoV3&);
	bool VDXAPIENTRY Read(int64_t lStart, uint32_t lCount, void* lpBuffer, uint32_t cbBuffer, uint32_t* lBytesRead, uint32_t* lSamplesRead);

	void VDXAPIENTRY ApplyStreamMode(uint32 flags);
	bool VDXAPIENTRY QueryStreamMode(uint32 flags);
	const void* VDXAPIENTRY GetDirectFormat();
	int         VDXAPIENTRY GetDirectFormatLen();

	ErrorMode VDXAPIENTRY GetDecodeErrorMode();
	void VDXAPIENTRY SetDecodeErrorMode(ErrorMode mode);
	bool VDXAPIENTRY IsDecodeErrorModeSupported(ErrorMode mode);

	bool VDXAPIENTRY IsVBR() { return false; }
	sint64 VDXAPIENTRY TimeToPositionVBR(int64_t us) { return 0; }
	sint64 VDXAPIENTRY PositionToTimeVBR(int64_t samples) { return 0; }

	void VDXAPIENTRY GetVideoSourceInfo(VDXVideoSourceInfo& info);

	bool VDXAPIENTRY CreateVideoDecoderModel(IVDXVideoDecoderModel** ppModel);
	bool VDXAPIENTRY CreateVideoDecoder(IVDXVideoDecoder** ppDecoder);

	void        VDXAPIENTRY GetSampleInfo(int64_t sample_num, VDXVideoFrameInfo& frameInfo);

	bool        VDXAPIENTRY IsKey(int64_t lSample);
	int64_t     VDXAPIENTRY GetFrameNumberForSample(int64_t sample_num);
	int64_t     VDXAPIENTRY GetSampleNumberForFrame(int64_t display_num);
	int64_t     VDXAPIENTRY GetRealFrame(int64_t display_num);

	int64_t     VDXAPIENTRY GetSampleBytePosition(int64_t sample_num);

	//Model Interface
	void    VDXAPIENTRY Reset();
	void    VDXAPIENTRY SetDesiredFrame(int64_t frame_num);
	int64_t VDXAPIENTRY GetNextRequiredSample(bool& is_preroll);
	int     VDXAPIENTRY GetRequiredCount();

	//Decoder Interface
	const void* VDXAPIENTRY DecodeFrame(const void* inputBuffer, uint32_t data_len, bool is_preroll, int64_t streamFrame, int64_t targetFrame);
	uint32_t    VDXAPIENTRY GetDecodePadding();
	bool        VDXAPIENTRY IsFrameBufferValid();
	const VDXPixmap& VDXAPIENTRY GetFrameBuffer();
	const FilterModPixmapInfo& VDXAPIENTRY GetFrameBufferInfo();
	bool        VDXAPIENTRY SetTargetFormat(int format, bool useDIBAlignment);
	bool        SetTargetFormat(nsVDXPixmap::VDXPixmapFormat format, bool useDIBAlignment, VDFFVideoSource* head);
	bool        VDXAPIENTRY SetDecompressedFormat(const VDXBITMAPINFOHEADER* pbih);

	const void* VDXAPIENTRY GetFrameBufferBase();
	bool        VDXAPIENTRY IsDecodable(int64_t sample_num);

private:
	//Internal
	const VDXInputDriverContext& mContext;
	VDFFInputFile* m_pSource;

	AVFormatContext* m_pFormatCtx = nullptr;
public:
	AVStream*        m_pStreamCtx = nullptr;
	AVCodecContext*  m_pCodecCtx  = nullptr;
	VDXStreamSourceInfoV3 m_streamInfo;
	int m_streamIndex;
	int sample_count;
	AVRational time_base;
	int64_t start_time;

private:
	void* direct_format   = nullptr;
	int direct_format_len = 0;

	AVFrame*    m_pFrame  = nullptr;
	SwsContext* m_pSwsCtx = nullptr;
	VDXPixmapAlpha m_pixmap;
	FilterModPixmapInfo m_pixmap_info;
	uint8_t* m_pixmap_data = nullptr;
	int m_pixmap_frame = 0;

public:
	struct ConvertInfo {
		nsVDXPixmap::VDXPixmapFormat req_format = nsVDXPixmap::kPixFormat_Null;
		nsVDXPixmap::VDXPixmapFormat ext_format = nsVDXPixmap::kPixFormat_Null;
		bool req_dib;

		AVPixelFormat av_fmt = AV_PIX_FMT_NONE;
		bool direct_copy;
		bool in_yuv;
		bool in_subs;
		bool out_rgb;
		bool out_garbage;
	} convertInfo;

	struct BufferPage {
		enum {
			err_badformat = 1,
			err_memory = 2
		};

		int i;
		int target;
		int refs;
		int error;
		volatile LONG access;
		void* map_base;
		uint8_t* p;
	};

	BufferPage* buffer = nullptr;
	int buffer_count   = 0;
	int buffer_reserve = 0;

private:
	ErrorMode errorMode = kErrorModeReportAll; // still not supported by host anyway

	HANDLE mem         = nullptr;

	BufferPage** frame_array = nullptr;
	char* frame_type         = nullptr;
	int64_t desired_frame;
	int required_count;
	int last_request    = -1;
	int next_frame      = -1;
	int first_frame;
	int last_frame;
	int last_seek_frame = -1;
	int used_frames;
	int fw_seek_threshold;

	AVPixelFormat frame_fmt;
	int frame_width;
	int frame_height;
public:
	int frame_size;

	int keyframe_gap;
	int decoded_count = 0;

	bool trust_index;
	bool sparse_index;
	bool has_vfr;
	bool average_fr;

private:
	bool flip_image         = false;
	bool avi_drop_index     = false;

	bool direct_buffer      = false;
	bool is_image_list      = false;
	bool m_copy_mode        = false;
	bool m_decode_mode      = true;
	bool m_small_cache_mode = false;
	bool enable_prefetch    = false;
	int small_buffer_count  = 0;
	int64_t dead_range_start;
	int64_t dead_range_end;

	AVPacket* copy_pkt = nullptr;

	//uint64 kPixFormat_XRGB64;

public:
	int  initStream(VDFFInputFile* pSource, int indexStream);
private:
	int  init_duration(const AVRational fr);
	void init_format();
	void set_pixmap_layout(uint8_t* p);
	int  handle_frame_num(int64_t pts, int64_t dts);
	int  handle_frame();
	bool check_frame_format();
	void set_start_time();
	bool read_frame(sint64 desired_frame, bool init = false);
	void alloc_direct_buffer();
	void alloc_page(int pos);
	BufferPage* remove_page(int play_pos, bool before = true, bool after = true);
	void dealloc_page(BufferPage* p);
	void free_buffers();
	void open_page(BufferPage* p, int flag);
	void open_read(BufferPage* p) { open_page(p, 1); }
	void open_write(BufferPage* p) { open_page(p, 2); }
	void copy_page(int start, int end, BufferPage* p);
	int64_t frame_to_pts_next(sint64 start);
	void setCopyMode(bool v);
	void setDecodeMode(bool v);
	void setCacheMode(bool v);
	bool is_intra();
	bool allow_copy();
	bool possible_delay();
	int  calc_sparse_key(int64_t sample, int64_t& pos);
	int  calc_seek(int jump, int64_t& pos);
	int  calc_prefetch(int jump);
};
