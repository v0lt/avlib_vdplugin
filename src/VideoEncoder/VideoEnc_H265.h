/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "VideoEnc.h"

//
// CodecH265
//

struct CodecH265 : public CodecBase {
	enum { id_tag = MKTAG('H', 'E', 'V', 'C') };

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

	CodecH265() {
		codec_name = "libx265";
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
		wcscpy_s(info.szName, L"x265");
		wcscpy_s(info.szDescription, L"FFmpeg / HEVC (x265)");
	}

	virtual int compress_input_info(VDXPixmapLayout* src) {
		switch (src->format) {
		case nsVDXPixmap::kPixFormat_RGB888:
		case nsVDXPixmap::kPixFormat_XRGB64:
		case nsVDXPixmap::kPixFormat_YUV420_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_Planar:
		case nsVDXPixmap::kPixFormat_YUV444_Planar:
		case nsVDXPixmap::kPixFormat_YUV420_Planar16:
		case nsVDXPixmap::kPixFormat_YUV422_Planar16:
		case nsVDXPixmap::kPixFormat_YUV444_Planar16:
			return 1;
		}
		return 0;
	}

	bool init_ctx(VDXPixmapLayout* layout);

	LRESULT configure(HWND parent);
};

//
// CodecH265LS
//

struct CodecH265LS : public CodecBase {
	enum { id_tag = MKTAG('H', '2', '6', '5') }; // Here we use another one because 'HEVC' is already used in CodecH265

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

	CodecH265LS() {
		codec_name = "libx265";
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
		wcscpy_s(info.szName, L"x265 lossless");
		wcscpy_s(info.szDescription, L"FFmpeg / HEVC lossless (x265)");
	}

	virtual int compress_input_info(VDXPixmapLayout* src) {
		switch (src->format) {
		case nsVDXPixmap::kPixFormat_RGB888:
		case nsVDXPixmap::kPixFormat_XRGB64:
		case nsVDXPixmap::kPixFormat_YUV420_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_Planar:
		case nsVDXPixmap::kPixFormat_YUV444_Planar:
		case nsVDXPixmap::kPixFormat_YUV420_Planar16:
		case nsVDXPixmap::kPixFormat_YUV422_Planar16:
		case nsVDXPixmap::kPixFormat_YUV444_Planar16:
			return 1;
		}
		return 0;
	}

	bool init_ctx(VDXPixmapLayout* layout);

	LRESULT configure(HWND parent);
};
