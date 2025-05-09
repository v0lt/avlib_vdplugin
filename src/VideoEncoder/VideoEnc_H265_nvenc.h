/*
 * Copyright (C) 2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "VideoEnc.h"

struct CodecH265_NVENC : public CodecBase {
	enum { id_tag = MKTAG('h', '2', '6', '5') };

	struct Config : public CodecBase::Config {
		int preset;
		int tune;

		Config() { reset(); }
		void reset() {
			version = 1;
			format  = format_yuv420;
			bits    = 8;
			preset  = 5;
			tune    = 0;
		}
	} codec_config;

	CodecH265_NVENC() {
		codec_name = "hevc_nvenc";
		codec_tag = MKTAG('H', 'E', 'V', 'C');
		config = &codec_config;
		load_config();
	}

	int config_size() override { return sizeof(Config); }
	void reset_config() override { codec_config.reset(); }

	virtual void load_config() override;
	virtual void save_config() override;

	void getinfo(ICINFO& info) {
		info.fccHandler = id_tag;
		info.dwFlags = VIDCF_COMPRESSFRAMES | VIDCF_FASTTEMPORALC;
		wcscpy_s(info.szName, L"hevc_nvenc");
		wcscpy_s(info.szDescription, L"FFmpeg / NVENC HEVC");
	}

	bool init_ctx(VDXPixmapLayout* layout);

	LRESULT configure(HWND parent);
};
