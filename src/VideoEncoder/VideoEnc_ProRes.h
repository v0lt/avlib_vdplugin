/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "VideoEnc.h"

enum {
	PRORES_PROFILE_AUTO = -1,
	PRORES_PROFILE_PROXY = 0,
	PRORES_PROFILE_LT,
	PRORES_PROFILE_STANDARD,
	PRORES_PROFILE_HQ,
	PRORES_PROFILE_4444,
};

struct CodecProres : public CodecBase {
	enum { id_tag = MKTAG('a', 'p', 'c', 'h') };

	struct Config : public CodecBase::Config {
		int profile;
		int qscale; // 2-31

		Config() { reset(); }
		void reset() {
			version = 1;
			format  = format_yuv422;
			bits    = 10; // only 10 bit
			profile = PRORES_PROFILE_HQ;
			qscale  = 4;
		}
	} codec_config;

	CodecProres() {
		codec_name = "prores_ks";
		codec_tag = MKTAG('a', 'p', 'c', 'h');
		config = &codec_config;
		load_config();
	}

	int config_size() override { return sizeof(Config); }
	void reset_config() override { codec_config.reset(); }

	virtual void load_config() override;
	virtual void save_config() override;

	virtual int compress_input_info(VDXPixmapLayout* src) {
		switch (src->format) {
		case nsVDXPixmap::kPixFormat_YUV422_Planar16:
		case nsVDXPixmap::kPixFormat_YUV444_Planar16:
		case nsVDXPixmap::kPixFormat_YUV444_Alpha_Planar16:
			return 1;
		}
		return 0;
	}

	void getinfo(ICINFO& info) {
		info.fccHandler = id_tag;
		info.dwFlags = VIDCF_COMPRESSFRAMES;
		wcscpy_s(info.szName, L"prores_ks");
		wcscpy_s(info.szDescription, L"FFmpeg / Apple ProRes (iCodec Pro)");
	}

	bool init_ctx(VDXPixmapLayout* layout);

	LRESULT configure(HWND parent);
};
