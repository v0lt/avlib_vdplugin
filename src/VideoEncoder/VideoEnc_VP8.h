/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "VideoEnc.h"

class ConfigVP8 : public ConfigBase {
public:
	ConfigVP8() { dialog_id = IDD_ENC_VP8; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	virtual void init_format();
	virtual void change_format(int sel);
};

struct CodecVP8 : public CodecBase {
	enum { id_tag = MKTAG('V', 'P', '8', '0') };
	struct Config : public CodecBase::Config {
		int crf; // 4-63

		Config() { set_default(); }
		void clear() { CodecBase::Config::clear(); set_default(); }
		void set_default() {
			crf = 10;
			format = format_yuv420;
			bits = 8;
		}
	} codec_config;

	CodecVP8() {
		config = &codec_config;
		codec_name = "libvpx";
		codec_tag = MKTAG('V', 'P', '8', '0');
	}

	int config_size() { return sizeof(Config); }
	void reset_config() { codec_config.clear(); }

	virtual int compress_input_info(VDXPixmapLayout* src) {
		switch (src->format) {
		case nsVDXPixmap::kPixFormat_YUV420_Planar:
		case nsVDXPixmap::kPixFormat_YUV420_Alpha_Planar:
			return 1;
		}
		return 0;
	}

	void getinfo(ICINFO& info) {
		info.fccHandler = id_tag;
		info.dwFlags = VIDCF_COMPRESSFRAMES | VIDCF_FASTTEMPORALC;
		wcscpy_s(info.szName, L"vp8");
		wcscpy_s(info.szDescription, L"FFmpeg / VP8");
	}

	bool init_ctx(VDXPixmapLayout* layout)
	{
		avctx->gop_size = -1;
		avctx->max_b_frames = -1;
		avctx->bit_rate = 0x400000000000;

		[[maybe_unused]] int ret = 0;
		ret = av_opt_set_int(avctx->priv_data, "crf", codec_config.crf, 0);
		ret = av_opt_set_int(avctx->priv_data, "max-intra-rate", 0, 0);
		if (codec_config.format == format_yuva420) {
			av_opt_set_int(avctx->priv_data, "auto-alt-ref", 0, 0);
		}
		avctx->qmin = codec_config.crf;
		avctx->qmax = codec_config.crf;
		return true;
	}

	LRESULT configure(HWND parent)
	{
		ConfigVP8 dlg;
		dlg.Show(parent, this);
		return ICERR_OK;
	}
};

INT_PTR ConfigVP8::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CodecVP8::Config* config = (CodecVP8::Config*)codec->config;
	switch (msg) {
	case WM_INITDIALOG:
	{
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMIN, FALSE, 4);
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

void ConfigVP8::init_format()
{
	const char* color_names[] = {
		"YUV 4:2:0",
		"YUV 4:2:0 + Alpha",
	};

	SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_RESETCONTENT, 0, 0);
	for (const auto& color_name : color_names) {
		SendDlgItemMessageA(mhdlg, IDC_ENC_COLORSPACE, CB_ADDSTRING, 0, (LPARAM)color_name);
	}
	int sel = 0; // format_yuv420
	if (codec->config->format == CodecBase::format_yuva420) {
		sel = 1;
	}
	SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_SETCURSEL, sel, 0);
	EnableWindow(GetDlgItem(mhdlg, IDC_ENC_COLORSPACE), false); //! need to pass side_data
}

void ConfigVP8::change_format(int sel)
{
	int format = CodecBase::format_yuv420;
	if (sel == 1) {
		format = CodecBase::format_yuva420;
	}
	codec->config->format = format;
}
