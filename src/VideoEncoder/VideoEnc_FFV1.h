/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "VideoEnc.h"

struct CodecFFV1 : public CodecBase {
	enum { id_tag = MKTAG('F', 'F', 'V', '1') };

	struct Config : public CodecBase::Config {
		int level;    // 0, 1, 3
		int slices;   // 0-42
		int coder;    // 0-1
		int context;  // 0-1
		int slicecrc; // 0-1

		Config() { reset(); }
		void reset() {
			version  = 1;
			format   = format_yuv422;
			bits     = 10;
			level    = 3;
			slices   = 0;
			coder    = 1;
			context  = 0;
			slicecrc = 1;
		}
	} codec_config;

	static constexpr int codec_formats[] = {
		CodecBase::format_rgb,
		CodecBase::format_rgba,
		CodecBase::format_yuv420,
		CodecBase::format_yuv422,
		CodecBase::format_yuv444,
		CodecBase::format_yuva420,
		CodecBase::format_yuva422,
		CodecBase::format_yuva444,
		CodecBase::format_gray,
	};

	CodecFFV1() {
		codec_name = "ffv1";
		codec_tag = MKTAG('F', 'F', 'V', '1');
		config = &codec_config;
		formats = codec_formats;
		load_config();
	}

	int config_size() override { return sizeof(Config); }
	void reset_config() override { codec_config.reset(); }

	virtual void load_config();
	virtual void save_config();

	virtual bool test_av_format(AVPixelFormat format) {
		switch (format) {
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
			if (codec_config.level < 1) {
				return false;
			}
		}
		return CodecBase::test_av_format(format);
	}

	void getinfo(ICINFO& info) {
		info.fccHandler = id_tag;
		info.dwFlags = VIDCF_COMPRESSFRAMES | VIDCF_FASTTEMPORALC;
		wcscpy_s(info.szName, L"FFV1");
		wcscpy_s(info.szDescription, L"FFmpeg / FFV1 lossless codec");
	}

	virtual int compress_input_info(VDXPixmapLayout* src) override;

	bool init_ctx(VDXPixmapLayout* layout);

	LRESULT configure(HWND parent);
};
