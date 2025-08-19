/*
 * Copyright (C) 2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "VideoEnc.h"

struct CodecNVENC_H264 : public CodecBase {
	enum { id_tag = CODEC_NVENC_H264 };

	struct Config : public CodecBase::Config {
		int preset;
		int tune;
		int qscale;

		Config() { reset(); }
		void reset() {
			version = 1;
			format  = format_yuv420;
			bits    = 8; // only 8 bit
			preset  = 5;
			tune    = 0;
			qscale  = 25;
		}
	} codec_config;

	static constexpr int codec_formats[] = {
		CodecBase::format_yuv420,
		CodecBase::format_yuv444,
	};
	static constexpr int codec_bitdepths[] = { 8 };

	CodecNVENC_H264() {
		codec_name = "h264_nvenc";
		codec_tag = MKTAG('H', '2', '6', '4');
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
		wcscpy_s(info.szName, L"h264_nvenc");
		wcscpy_s(info.szDescription, L"FFmpeg / NVENC H.264");
	}

	int compress_input_info(VDXPixmapLayout* src) override;

	bool init_ctx(VDXPixmapLayout* layout) override;

	LRESULT configure(HWND parent) override;
};
