/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stdafx.h"

#include "VideoEnc_ProRes.h"
#include "../resource.h"

const char* prores_profile_422_names[] = {
	"proxy",
	"lt",
	"standard",
	"hq",
};

const char* prores_profile_4444_names[] = {
	"4444",
	"4444xq",
};

//
// ConfigProres
//

class ConfigProres : public ConfigBase {
public:
	ConfigProres() { dialog_id = IDD_ENC_PRORES; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override;
	void change_format(int sel) override;
	void init_profile();
};


INT_PTR ConfigProres::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CodecProres::Config* config = (CodecProres::Config*)codec->config;
	switch (msg) {
	case WM_INITDIALOG:
	{
		init_profile();
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMIN, FALSE, 2);
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMAX, TRUE, 31);
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETPOS, TRUE, config->qscale);
		SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, config->qscale, FALSE);
		break;
	}

	case WM_HSCROLL:
		if ((HWND)lParam == GetDlgItem(mhdlg, IDC_ENC_QUALITY)) {
			config->qscale = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_GETPOS, 0, 0);
			SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, config->qscale, FALSE);
			break;
		}
		return FALSE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_BUTTON_DEFAULT:
			codec->reset_config();
			init_format();
			init_bits();
			init_profile();
			SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETPOS, TRUE, config->qscale);
			SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, config->qscale, FALSE);
			break;
		case IDC_ENC_PROFILE:
			if (HIWORD(wParam) == LBN_SELCHANGE) {
				LRESULT ret = SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_GETCURSEL, 0, 0);
				if (ret >= 0) {
					config->profile = (int)ret;
				}
				return TRUE;
			}
			break;
		}
	}
	return ConfigBase::DlgProc(msg, wParam, lParam);
}

void ConfigProres::change_format(int sel)
{
	if (sel >= 0 && sel < (int)std::size(codec->formats)) {
		codec->config->format = codec->formats[sel];
		init_profile();
	}
}

void ConfigProres::init_profile()
{
	CodecProres* prores_codec = (CodecProres*)codec;

	if (codec->config->format == CodecBase::format_yuv422) {
		if (prores_codec->prores_profile_names.data() != prores_profile_422_names) {
			prores_codec->prores_profile_names = prores_profile_422_names;
			prores_codec->codec_config.profile = 3; // "HQ"
		}
	} else { // format_yuv444, format_yuva444
		if (prores_codec->prores_profile_names.data() != prores_profile_4444_names) {
			prores_codec->prores_profile_names = prores_profile_4444_names;
			prores_codec->codec_config.profile = 0; // "4444"
		}
	}

	SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_RESETCONTENT, 0, 0);
	for (const auto& profile_name : prores_codec->prores_profile_names) {
		SendDlgItemMessageA(mhdlg, IDC_ENC_PROFILE, CB_ADDSTRING, 0, (LPARAM)profile_name);
	}
	SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_SETCURSEL, prores_codec->codec_config.profile, 0);
}

//
// CodecProres
//

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\VideoEnc_Prores"

void CodecProres::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		load_format_bitdepth(reg);
		if (codec_config.format == CodecBase::format_yuv422) {
			prores_profile_names = prores_profile_422_names;
			codec_config.profile = 3; // "HQ"
		} else { // format_yuv444, format_yuva444
			prores_profile_names = prores_profile_4444_names;
			codec_config.profile = 0; // "4444"
		}
		reg.CheckString("profile", codec_config.profile, prores_profile_names);
		reg.ReadInt("qscale", codec_config.qscale, 2, 31);
		reg.CloseKey();
	}
}

void CodecProres::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		save_format_bitdepth(reg);
		reg.WriteString("profile", prores_profile_names[codec_config.profile]);
		reg.WriteInt("qscale", codec_config.qscale);
		reg.CloseKey();
	}
}

int CodecProres::compress_input_info(VDXPixmapLayout* src)
{
	switch (src->format) {
	case nsVDXPixmap::kPixFormat_YUV422_Planar16:
	case nsVDXPixmap::kPixFormat_YUV444_Planar16:
	case nsVDXPixmap::kPixFormat_YUV444_Alpha_Planar16:
		return 1;
	}
	return 0;
}

bool CodecProres::init_ctx(VDXPixmapLayout* layout)
{
	[[maybe_unused]] int ret = 0;
	ret = av_opt_set(avctx->priv_data, "profile", prores_profile_names[codec_config.profile], 0);
	if (codec_config.format == format_yuva444) {
		ret = av_opt_set_int(avctx->priv_data, "alpha_bits", 16, 0);
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
