/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "VideoEnc.h"

struct CodecVP9 : public CodecBase {
	enum { id_tag = MKTAG('V', 'P', '9', '0') };

	struct Config : public CodecBase::Config {
		int crf; // 0-63

		Config() { reset(); }
		void reset() {
			version = 1;
			format = format_yuv420;
			bits = 8;
			crf = 15;
		}
	} codec_config;

	CodecVP9() {
		codec_name = "libvpx-vp9";
		codec_tag  = MKTAG('V', 'P', '9', '0');
		config = &codec_config;
		load_config();
	}

	int config_size() override { return sizeof(Config); }
	void reset_config() override { codec_config.reset(); }

	virtual void load_config() override;
	virtual void save_config() override;

	virtual int compress_input_info(VDXPixmapLayout* src) {
		switch (src->format) {
		case nsVDXPixmap::kPixFormat_YUV420_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_Planar:
		case nsVDXPixmap::kPixFormat_YUV444_Planar:
		case nsVDXPixmap::kPixFormat_YUV420_Planar16:
		case nsVDXPixmap::kPixFormat_YUV422_Planar16:
		case nsVDXPixmap::kPixFormat_YUV444_Planar16:
		case nsVDXPixmap::kPixFormat_XRGB8888:
		case nsVDXPixmap::kPixFormat_XRGB64:
			return 1;
		}
		return 0;
	}

	void getinfo(ICINFO& info) {
		info.fccHandler = id_tag;
		info.dwFlags = VIDCF_COMPRESSFRAMES | VIDCF_FASTTEMPORALC;
		wcscpy_s(info.szName, L"vp9");
		wcscpy_s(info.szDescription, L"FFmpeg / VP9");
	}

	bool init_ctx(VDXPixmapLayout* layout);

	LRESULT configure(HWND parent);
};
