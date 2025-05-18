/*
 * Copyright (C) 2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "VideoEnc.h"

struct CodecAMF_H264 : public CodecBase {
	enum { id_tag = CODEC_AMF_H264 };

	struct Config : public CodecBase::Config {
		int preset;
		int tune;

		Config() { reset(); }
		void reset() {
			version = 1;
			format  = format_yuv420;
			bits    = 8; // only 8 bit
			preset  = 2; // "balanced"
			tune    = 0;
		}
	} codec_config;

	static constexpr int codec_formats[] = {
		CodecBase::format_yuv420,
	};
	static constexpr int codec_bitdepths[] = { 8 };

	CodecAMF_H264() {
		codec_name = "h264_amf";
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
		wcscpy_s(info.szName, L"h264_amf");
		wcscpy_s(info.szDescription, L"FFmpeg / AMF H.264");
	}

	bool test_bits(int format, int bits) override;

	int compress_input_info(VDXPixmapLayout* src) override;

	int compress_input_format(FilterModPixmapInfo* info) override;

	bool init_ctx(VDXPixmapLayout* layout) override;

	LRESULT configure(HWND parent) override;
};
