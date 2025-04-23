/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef export_header
#define export_header

#include <vd2/plugin/vdinputdriver.h>
#include <vd2/VDXFrame/Unknown.h>
#include <string>
#include <vector>

uint32 export_avi_fcc(AVStream* src);

struct IOBuffer {
	uint8_t* data = nullptr;
	int64_t  size = 0;
	int64_t  pos  = 0;

	IOBuffer(const void* data, const int size) {
		this->data = (uint8_t*)av_malloc(size + AVPROBE_PADDING_SIZE);
		this->size = size;
		memcpy(this->data, data, size);
	}

	~IOBuffer() {
		av_free(data);
	}

	static int Read(void* obj, uint8_t* buf, int buf_size) {
		IOBuffer* t = (IOBuffer*)obj;
		const int64_t n = (t->pos + buf_size < t->size) ? buf_size : t->size - t->pos;
		if (n > 0) {
			memcpy(buf, t->data + t->pos, (size_t)n);
			t->pos += n;
			return int(n);
		}
		return AVERROR_EOF;
	}

	static int64_t Seek(void* obj, int64_t offset, int whence) {
		IOBuffer* t = (IOBuffer*)obj;
		if (whence == AVSEEK_SIZE) {
			return t->size;
		}
		if (whence == SEEK_CUR) {
			t->pos += offset;
			return t->pos;
		}
		if (whence == SEEK_SET) {
			t->pos = offset;
			return t->pos;
		}
		if (whence == SEEK_END) {
			t->pos = t->size + offset;
			return t->pos;
		}
		return -1;
	}
};

struct IOWBuffer {
	uint8_t* data = nullptr;
	int64_t  size = 0;
	int64_t  pos  = 0;

	~IOWBuffer() {
		free(data);
	}

	static int Write(void* obj, const uint8_t* buf, int buf_size) {
		IOWBuffer* t = (IOWBuffer*)obj;
		int64_t pos = t->pos;
		if (t->size < pos + buf_size) {
			t->data = (uint8_t*)realloc(t->data, size_t(pos + buf_size));
			t->size = pos + buf_size;
		}
		memcpy(t->data + pos, buf, buf_size);
		t->pos += buf_size;
		return buf_size;
	}

	static int64_t Seek(void* obj, int64_t offset, int whence) {
		IOWBuffer* t = (IOWBuffer*)obj;
		if (whence == AVSEEK_SIZE) {
			return t->size;
		}
		if (whence == SEEK_CUR) {
			t->pos += offset;
			return t->pos;
		}
		if (whence == SEEK_SET) {
			t->pos = offset;
			return t->pos;
		}
		if (whence == SEEK_END) {
			t->pos = t->size + offset;
			return t->pos;
		}
		return -1;
	}
};

class FFOutputFile final : public  vdxunknown<IVDXOutputFile>
{
public:
	const VDXInputDriverContext& mContext;

	struct StreamInfo {
		AVStream* st = nullptr;
		int64_t frame = 0;
		int64_t offset_num = 0;
		int64_t offset_den = 1;
		AVRational time_base = { 0, 0 };
		bool bswap_pcm = false;
	};

	std::string m_out_ff_path;
	std::string format_name;
	std::vector<StreamInfo> stream;
	AVFormatContext* m_ofmt = nullptr;
	bool header = false;
	bool stream_test = false;
	bool mp4_faststart = false;

	void* a_buf = nullptr;
	uint32 a_buf_size = 0;

	FFOutputFile(const VDXInputDriverContext& pContext);
	~FFOutputFile();

	// IVDXOutputFile
	void VDXAPIENTRY Init(const wchar_t* path, const char* format) override;
	uint32 VDXAPIENTRY CreateStream(int type) override;
	void VDXAPIENTRY SetVideo(uint32 index, const VDXStreamInfo& si, const void* pFormat, int cbFormat) override;
	void VDXAPIENTRY SetAudio(uint32 index, const VDXStreamInfo& si, const void* pFormat, int cbFormat) override;
	void VDXAPIENTRY Write(uint32 index, const void* pBuffer, uint32 cbBuffer, PacketInfo& info) override;
	void Finalize() override;
	const char* GetFormatName() override { return format_name.c_str(); }
	bool Begin() override;

	void av_error(int err);
	void adjust_codec_tag(AVStream* st);
	void import_bmp(AVStream* st, const void* pFormat, int cbFormat);
	void import_wav(AVStream* st, const void* pFormat, int cbFormat);
	bool test_streams();
	void* bswap_pcm(uint32 index, const void* pBuffer, uint32 cbBuffer);
};

class VDFFOutputFileDriver : public vdxunknown<IVDXOutputFileDriver>
{
public:
	const VDXInputDriverContext& mpContext;

	VDFFOutputFileDriver(const VDXInputDriverContext& pContext)
		:mpContext(pContext)
	{
	}

	virtual bool VDXAPIENTRY GetStreamControl(const wchar_t* path, const char* format, VDXStreamControl& sc) override;

	virtual bool VDXAPIENTRY CreateOutputFile(IVDXOutputFile** ppFile) override {
		*ppFile = new FFOutputFile(mpContext);
		return true;
	}

	virtual bool   VDXAPIENTRY EnumFormats(int i, wchar_t* filter, wchar_t* ext, char* name) override;
	virtual uint32 VDXAPIENTRY GetFormatCaps(int i) override;
};

extern VDXPluginInfo ff_output_info;

#endif
