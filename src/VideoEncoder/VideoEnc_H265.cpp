/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "VideoEnc_H265.h"
#include "../resource.h"
#include "../registry.h"

const char* x265_preset_names[] = {
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

const char* x265_tune_names[] = {
	"none",
	"psnr",
	"ssim",
	"grain",
	"fastdecode",
	"zerolatency",
};

//
// ConfigH265
//

class ConfigH265 : public ConfigBase {
public:
	ConfigH265() { dialog_id = IDD_ENC_X265; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	virtual void init_format();
	virtual void change_format(int sel);
};

INT_PTR ConfigH265::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CodecH265::Config* config = (CodecH265::Config*)codec->config;
	switch (msg) {
	case WM_INITDIALOG:
	{
		SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_RESETCONTENT, 0, 0);
		for (const auto& preset_name : x265_preset_names) {
			SendDlgItemMessageA(mhdlg, IDC_ENC_PROFILE, CB_ADDSTRING, 0, (LPARAM)preset_name);
		}
		SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_SETCURSEL, config->preset, 0);

		SendDlgItemMessageW(mhdlg, IDC_ENC_TUNE, CB_RESETCONTENT, 0, 0);
		for (const auto& tune_name : x265_tune_names) {
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

void ConfigH265::init_format()
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

void ConfigH265::change_format(int sel)
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

//
// CodecH265
//

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\VideoEnc_H265"

void CodecH265::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		size_t n;
		n = reg.CheckString("preset", x265_preset_names, std::size(x265_preset_names));
		if (n != -1) {
			codec_config.preset = (int)n;
		}
		n = reg.CheckString("tune", x265_tune_names, std::size(x265_tune_names));
		if (n != -1) {
			codec_config.tune = (int)n;
		}
		reg.ReadInt("crf", codec_config.crf, 0, 51);
		reg.CloseKey();
	}
}

void CodecH265::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		reg.WriteString("preset", x265_preset_names[codec_config.preset]);
		reg.WriteString("tune", x265_preset_names[codec_config.tune]);
		reg.WriteInt("crf", codec_config.crf);
		reg.CloseKey();
	}
}

bool CodecH265::init_ctx(VDXPixmapLayout* layout)
{
	avctx->gop_size = -1;
	avctx->max_b_frames = -1;

	[[maybe_unused]] int ret = 0;
	ret = av_opt_set(avctx->priv_data, "preset", x265_preset_names[codec_config.preset], 0);
	if (codec_config.tune) {
		ret = av_opt_set(avctx->priv_data, "tune", x265_tune_names[codec_config.tune], 0);
	}
	ret = av_opt_set_double(avctx->priv_data, "crf", (double)codec_config.crf, 0);
	return true;
}

LRESULT CodecH265::configure(HWND parent)
{
	ConfigH265 dlg;
	dlg.Show(parent, this);
	return ICERR_OK;
}

//
// CodecH265LS
//

LRESULT CodecH265LS::configure(HWND parent)
{
	ConfigH265 dlg;
	dlg.dialog_id = IDD_ENC_X265LS;
	dlg.Show(parent, this);
	return ICERR_OK;
}
