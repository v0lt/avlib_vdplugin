/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "VideoEnc.h"

//
// CodecX265
//

struct CodecX265 : public CodecBase {
	enum { id_tag = MKTAG('X', '2', '6', '5') };

	struct Config : public CodecBase::Config {
		int preset;
		int tune;
		int crf; // 0-51

		Config() { reset(); }
		void reset() {
			version = 2;
			format = format_yuv420;
			bits = 8;
			preset = 4;
			tune = 0;
			crf = 28;
		}
	} codec_config;

	static constexpr int codec_formats[] = {
		CodecBase::format_rgb,
		CodecBase::format_yuv420,
		CodecBase::format_yuv422,
		CodecBase::format_yuv444,
	};
	static constexpr int codec_bitdepths[] = { 8, 10, 12 };

	CodecX265() {
		codec_name = "libx265";
		codec_tag = MKTAG('H', 'E', 'V', 'C');
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
		wcscpy_s(info.szName, L"x265");
		wcscpy_s(info.szDescription, L"FFmpeg / HEVC (x265)");
	}

	int compress_input_info(VDXPixmapLayout* src) override;

	bool init_ctx(VDXPixmapLayout* layout) override;

	LRESULT configure(HWND parent) override;
};

//
// CodecH265LS
//

struct CodecH265LS : public CodecBase {
	enum { id_tag = MKTAG('x', '2', '6', '5') }; // Here we use another one because 'X265' is already used in CodecX265

	struct Config : public CodecBase::Config {
		int preset;

		Config() { reset(); }
		void reset() {
			version = 2;
			format = format_yuv420;
			bits = 8;
			preset = 4;
		}
	} codec_config;

	static constexpr int codec_formats[] = {
		CodecBase::format_rgb,
		CodecBase::format_yuv420,
		CodecBase::format_yuv422,
		CodecBase::format_yuv444,
	};
	static constexpr int codec_bitdepths[] = { 8, 10, 12 };

	CodecH265LS() {
		codec_name = "libx265";
		codec_tag = MKTAG('H', 'E', 'V', 'C');
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
		wcscpy_s(info.szName, L"x265 lossless");
		wcscpy_s(info.szDescription, L"FFmpeg / HEVC lossless (x265)");
	}

	int compress_input_info(VDXPixmapLayout* src) override;

	bool init_ctx(VDXPixmapLayout* layout) override;

	LRESULT configure(HWND parent) override;
};
