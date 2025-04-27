/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "VideoEnc.h"

class ConfigH264 : public ConfigBase {
public:
	ConfigH264() { dialog_id = IDD_ENC_X264; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	virtual void init_format();
	virtual void change_format(int sel);
};

const char* x264_preset_names[] = {
	"ultrafast",
	"superfast",
	"veryfast",
	"faster",
	"fast",
	"medium",
	"slow",
	"slower",
	"veryslow",
};

const char* x264_tune_names[] = {
	"none",
	"film",
	"animation",
	"grain",
	"stillimage",
	"fastdecode",
	"zerolatency",
};

struct CodecH264 : public CodecBase {
	enum { id_tag = MKTAG('H', '2', '6', '4') };
	struct Config : public CodecBase::Config {
		int preset;
		int tune;
		int crf; // 0-51
		int flags; // reserved

		Config() { set_default(); }
		void clear() { CodecBase::Config::clear(); set_default(); }
		void set_default() {
			version = 1;
			format = format_yuv420;
			bits = 8;
			preset = 5;
			tune = 0;
			crf = 23;
			flags = 0;
		}
	} codec_config;

	CodecH264() {
		config = &codec_config;
		codec_name = "libx264";
		codec_tag = MKTAG('H', '2', '6', '4');
	}

	int config_size() { return sizeof(Config); }
	void reset_config() { codec_config.clear(); }

	void getinfo(ICINFO& info) {
		info.fccHandler = id_tag;
		info.dwFlags = VIDCF_COMPRESSFRAMES | VIDCF_FASTTEMPORALC;
		wcscpy_s(info.szName, L"x264");
		wcscpy_s(info.szDescription, L"FFmpeg / H.264 (x264)");
	}

	bool init_ctx(VDXPixmapLayout* layout)
	{
		avctx->gop_size = -1;
		avctx->max_b_frames = -1;

		[[maybe_unused]] int ret = 0;
		ret = av_opt_set(avctx->priv_data, "preset", x264_preset_names[codec_config.preset], 0);
		if (codec_config.tune) {
			ret = av_opt_set(avctx->priv_data, "tune", x264_tune_names[codec_config.tune], 0);
		}
		ret = av_opt_set_double(avctx->priv_data, "crf", codec_config.crf, 0);
		ret = av_opt_set(avctx->priv_data, "level", "3", 0);
		return true;
	}

	LRESULT configure(HWND parent)
	{
		ConfigH264 dlg;
		dlg.Show(parent, this);
		return ICERR_OK;
	}
};

INT_PTR ConfigH264::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CodecH264::Config* config = (CodecH264::Config*)codec->config;
	switch (msg) {
	case WM_INITDIALOG:
	{
		SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_RESETCONTENT, 0, 0);
		for (const auto& preset_name : x264_preset_names) {
			SendDlgItemMessageA(mhdlg, IDC_ENC_PROFILE, CB_ADDSTRING, 0, (LPARAM)preset_name);
		}
		SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_SETCURSEL, config->preset, 0);

		SendDlgItemMessageW(mhdlg, IDC_ENC_TUNE, CB_RESETCONTENT, 0, 0);
		for (const auto& tune_name : x264_tune_names) {
			SendDlgItemMessageA(mhdlg, IDC_ENC_TUNE, CB_ADDSTRING, 0, (LPARAM)tune_name);
		}
		SendDlgItemMessageW(mhdlg, IDC_ENC_TUNE, CB_SETCURSEL, config->tune, 0);

		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMIN, FALSE, 0);
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMAX, TRUE, 51);
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

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_ENC_PROFILE:
			if (HIWORD(wParam) == LBN_SELCHANGE) {
				config->preset = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_GETCURSEL, 0, 0);
				return TRUE;
			}
			break;
		case IDC_ENC_TUNE:
			if (HIWORD(wParam) == LBN_SELCHANGE) {
				config->tune = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_TUNE, CB_GETCURSEL, 0, 0);
				return TRUE;
			}
			break;
		}
	}
	return ConfigBase::DlgProc(msg, wParam, lParam);
}

void ConfigH264::init_format()
{
	const char* color_names[] = {
		"YUV 4:2:0",
		"YUV 4:2:2",
		"YUV 4:4:4",
	};

	SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_RESETCONTENT, 0, 0);
	for (const auto& color_name : color_names) {
		SendDlgItemMessageA(mhdlg, IDC_ENC_COLORSPACE, CB_ADDSTRING, 0, (LPARAM)color_name);
	}
	int sel = 0; // format_yuv420
	if (codec->config->format == CodecBase::format_yuv422) {
		sel = 1;
	}
	else if (codec->config->format == CodecBase::format_yuv444) {
		sel = 2;
	}
	SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_SETCURSEL, sel, 0);
}

void ConfigH264::change_format(int sel)
{
	int format = CodecBase::format_yuv420;
	if (sel == 1) {
		format = CodecBase::format_yuv422;
	}
	else if (sel == 2) {
		format = CodecBase::format_yuv444;
	}
	codec->config->format = format;
	init_bits();
}
