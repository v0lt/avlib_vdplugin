/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stdafx.h"

#include "VideoEnc_x264.h"
#include "../Helper.h"
#include "../resource.h"

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

//
// ConfigX264
//

class ConfigX264 : public ConfigBase {
public:
	ConfigX264() { dialog_id = IDD_ENC_X264; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override;
};

INT_PTR ConfigX264::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CodecX264::Config* config = (CodecX264::Config*)codec->config;
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

//
// CodecX264
//

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\VideoEnc_x264"

void CodecX264::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		load_format_bitdepth(reg);
		reg.CheckString("preset", codec_config.preset, x264_preset_names);
		reg.CheckString("tune", codec_config.tune, x264_tune_names);
		reg.ReadInt("crf", codec_config.crf, 0, 51);
		reg.CloseKey();
	}
}

void CodecX264::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		save_format_bitdepth(reg);
		reg.WriteString("preset", x264_preset_names[codec_config.preset]);
		reg.WriteString("tune", x264_tune_names[codec_config.tune]);
		reg.WriteInt("crf", codec_config.crf);
		reg.CloseKey();
	}
}

int CodecX264::compress_input_info(VDXPixmapLayout* src)
{
	switch (src->format) {
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

bool CodecX264::init_ctx(VDXPixmapLayout* layout)
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

LRESULT CodecX264::configure(HWND parent)
{
	ConfigX264 dlg;
	dlg.Show(parent, this);
	return ICERR_OK;
}
