/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2026 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "VideoEnc.h"

struct CodecVP9 : public CodecBase {
	enum { id_tag = CODEC_VP9 };

	enum {
		VP9_RC_CRF = 0,
		VP9_RC_VBR,
	};

	struct Config : public CodecBase::Config {
		int rc;
		int crf;
		int bitrate;

		Config() { reset(); }
		void reset() {
			version = 2;
			format  = format_yuv420;
			bits    = 8;
			rc      = VP9_RC_CRF;
			crf     = 15;
			bitrate = 3000;
		}
	} codec_config;

	static constexpr int codec_formats[] = {
		CodecBase::format_rgb,
		CodecBase::format_yuv420,
		CodecBase::format_yuv422,
		CodecBase::format_yuv444,
	};
	static constexpr int codec_bitdepths[] = { 8, 10, 12 };

	CodecVP9() {
		codec_name = "libvpx-vp9";
		codec_tag  = MKTAG('V', 'P', '9', '0');
		config = &codec_config;
		formats = codec_formats;
		bitdepths = codec_bitdepths;
		load_config();
	}

	int config_size() override { return sizeof(Config); }
	void reset_config() override { codec_config.reset(); }

	void load_config() override;
	void save_config() override;

	void getinfo(ICINFO& info) override {
		info.fccHandler = id_tag;
		info.dwFlags = VIDCF_COMPRESSFRAMES | VIDCF_FASTTEMPORALC;
		wcscpy_s(info.szName, L"vp9");
		wcscpy_s(info.szDescription, L"FFmpeg / VP9");
	}

	int compress_input_info(VDXPixmapLayout* src) override;

	bool init_ctx(VDXPixmapLayout* layout) override;

	LRESULT configure(HWND parent) override;
};
