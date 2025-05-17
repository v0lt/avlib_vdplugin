/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "VideoEnc.h"

struct CodecX264 : public CodecBase {
	enum { id_tag = MKTAG('X', '2', '6', '4') };

	struct Config : public CodecBase::Config {
		int preset;
		int tune;
		int crf; // 0-51
		int flags; // reserved

		Config() { reset(); }
		void reset() {
			version = 1;
			format = format_yuv420;
			bits = 8;
			preset = 5;
			tune = 0;
			crf = 23;
			flags = 0;
		}
	} codec_config;

	static constexpr int codec_formats[] = {
		CodecBase::format_yuv420,
		CodecBase::format_yuv422,
		CodecBase::format_yuv444,
	};

	CodecX264() {
		codec_name = "libx264";
		codec_tag = MKTAG('H', '2', '6', '4');
		config = &codec_config;
		formats = codec_formats;
		load_config();
	}

	int config_size() override { return sizeof(Config); }
	void reset_config() override { codec_config.reset(); }

	virtual void load_config() override;
	virtual void save_config() override;

	void getinfo(ICINFO& info) {
		info.fccHandler = id_tag;
		info.dwFlags = VIDCF_COMPRESSFRAMES | VIDCF_FASTTEMPORALC;
		wcscpy_s(info.szName, L"x264");
		wcscpy_s(info.szDescription, L"FFmpeg / H.264 (x264)");
	}

	virtual int compress_input_info(VDXPixmapLayout* src) override;

	bool init_ctx(VDXPixmapLayout* layout);

	LRESULT configure(HWND parent);
};
