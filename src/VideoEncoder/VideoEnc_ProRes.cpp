/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "VideoEnc_ProRes.h"
#include "../resource.h"
#include "../registry.h"

const char* prores_profile_names[] = {
	"proxy",
	"LT",
	"standard",
	"high quality",
	"4444",
	"4444XQ",
};

//
// ConfigProres
//

class ConfigProres : public ConfigBase {
public:
	ConfigProres() { dialog_id = IDD_ENC_PRORES; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	virtual void init_format();
	virtual void change_format(int sel);
	void init_profile();
};


INT_PTR ConfigProres::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
	{
		init_profile();
		CodecProres::Config* config = (CodecProres::Config*)codec->config;
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMIN, FALSE, 2);
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMAX, TRUE, 31);
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETPOS, TRUE, config->qscale);
		SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, config->qscale, false);
		break;
	}

	case WM_HSCROLL:
		if ((HWND)lParam == GetDlgItem(mhdlg, IDC_ENC_QUALITY)) {
			CodecProres::Config* config = (CodecProres::Config*)codec->config;
			config->qscale = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_GETPOS, 0, 0);
			SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, config->qscale, false);
			break;
		}
		return FALSE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_ENC_PROFILE:
			if (HIWORD(wParam) == LBN_SELCHANGE) {
				CodecProres::Config* config = (CodecProres::Config*)codec->config;
				int v = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_GETCURSEL, 0, 0);
				if (config->format == CodecBase::format_yuva444) {
					config->profile = v + 4;
				}
				else {
					config->profile = v;
				}
				return TRUE;
			}
			break;
		}
	}
	return ConfigBase::DlgProc(msg, wParam, lParam);
}

void ConfigProres::init_profile()
{
	CodecProres::Config* config = (CodecProres::Config*)codec->config;
	if (config->format == CodecBase::format_yuva444) {
		if (config->profile < 4) {
			config->profile = 4;
		}

		const char* profile_names[] = {
			"4444",
			"4444XQ",
		};

		SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_RESETCONTENT, 0, 0);
		for (const auto& profile_name : profile_names) {
			SendDlgItemMessageA(mhdlg, IDC_ENC_PROFILE, CB_ADDSTRING, 0, (LPARAM)profile_name);
		}
		SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_SETCURSEL, config->profile - 4, 0);

	}
	else {
		SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_RESETCONTENT, 0, 0);
		for (const auto& profile_name : prores_profile_names) {
			SendDlgItemMessageA(mhdlg, IDC_ENC_PROFILE, CB_ADDSTRING, 0, (LPARAM)profile_name);
		}
		SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_SETCURSEL, config->profile, 0);
	}
}

void ConfigProres::init_format()
{
	const char* color_names[] = {
		"YUV 4:2:2",
		"YUV 4:4:4",
		"YUV 4:4:4 + Alpha",
	};

	SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_RESETCONTENT, 0, 0);
	for (const auto& color_name : color_names) {
		SendDlgItemMessageA(mhdlg, IDC_ENC_COLORSPACE, CB_ADDSTRING, 0, (LPARAM)color_name);
	}
	int sel = 0; // format_yuv422
	if (codec->config->format == CodecBase::format_yuv444) {
		sel = 1;
	}
	else if (codec->config->format == CodecBase::format_yuva444) {
		sel = 2;
	}
	SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_SETCURSEL, sel, 0);
}

void ConfigProres::change_format(int sel)
{
	int format = CodecBase::format_yuv422;
	if (sel == 1) {
		format = CodecBase::format_yuv444;
	}
	else if (sel == 2) {
		format = CodecBase::format_yuva444;
	}
	codec->config->format = format;
	init_profile();
}

//
// CodecProres
//

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\VideoEnc_Prores"

void CodecProres::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		size_t n = reg.CheckString("profile", prores_profile_names, std::size(prores_profile_names));
		if (n != -1) {
			codec_config.profile = (int)n;
		}
		reg.ReadInt("qscale", codec_config.qscale, 2, 31);
		reg.CloseKey();
	}
}

void CodecProres::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		reg.WriteString("profile", prores_profile_names[codec_config.profile]);
		reg.WriteInt("qscale", codec_config.qscale);
		reg.CloseKey();
	}
}

bool CodecProres::init_ctx(VDXPixmapLayout* layout)
{
	av_opt_set_int(avctx->priv_data, "profile", codec_config.profile, 0);
	if (codec_config.format == format_yuva444) {
		av_opt_set_int(avctx->priv_data, "alpha_bits", 16, 0);
	}
	avctx->flags |= AV_CODEC_FLAG_QSCALE;
	avctx->global_quality = FF_QP2LAMBDA * codec_config.qscale;
	return true;
}

LRESULT CodecProres::configure(HWND parent)
{
	ConfigProres dlg;
	dlg.Show(parent, this);
	return ICERR_OK;
}
