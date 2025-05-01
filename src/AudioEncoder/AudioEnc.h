/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <windows.h>
#include <mmreg.h>

#include <vd2/plugin/vdinputdriver.h>
#include <vd2/VDXFrame/VideoFilterDialog.h>
#include <vd2/VDXFrame/Unknown.h>

#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libswresample/swresample.h>
}

struct WAVEFORMATEX_VDFF : public WAVEFORMATEXTENSIBLE {
	enum AVCodecID codec_id;
};

const GUID KSDATAFORMAT_SUBTYPE_VDFF = { 0x54939996, 0xF549, 0x42d7, 0xA8, 0x73, 0xBD, 0xB0, 0x63, 0x85, 0xE5, 0x9F };

class VDFFAudio : public vdxunknown<IVDXAudioEnc>
{
public:
	const VDXInputDriverContext& mContext;

	enum { flag_constant_rate = 1 };

	struct Config {
		int version;
	}*config = nullptr;

	const AVCodec* codec  = nullptr;
	AVCodecContext* avctx = nullptr;
	AVFrame* frame        = nullptr;
	SwrContext* swr       = nullptr;
	uint8_t** sample_buf  = nullptr;
	uint8_t* in_buf       = nullptr;
	unsigned frame_size   = 0;
	int frame_pos         = 0;
	unsigned in_pos       = 0;
	int src_linesize      = 0;
	AVPacket* pkt         = nullptr;
	int64_t total_in      = 0;
	int64_t total_out     = 0;
	int max_packet        = 0;

	WAVEFORMATEXTENSIBLE* out_format = nullptr;
	int out_format_size = 0;
	bool wav_compatible = false;

	VDFFAudio(const VDXInputDriverContext& pContext);
	~VDFFAudio();
	void cleanup();

	void export_wav();

	virtual void CreateCodec() = 0;
	virtual void InitContext() = 0;
	virtual void reset_config() = 0;
	virtual void load_config() = 0;
	virtual void save_config() = 0;

	virtual bool HasAbout() { return false; }
	virtual bool HasConfig() { return false; }
	virtual void ShowAbout(VDXHWND parent) {}
	virtual void ShowConfig(VDXHWND parent) {}
	virtual size_t GetConfigSize() { return sizeof(Config); }
	virtual void* GetConfig() { return config; }
	virtual void SetConfig(void* data, size_t size);

	virtual void SetInputFormat(VDXWAVEFORMATEX* format);
	virtual void Shutdown() {}
	void select_fmt(AVSampleFormat* list);

	virtual bool IsEnded() const { return false; }

	virtual unsigned GetInputLevel() const;
	virtual unsigned GetInputSpace() const;
	virtual unsigned GetOutputLevel() const;
	virtual const VDXWAVEFORMATEX* GetOutputFormat() const { return (VDXWAVEFORMATEX*)out_format; }
	virtual unsigned GetOutputFormatSize() const { return out_format_size; }
	virtual void     GetStreamInfo(VDXStreamInfo& si) const;

	virtual void Restart() {}
	virtual bool Convert(bool flush, bool requireOutput);

	virtual void* LockInputBuffer(unsigned& bytes);
	virtual void  UnlockInputBuffer(unsigned bytes);
	virtual const void* LockOutputBuffer(unsigned& bytes);
	virtual void  UnlockOutputBuffer(unsigned bytes);
	virtual unsigned    CopyOutput(void* dst, unsigned bytes, sint64& duration);
	virtual int SuggestFileFormat(const char* name);
};

class AConfigBase : public VDXVideoFilterDialog
{
public:
	VDFFAudio* codec = nullptr;
	std::unique_ptr<uint8_t[]> old_param;

	int dialog_id = -1;

	virtual ~AConfigBase() {}

	void Show(HWND parent, VDFFAudio* codec);

	virtual INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
};

extern VDXPluginInfo ff_aacenc_info;
extern VDXPluginInfo ff_mp3enc_info;
extern VDXPluginInfo ff_flacenc_info;
extern VDXPluginInfo ff_alacenc_info;
extern VDXPluginInfo ff_vorbisenc_info;
extern VDXPluginInfo ff_opusenc_info;
