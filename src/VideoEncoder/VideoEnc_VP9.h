/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "VideoEnc.h"

class ConfigVP9 : public ConfigBase {
public:
	ConfigVP9() { dialog_id = IDD_ENC_VP9; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	virtual void init_format();
	virtual void change_format(int sel);
};

struct CodecVP9 : public CodecBase {
	enum { id_tag = MKTAG('V', 'P', '9', '0') };

	struct Config : public CodecBase::Config {
		int crf; // 0-63

		Config() { reset(); }
		void reset() {
			version = 1;
			format = format_yuv420;
			bits = 8;
			crf = 15;
		}
	} codec_config;

	CodecVP9() {
		codec_name = "libvpx-vp9";
		codec_tag  = MKTAG('V', 'P', '9', '0');
		config = &codec_config;
		load_config();
	}

	int config_size() override { return sizeof(Config); }
	void reset_config() override { codec_config.reset(); }

	virtual void load_config() override {

	}
	virtual void save_config() override {

	}

	virtual int compress_input_info(VDXPixmapLayout* src) {
		switch (src->format) {
		case nsVDXPixmap::kPixFormat_YUV420_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_Planar:
		case nsVDXPixmap::kPixFormat_YUV444_Planar:
		case nsVDXPixmap::kPixFormat_YUV420_Planar16:
		case nsVDXPixmap::kPixFormat_YUV422_Planar16:
		case nsVDXPixmap::kPixFormat_YUV444_Planar16:
		case nsVDXPixmap::kPixFormat_XRGB8888:
		case nsVDXPixmap::kPixFormat_XRGB64:
			return 1;
		}
		return 0;
	}

	void getinfo(ICINFO& info) {
		info.fccHandler = id_tag;
		info.dwFlags = VIDCF_COMPRESSFRAMES | VIDCF_FASTTEMPORALC;
		wcscpy_s(info.szName, L"vp9");
		wcscpy_s(info.szDescription, L"FFmpeg / VP9");
	}

	bool init_ctx(VDXPixmapLayout* layout)
	{
		avctx->gop_size = -1;
		avctx->max_b_frames = -1;

		[[maybe_unused]] int ret = 0;
		ret = av_opt_set_int(avctx->priv_data, "crf", codec_config.crf, 0);
		ret = av_opt_set_int(avctx->priv_data, "max-intra-rate", 0, 0);
		avctx->qmin = codec_config.crf;
		avctx->qmax = codec_config.crf;
		return true;
	}

	LRESULT configure(HWND parent)
	{
		ConfigVP9 dlg;
		dlg.Show(parent, this);
		return ICERR_OK;
	}
};

INT_PTR ConfigVP9::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CodecVP9::Config* config = (CodecVP9::Config*)codec->config;
	switch (msg) {
	case WM_INITDIALOG:
	{
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMIN, FALSE, 0);
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMAX, TRUE, 63);
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETPOS, TRUE, config->crf);
		SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, config->crf, false);
		break;
	}

	case WM_HSCROLL:
		if ((HWND)lParam == GetDlgItem(mhdlg, IDC_ENC_QUALITY)) {
			config->crf = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_GETPOS, 0, 0);
			SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, config->crf, false);
			break;
		}
		return FALSE;
	}
	return ConfigBase::DlgProc(msg, wParam, lParam);
}

void ConfigVP9::init_format()
{
	const char* color_names[] = {
		"RGB",
		"YUV 4:2:0",
		"YUV 4:2:2",
		"YUV 4:4:4",
	};

	SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_RESETCONTENT, 0, 0);
	for (const auto& color_name : color_names) {
		SendDlgItemMessageA(mhdlg, IDC_ENC_COLORSPACE, CB_ADDSTRING, 0, (LPARAM)color_name);
	}
	int sel = 0; // format_rgb
	if (codec->config->format == CodecBase::format_yuv420) {
		sel = 1;
	}
	else if (codec->config->format == CodecBase::format_yuv422) {
		sel = 2;
	}
	else if (codec->config->format == CodecBase::format_yuv444) {
		sel = 3;
	}
	SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_SETCURSEL, sel, 0);
}

void ConfigVP9::change_format(int sel)
{
	int format = CodecBase::format_rgb;
	if (sel == 1) {
		format = CodecBase::format_yuv420;
	}
	else if (sel == 2) {
		format = CodecBase::format_yuv422;
	}
	else if (sel == 3) {
		format = CodecBase::format_yuv444;
	}
	codec->config->format = format;
	init_bits();
}
