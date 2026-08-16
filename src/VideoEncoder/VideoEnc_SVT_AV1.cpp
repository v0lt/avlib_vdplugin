/*
 * Copyright (C) 2025-2026 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stdafx.h"

#include "VideoEnc_SVT_AV1.h"
#include "../resource.h"

const char* stv_av1_preset_names[] = {
	"0 - slow",
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	"8",
	"9",
	"10",
	"11",
	"12",
	"13 - fast",
};

 //
 // ConfigSVT_AV1
 //

class ConfigSVT_AV1 : public ConfigBase {
public:
	ConfigSVT_AV1() { dialog_id = IDD_ENC_AV1; }
	void ChangeRateControl(CodecSVT_AV1::Config* config);
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override;
};

void ConfigSVT_AV1::ChangeRateControl(CodecSVT_AV1::Config* pConfig)
{
	if (pConfig->rc == CodecSVT_AV1::SVT_AV1_RC_ABR) {
		SetBitrate(pConfig->bitrate);
	} else {
		SetQuality(pConfig->crf);
	}
}

INT_PTR ConfigSVT_AV1::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CodecSVT_AV1::Config* config = (CodecSVT_AV1::Config*)codec->config;
	switch (msg) {
	case WM_INITDIALOG:
	{
		SendDlgItemMessageW(mhdlg, IDC_ENC_PRESET, CB_RESETCONTENT, 0, 0);
		for (const auto& preset_name : stv_av1_preset_names) {
			SendDlgItemMessageA(mhdlg, IDC_ENC_PRESET, CB_ADDSTRING, 0, (LPARAM)preset_name);
		}
		SendDlgItemMessageW(mhdlg, IDC_ENC_PRESET, CB_SETCURSEL, config->preset, 0);

		SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_ADDSTRING, 0, (LPARAM)L"Constant Rate Factor (CRF)");
		SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_ADDSTRING, 0, (LPARAM)L"Average bitrate (ABR)");
		SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_SETCURSEL, (WPARAM)config->rc, 0);

		ChangeRateControl(config);
		break;
	}

	case WM_HSCROLL:
		if ((HWND)lParam == GetDlgItem(mhdlg, IDC_ENC_RATECONTROL_SLIDER)) {
			int value = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL_SLIDER, TBM_GETPOS, 0, 0);
			if (config->rc == CodecSVT_AV1::SVT_AV1_RC_ABR) {
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
			SendDlgItemMessageW(mhdlg, IDC_ENC_PRESET, CB_SETCURSEL, config->preset, 0);
			SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_SETCURSEL, config->rc, 0);
			ChangeRateControl(config);
			break;
		case IDC_ENC_PRESET:
			if (HIWORD(wParam) == LBN_SELCHANGE) {
				config->preset = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_PRESET, CB_GETCURSEL, 0, 0);
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
		case IDC_ENC_RATECONTROL_VALUE:
			if (config->rc == CodecSVT_AV1::SVT_AV1_RC_ABR) {
				OnRCValueProc(HIWORD(wParam), true, config->bitrate);
			} else {
				OnRCValueProc(HIWORD(wParam), false, config->crf);
			}
			break;
		}
	}
	return ConfigBase::DlgProc(msg, wParam, lParam);
}

//
// CodecSVT_AV1
//

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\VideoEnc_SVT_AV1"

void CodecSVT_AV1::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		load_format_bitdepth(reg);
		reg.ReadInt("preset", codec_config.preset, 0, 13);
		reg.ReadInt("rate_control", codec_config.rc, 0, 1);
		reg.ReadInt("crf", codec_config.crf, MIN_VIDEO_QP, MAX_VIDEO_QP);
		reg.ReadInt("bitrate", codec_config.bitrate, MIN_VIDEO_BITRATE, MAX_VIDEO_BITRATE);
		reg.CloseKey();
	}
}

void CodecSVT_AV1::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		save_format_bitdepth(reg);
		reg.WriteInt("preset", codec_config.preset);
		reg.WriteInt("rate_control", codec_config.rc);
		reg.WriteInt("crf", codec_config.crf);
		reg.WriteInt("bitrate", codec_config.bitrate);
		reg.CloseKey();
	}
}

bool CodecSVT_AV1::init_ctx(VDXPixmapLayout* layout)
{
	avctx->gop_size = -1;
	avctx->max_b_frames = -1;

	[[maybe_unused]] int ret = 0;
	ret = av_opt_set_int(avctx->priv_data, "preset", codec_config.preset, 0);
	if (codec_config.rc == SVT_AV1_RC_ABR) {
		avctx->bit_rate = codec_config.bitrate * 1000;
	} else {
		ret = av_opt_set_int(avctx->priv_data, "crf", codec_config.crf, 0);
	}

	return true;
}

LRESULT CodecSVT_AV1::configure(HWND parent)
{
	ConfigSVT_AV1 dlg;
	dlg.Show(parent, this);
	return ICERR_OK;
}
