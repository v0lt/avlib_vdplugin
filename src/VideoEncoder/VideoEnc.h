/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <windows.h>
#include <commctrl.h>
#include <vfw.h>

#include <vd2/plugin/vdinputdriver.h>
#include <vd2/VDXFrame/VideoFilterDialog.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

#include <memory>

struct CodecClass {
	int class_id;

	CodecClass() { class_id = 0; }
	virtual ~CodecClass() {}
};

struct CodecBase : public CodecClass {
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

	struct Config {
		int version;
		int format;
		int bits;
	}*config = nullptr;

	AVRational time_base = {};
	int keyint = 1;
	LONG frame_total = 0;
	AVColorRange color_range = AVCOL_RANGE_UNSPECIFIED;
	AVColorSpace colorspace = AVCOL_SPC_UNSPECIFIED;

	const char* codec_name = nullptr;
	uint32_t codec_tag = 0;
	const AVCodec* codec = nullptr;
	AVCodecContext* avctx = nullptr;
	AVFrame* frame = nullptr;
	VDLogProc logProc = nullptr;
	bool global_header = false;

	virtual ~CodecBase() {
		compress_end();
	}

	bool init();

	virtual void getinfo(ICINFO& info) = 0;

	virtual int config_size() { return sizeof(Config); }

	virtual void reset_config() = 0;
	virtual void load_config() = 0;
	virtual void save_config() = 0;

	virtual bool load_config(void* data, size_t size);

	virtual bool test_av_format(AVPixelFormat format);

	bool test_bits(int format, int bits);

	virtual int compress_input_info(VDXPixmapLayout* src) { return 0; }

	LRESULT compress_input_format(FilterModPixmapInfo* info);

	LRESULT compress_get_format(BITMAPINFO* lpbiOutput, VDXPixmapLayout* layout);

	LRESULT compress_query(BITMAPINFO* lpbiOutput, VDXPixmapLayout* layout);

	LRESULT compress_get_size(BITMAPINFO* lpbiOutput);

	LRESULT compress_frames_info(ICCOMPRESSFRAMES* icf);

	virtual bool init_ctx(VDXPixmapLayout* layout) { return true; }

	LRESULT compress_begin(BITMAPINFO* lpbiOutput, VDXPixmapLayout* layout);

	void streamControl(VDXStreamControl* sc);

	void getStreamInfo(VDXStreamInfo* si);

	LRESULT compress_end();

	LRESULT compress1(ICCOMPRESS* icc, VDXPixmapLayout* layout);

	LRESULT compress2(ICCOMPRESS* icc, VDXPictureCompress* pc);

	virtual LRESULT configure(HWND parent) = 0;
};


class ConfigBase : public VDXVideoFilterDialog {
public:
	CodecBase* codec = nullptr;
	std::unique_ptr<uint8_t[]> old_param;
	int dialog_id = -1;
	int idc_message = -1;

	virtual ~ConfigBase() {}

	void Show(HWND parent, CodecBase* codec);

	virtual INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override;
	virtual void init_format();
	virtual void change_format(int sel);
	virtual void change_bits() {}
	void init_bits();
	void adjust_bits();
	void notify_bits_change(int bits_new, int bits_old);
	void notify_hide();
};
