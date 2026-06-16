/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2026 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stdafx.h"

#include "VideoEnc_x264.h"
#include "../Helper.h"
#include "../resource.h"

#define MIN_BITRATE 100
#define MAX_BITRATE 100'000

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
	void ChangeRateControl(CodecX264::Config* config);
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override;
};

void ConfigX264::ChangeRateControl(CodecX264::Config* pConfig)
{
	if (pConfig->rc == CodecX264::X264_RC_ABR) {
		SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL_SLIDER, TBM_SETRANGEMIN, FALSE, MIN_BITRATE);
		SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL_SLIDER, TBM_SETRANGEMAX, TRUE, scale2pos(MAX_BITRATE));
		SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL_SLIDER, TBM_SETPOS, TRUE, scale2pos(pConfig->bitrate));
		SetWindowTextW(GetDlgItem(mhdlg, IDC_ENC_RATECONTROL_DESC), L"Bitrate (kbit/s)");
		SetDlgItemInt(mhdlg, IDC_ENC_RATECONTROL_VALUE, pConfig->bitrate, FALSE);
	}
	else {
		SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL_SLIDER, TBM_SETRANGEMIN, FALSE, 0);
		SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL_SLIDER, TBM_SETRANGEMAX, TRUE, 51);
		SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL_SLIDER, TBM_SETPOS, TRUE, pConfig->crf);
		SetWindowTextW(GetDlgItem(mhdlg, IDC_ENC_RATECONTROL_DESC), L"Quality (high-low)");
		SetDlgItemInt(mhdlg, IDC_ENC_RATECONTROL_VALUE, pConfig->crf, FALSE);
	}
}

INT_PTR ConfigX264::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CodecX264::Config* config = (CodecX264::Config*)codec->config;
	switch (msg) {
	case WM_INITDIALOG: {
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

		SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_ADDSTRING, 0, (LPARAM)L"Constant Rate Factor (CRF)");
		SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_ADDSTRING, 0, (LPARAM)L"Average bitrate (ABR)");
		SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_SETCURSEL, (WPARAM)config->rc, 0);

		ChangeRateControl(config);
		break;
	}

	case WM_HSCROLL:
		if ((HWND)lParam == GetDlgItem(mhdlg, IDC_ENC_RATECONTROL_SLIDER)) {
			int value = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL_SLIDER, TBM_GETPOS, 0, 0);
			if (config->rc == 1) {
				config->bitrate = pos2scale(value);
				SetDlgItemInt(mhdlg, IDC_ENC_RATECONTROL_VALUE, config->bitrate, FALSE);
			} else {
				config->crf = value;
				SetDlgItemInt(mhdlg, IDC_ENC_RATECONTROL_VALUE, config->crf, FALSE);
			}
			break;
		}
		return FALSE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_BUTTON_DEFAULT:
			codec->reset_config();
			init_format();
			init_bits();
			SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_SETCURSEL, config->preset, 0);
			SendDlgItemMessageW(mhdlg, IDC_ENC_TUNE, CB_SETCURSEL, config->tune, 0);
			SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_SETCURSEL, config->rc, 0);
			ChangeRateControl(config);
			break;
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
		case IDC_ENC_RATECONTROL:
			if (HIWORD(wParam) == LBN_SELCHANGE) {
				config->rc = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_GETCURSEL, 0, 0);
				ChangeRateControl(config);
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
		reg.ReadInt("rate_control", codec_config.rc, 0, 1);
		reg.ReadInt("crf", codec_config.crf, 0, 51);
		reg.ReadInt("bitrate", codec_config.bitrate, MIN_BITRATE, MAX_BITRATE);
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
		reg.WriteInt("rate_control", codec_config.rc);
		reg.WriteInt("crf", codec_config.crf);
		reg.WriteInt("bitrate", codec_config.bitrate);
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
	if (codec_config.rc == X264_RC_ABR) {
		avctx->bit_rate = codec_config.bitrate * 1000;
	} else {
		ret = av_opt_set_double(avctx->priv_data, "crf", codec_config.crf, 0);
	}

	return true;
}

LRESULT CodecX264::configure(HWND parent)
{
	ConfigX264 dlg;
	dlg.Show(parent, this);
	return ICERR_OK;
}
