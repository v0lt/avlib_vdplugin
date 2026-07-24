/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2026 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stdafx.h"

#include "VideoEnc_VP9.h"
#include "../resource.h"

//
// ConfigVP9
//

class ConfigVP9 : public ConfigBase {
public:
	ConfigVP9() { dialog_id = IDD_ENC_VP9; }
	void ChangeRateControl(CodecVP9::Config* config);
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override;
};

void ConfigVP9::ChangeRateControl(CodecVP9::Config* pConfig)
{
	if (pConfig->rc == CodecVP9::VP9_RC_VBR) {
		SetBitrate(pConfig->bitrate);
	} else {
		SetQuality(pConfig->crf);
	}
}

INT_PTR ConfigVP9::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CodecVP9::Config* config = (CodecVP9::Config*)codec->config;
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
			if (config->rc == CodecVP9::VP9_RC_VBR) {
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
		}
	}
	return ConfigBase::DlgProc(msg, wParam, lParam);
}

//
// CodecVP9
//

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\VideoEnc_VP9"

void CodecVP9::load_config()
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

void CodecVP9::save_config()
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

int CodecVP9::compress_input_info(VDXPixmapLayout* src)
{
	switch (src->format) {
	case nsVDXPixmap::kPixFormat_YUV420_Planar:
	case nsVDXPixmap::kPixFormat_YUV422_Planar:
	case nsVDXPixmap::kPixFormat_YUV444_Planar:
	case nsVDXPixmap::kPixFormat_YUV420_Planar16:
	case nsVDXPixmap::kPixFormat_YUV422_Planar16:
	case nsVDXPixmap::kPixFormat_YUV444_Planar16:
	case nsVDXPixmap::kPixFormat_XRGB8888:
	case nsVDXPixmap::kPixFormat_XRGB64:
		return 1;
	}
	return 0;
}

bool CodecVP9::init_ctx(VDXPixmapLayout* layout)
{
	avctx->gop_size = -1;
	avctx->max_b_frames = -1;

	[[maybe_unused]] int ret = 0;
	if (codec_config.rc == VP9_RC_VBR) {
		avctx->bit_rate = codec_config.bitrate * 1000;
	} else {
		ret = av_opt_set_int(avctx->priv_data, "crf", codec_config.crf, 0);
		avctx->bit_rate = 0; // MUST be 0 https://trac.ffmpeg.org/wiki/Encode/VP9#constantq
	}

	return true;
}

LRESULT CodecVP9::configure(HWND parent)
{
	ConfigVP9 dlg;
	dlg.Show(parent, this);
	return ICERR_OK;
}
