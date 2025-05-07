/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "VideoEnc.h"

struct CodecHUFF : public CodecBase {
	enum { id_tag = MKTAG('F', 'F', 'V', 'H') };

	struct Config : public CodecBase::Config {
		int prediction = 0;

		Config() { reset(); }
		void reset() {
			version    = 1;
			format     = format_rgb;
			bits       = 8;
			prediction = 0;
		}
	} codec_config;

	CodecHUFF() {
		codec_name = "ffvhuff";
		codec_tag = MKTAG('F', 'F', 'V', 'H');
		config = &codec_config;
		load_config();
	}

	int config_size() override { return sizeof(Config); }
	void reset_config() override { codec_config.reset(); }

	virtual void load_config() override;
	virtual void save_config() override;

	void getinfo(ICINFO& info) {
		info.fccHandler = id_tag;
		info.dwFlags = VIDCF_COMPRESSFRAMES;
		wcscpy_s(info.szName, L"FFVHUFF");
		wcscpy_s(info.szDescription, L"FFmpeg / FFVHuff lossless codec");
	}

	virtual int compress_input_info(VDXPixmapLayout* src) {
		switch (src->format) {
		case nsVDXPixmap::kPixFormat_RGB888:
		case nsVDXPixmap::kPixFormat_XRGB8888:
		case nsVDXPixmap::kPixFormat_XRGB64:
		case nsVDXPixmap::kPixFormat_YUV420_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_Planar:
		case nsVDXPixmap::kPixFormat_YUV444_Planar:
		case nsVDXPixmap::kPixFormat_YUV420_Planar16:
		case nsVDXPixmap::kPixFormat_YUV422_Planar16:
		case nsVDXPixmap::kPixFormat_YUV444_Planar16:
		case nsVDXPixmap::kPixFormat_YUV420_Alpha_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_Alpha_Planar:
		case nsVDXPixmap::kPixFormat_YUV444_Alpha_Planar:
		case nsVDXPixmap::kPixFormat_YUV420_Alpha_Planar16:
		case nsVDXPixmap::kPixFormat_YUV422_Alpha_Planar16:
		case nsVDXPixmap::kPixFormat_YUV444_Alpha_Planar16:
		case nsVDXPixmap::kPixFormat_Y8:
		case nsVDXPixmap::kPixFormat_Y16:
			return 1;
		}
		return 0;
	}

	bool init_ctx(VDXPixmapLayout* layout);

	LRESULT configure(HWND parent);
};
