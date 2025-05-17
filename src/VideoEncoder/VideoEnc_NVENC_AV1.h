/*
 * Copyright (C) 2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "VideoEnc.h"

struct CodecNVENC_AV1 : public CodecBase {
	enum { id_tag = MKTAG('a', 'v', '0', '1') };

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

	static constexpr int codec_formats[] = {
		CodecBase::format_yuv420,
	};

	CodecNVENC_AV1() {
		codec_name = "av1_nvenc";
		codec_tag = MKTAG('A', 'V', '0', '1');
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
		wcscpy_s(info.szName, L"av1_nvenc");
		wcscpy_s(info.szDescription, L"FFmpeg / NVENC AV1");
	}

	virtual bool test_bits(int format, int bits) override;

	virtual int compress_input_info(VDXPixmapLayout* src) override;

	virtual int compress_input_format(FilterModPixmapInfo* info) override;

	bool init_ctx(VDXPixmapLayout* layout);

	LRESULT configure(HWND parent);
};
