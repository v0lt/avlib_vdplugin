/*
 * Copyright (C) 2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "VideoEnc.h"

struct CodecAV1 : public CodecBase {
	enum { id_tag = MKTAG('A', 'V', '0', '1') };

	struct Config : public CodecBase::Config {
		int preset; // 0-13
		int crf;    // 0-63

		Config() { reset(); }
		void reset() {
			version = 1;
			format  = format_yuv420;
			bits    = 8;
			preset  = 5;
			crf     = 35;
		}
	} codec_config;

	static constexpr int codec_formats[] = {
		CodecBase::format_yuv420,
	};
	static constexpr int codec_bitdepths[] = { 8, 10 };

	CodecAV1() {
		codec_name = "libsvtav1";
		codec_tag = MKTAG('A', 'V', '0', '1');
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
		wcscpy_s(info.szName, L"av1");
		wcscpy_s(info.szDescription, L"FFmpeg / SVT-AV1");
	}

	bool init_ctx(VDXPixmapLayout* layout) override;

	LRESULT configure(HWND parent) override;
};
