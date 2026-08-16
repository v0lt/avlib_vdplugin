/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2026 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stdafx.h"

#include "VideoEnc_VP8.h"
#include "../resource.h"

//
// ConfigVP8
//

class ConfigVP8 : public ConfigBase {
public:
	ConfigVP8() { dialog_id = IDD_ENC_VP8; }
	void ChangeRateControl(CodecVP8::Config* config);
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override;
};

void ConfigVP8::ChangeRateControl(CodecVP8::Config* pConfig)
{
	if (pConfig->rc == CodecVP8::VP8_RC_VBR) {
		SetBitrate(pConfig->bitrate);
	} else {
		SetQuality(pConfig->crf);
	}
}

INT_PTR ConfigVP8::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CodecVP8::Config* config = (CodecVP8::Config*)codec->config;
	switch (msg) {
	case WM_INITDIALOG:
	{
		SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_ADDSTRING, 0, (LPARAM)L"Constant Rate Factor (CRF)");
		SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_ADDSTRING, 0, (LPARAM)L"Variable bitrate (VBR)");
		SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_SETCURSEL, (WPARAM)config->rc, 0);

		ChangeRateControl(config);
		break;
	}

	case WM_HSCROLL:
		if ((HWND)lParam == GetDlgItem(mhdlg, IDC_ENC_RATECONTROL_SLIDER)) {
			int value = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL_SLIDER, TBM_GETPOS, 0, 0);
			if (config->rc == CodecVP8::VP8_RC_VBR) {
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
			SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_SETCURSEL, config->rc, 0);
			ChangeRateControl(config);
			break;
		case IDC_ENC_RATECONTROL:
			if (HIWORD(wParam) == LBN_SELCHANGE) {
				config->rc = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_GETCURSEL, 0, 0);
				ChangeRateControl(config);
				return TRUE;
			}
			break;
		case IDC_ENC_RATECONTROL_VALUE:
			if (config->rc == CodecVP8::VP8_RC_VBR) {
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
// CodecVP8
//

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\VideoEnc_VP8"

void CodecVP8::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		load_format_bitdepth(reg);
		reg.ReadInt("rate_control", codec_config.rc, 0, 1);
		reg.ReadInt("crf", codec_config.crf, MIN_VIDEO_QP, MAX_VIDEO_QP);
		reg.ReadInt("bitrate", codec_config.bitrate, MIN_VIDEO_BITRATE, MAX_VIDEO_BITRATE);
		reg.CloseKey();
	}
}

void CodecVP8::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		save_format_bitdepth(reg);
		reg.WriteInt("rate_control", codec_config.rc);
		reg.WriteInt("crf", codec_config.crf);
		reg.WriteInt("bitrate", codec_config.bitrate);
		reg.CloseKey();
	}
}

int CodecVP8::compress_input_info(VDXPixmapLayout* src)
{
	switch (src->format) {
	case nsVDXPixmap::kPixFormat_YUV420_Planar:
	case nsVDXPixmap::kPixFormat_YUV420_Alpha_Planar:
		return 1;
	}
	return 0;
}

bool CodecVP8::init_ctx(VDXPixmapLayout* layout)
{
	avctx->gop_size = -1;
	avctx->max_b_frames = -1;

	[[maybe_unused]] int ret = 0;
	if (codec_config.rc == VP8_RC_VBR) {
		avctx->bit_rate = codec_config.bitrate * 1000;
	} else {
		ret = av_opt_set_int(avctx->priv_data, "crf", codec_config.crf, 0);
		avctx->bit_rate = 100'000'000; // maximum allowed bitrate https://trac.ffmpeg.org/wiki/Encode/VP8#VariableBitrate
	}
	if (codec_config.format == format_yuva420) {
		ret = av_opt_set_int(avctx->priv_data, "auto-alt-ref", 0, 0);
	}

	return true;
}

LRESULT CodecVP8::configure(HWND parent)
{
	ConfigVP8 dlg;
	dlg.Show(parent, this);
	return ICERR_OK;
}
