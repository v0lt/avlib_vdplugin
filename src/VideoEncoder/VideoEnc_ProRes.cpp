/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stdafx.h"

#include "VideoEnc_ProRes.h"
#include "../resource.h"
#include "../Helper.h"

#define ENABLE_PRORES_QSCALE 0

const char* prores_profile_names[] = {
	"proxy",
	"lt",
	"standard",
	"hq",
	"4444",
	"4444xq",
};

const int prores_profile_ids[] = {
	PRORES_PROFILE_PROXY,
	PRORES_PROFILE_LT,
	PRORES_PROFILE_STANDARD,
	PRORES_PROFILE_HQ,
	PRORES_PROFILE_4444,
	PRORES_PROFILE_4444XQ,
};

static_assert(std::size(prores_profile_names) == std::size(prores_profile_ids));

const std::span<const char*> prores_profile_422_names  = std::span(&prores_profile_names[0], 4);
const std::span<const char*> prores_profile_4444_names = std::span(&prores_profile_names[4], 2);

const std::span<const int> prores_profile_422_ids  = std::span(&prores_profile_ids[0], 4);
const std::span<const int> prores_profile_4444_ids = std::span(&prores_profile_ids[4], 2);

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
#if ENABLE_PRORES_QSCALE
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETPOS, TRUE, config->qscale);
		SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, config->qscale, FALSE);
#else
		ShowWindow(GetDlgItem(mhdlg, IDC_ENC_QUALITY), SW_HIDE);
		ShowWindow(GetDlgItem(mhdlg, IDC_ENC_QUALITY_LABEL), SW_HIDE);
		ShowWindow(GetDlgItem(mhdlg, IDC_ENC_QUALITY_VALUE), SW_HIDE);
#endif
		break;
	}

#if ENABLE_PRORES_QSCALE
	case WM_HSCROLL:
		if ((HWND)lParam == GetDlgItem(mhdlg, IDC_ENC_QUALITY)) {
			config->qscale = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_GETPOS, 0, 0);
			SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, config->qscale, FALSE);
			break;
		}
		return FALSE;
#endif

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_BUTTON_DEFAULT:
			codec->reset_config();
			init_format();
			init_bits();
			init_profile();
#if ENABLE_PRORES_QSCALE
			SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETPOS, TRUE, config->qscale);
			SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, config->qscale, FALSE);
#endif
			break;
		case IDC_ENC_PROFILE:
			if (HIWORD(wParam) == LBN_SELCHANGE) {
				LRESULT ret = GetCurrentItemData(mhdlg, IDC_ENC_PROFILE);
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

	SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_RESETCONTENT, 0, 0);

	if (codec->config->format == CodecBase::format_yuv422) {
		if (prores_codec->codec_config.profile < PRORES_PROFILE_PROXY || prores_codec->codec_config.profile > PRORES_PROFILE_HQ) {
			prores_codec->codec_config.profile = PRORES_PROFILE_HQ;
		}
		for (size_t i = 0; i < std::size(prores_profile_422_names); i++) {
			AddStringSetData(mhdlg, IDC_ENC_PROFILE, prores_profile_422_names[i], prores_profile_422_ids[i]);
		}
	}
	else { // format_yuv444, format_yuva444
		if (prores_codec->codec_config.profile < PRORES_PROFILE_4444 || prores_codec->codec_config.profile > PRORES_PROFILE_4444XQ) {
			prores_codec->codec_config.profile = PRORES_PROFILE_4444;
		}
		for (size_t i = 0; i < std::size(prores_profile_4444_names); i++) {
			AddStringSetData(mhdlg, IDC_ENC_PROFILE, prores_profile_4444_names[i], prores_profile_4444_ids[i]);
		}
	}

	SelectByItemData(mhdlg, IDC_ENC_PROFILE, prores_codec->codec_config.profile);
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
		int index = -1;
		if (codec_config.format == CodecBase::format_yuv422) {
			codec_config.profile = PRORES_PROFILE_HQ;
			reg.CheckString("profile", index, prores_profile_422_names);
			if (index >= 0) {
				codec_config.profile = prores_profile_422_ids[index];
			}
		}
		else { // format_yuv444, format_yuva444
			codec_config.profile = PRORES_PROFILE_4444;
			reg.CheckString("profile", index, prores_profile_4444_names);
			if (index >= 0) {
				codec_config.profile = prores_profile_4444_ids[index];
			}
		}
#if ENABLE_PRORES_QSCALE
		reg.ReadInt("qscale", codec_config.qscale, 2, 31);
#endif
		reg.CloseKey();
	}
}

void CodecProres::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		save_format_bitdepth(reg);
		reg.WriteString("profile", prores_profile_names[codec_config.profile]);
#if ENABLE_PRORES_QSCALE
		reg.WriteInt("qscale", codec_config.qscale);
#endif
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
#if ENABLE_PRORES_QSCALE
	avctx->flags |= AV_CODEC_FLAG_QSCALE;
	avctx->global_quality = FF_QP2LAMBDA * codec_config.qscale;
#endif
	return true;
}

LRESULT CodecProres::configure(HWND parent)
{
	ConfigProres dlg;
	dlg.Show(parent, this);
	return ICERR_OK;
}
