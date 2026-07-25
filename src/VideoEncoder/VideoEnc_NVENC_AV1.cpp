/*
 * Copyright (C) 2025-2026 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stdafx.h"

#include "VideoEnc_NVENC_AV1.h"
#include "../Helper.h"
#include "../resource.h"

const char* av1_nvenc_preset_names[] = {
	"p1",
	"p2",
	"p3",
	"p4",
	"p5",
	"p6",
	"p7",
};

const char* av1_nvenc_tune_names[] = {
	"hq",
	"uhq",
	"ll",
	"ull",
	"lossless",
};

//
// ConfigNVENC_AV1
//

class ConfigNVENC_AV1 : public ConfigBase {
public:
	ConfigNVENC_AV1() { dialog_id = IDD_ENC_NVENC_AV1; }
	void ChangeRateControl(CodecNVENC_AV1::Config* config);
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override;
};

void ConfigNVENC_AV1::ChangeRateControl(CodecNVENC_AV1::Config* pConfig)
{
	if (pConfig->rc == CODEC_RC_VBR) {
		SetBitrate(pConfig->bitrate);
	} else {
		SetQuality(pConfig->qscale);
	}
}

INT_PTR ConfigNVENC_AV1::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CodecNVENC_AV1::Config* config = (CodecNVENC_AV1::Config*)codec->config;
	switch (msg) {
	case WM_INITDIALOG:
	{
		SendDlgItemMessageW(mhdlg, IDC_ENC_PRESET, CB_RESETCONTENT, 0, 0);
		for (const auto& preset_name : av1_nvenc_preset_names) {
			SendDlgItemMessageA(mhdlg, IDC_ENC_PRESET, CB_ADDSTRING, 0, (LPARAM)preset_name);
		}
		SendDlgItemMessageW(mhdlg, IDC_ENC_PRESET, CB_SETCURSEL, config->preset, 0);

		SendDlgItemMessageW(mhdlg, IDC_ENC_TUNE, CB_RESETCONTENT, 0, 0);
		for (const auto& tune_name : av1_nvenc_tune_names) {
			SendDlgItemMessageA(mhdlg, IDC_ENC_TUNE, CB_ADDSTRING, 0, (LPARAM)tune_name);
		}
		SendDlgItemMessageW(mhdlg, IDC_ENC_TUNE, CB_SETCURSEL, config->tune, 0);

		SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_ADDSTRING, 0, (LPARAM)L"Constant QP");
		SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_ADDSTRING, 0, (LPARAM)L"Variable bitrate (VBR)");
		SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_SETCURSEL, (WPARAM)config->rc, 0);

		ChangeRateControl(config);
		EnableRateControl(av1_nvenc_tune_names[config->tune] != "lossless");
		break;
	}

	case WM_HSCROLL:
		if ((HWND)lParam == GetDlgItem(mhdlg, IDC_ENC_RATECONTROL_SLIDER)) {
			int value = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL_SLIDER, TBM_GETPOS, 0, 0);
			if (config->rc == CODEC_RC_VBR) {
				config->bitrate = pos2scale(value);
				SetDlgItemInt(mhdlg, IDC_ENC_RATECONTROL_VALUE, config->bitrate, FALSE);
			} else {
				config->qscale = value;
				SetDlgItemInt(mhdlg, IDC_ENC_RATECONTROL_VALUE, config->qscale, FALSE);
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
			SendDlgItemMessageW(mhdlg, IDC_ENC_TUNE, CB_SETCURSEL, config->tune, 0);
			SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_SETCURSEL, config->rc, 0);
			ChangeRateControl(config);
			break;
		case IDC_ENC_PRESET:
			if (HIWORD(wParam) == LBN_SELCHANGE) {
				config->preset = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_PRESET, CB_GETCURSEL, 0, 0);
				return TRUE;
			}
			break;
		case IDC_ENC_TUNE:
			if (HIWORD(wParam) == LBN_SELCHANGE) {
				config->tune = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_TUNE, CB_GETCURSEL, 0, 0);
				EnableRateControl(av1_nvenc_tune_names[config->tune] != "lossless");
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
// CodecNVENC_AV1
//

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\VideoEnc_NVENC_AV1"

void CodecNVENC_AV1::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		load_format_bitdepth(reg);
		reg.CheckString("preset", codec_config.preset, av1_nvenc_preset_names);
		reg.CheckString("tune", codec_config.tune, av1_nvenc_tune_names);
		reg.ReadInt("rate_control", codec_config.rc, 0, 1);
		reg.ReadInt("qscale", codec_config.qscale, MIN_VIDEO_QP, MAX_VIDEO_QP);
		reg.ReadInt("bitrate", codec_config.bitrate, MIN_VIDEO_BITRATE, MAX_VIDEO_BITRATE);
		reg.CloseKey();
	}
}

void CodecNVENC_AV1::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		save_format_bitdepth(reg);
		reg.WriteString("preset", av1_nvenc_preset_names[codec_config.preset]);
		reg.WriteString("tune", av1_nvenc_tune_names[codec_config.tune]);
		reg.WriteInt("rate_control", codec_config.rc);
		reg.WriteInt("qscale", codec_config.qscale);
		reg.WriteInt("bitrate", codec_config.bitrate);
		reg.CloseKey();
	}
}

bool CodecNVENC_AV1::test_bits(int format, int bits)
{
	switch (format) {
	case format_yuv420:
		if (bits == 8) return test_av_format(AV_PIX_FMT_YUV420P);
		if (bits == 10) return test_av_format(AV_PIX_FMT_P010);
		break;
	}
	return false;
}

int CodecNVENC_AV1::compress_input_info(VDXPixmapLayout* src)
{
	switch (src->format) {
	case nsVDXPixmap::kPixFormat_YUV420_Planar:
	case nsVDXPixmap::kPixFormat_YUV420_NV12: // also an acceptable format
	case nsVDXPixmap::kPixFormat_YUV420_P010:
	case nsVDXPixmap::kPixFormat_YUV420_P016: // also an acceptable format, truncated to 10bits
		return 1;
	}
	return 0;
}

int CodecNVENC_AV1::compress_input_format(FilterModPixmapInfo* info)
{
	if (config->format == format_yuv420) {
		if (config->bits == 8) {
			return nsVDXPixmap::kPixFormat_YUV420_Planar;
		}
		if (config->bits == 10) {
			if (info) {
				info->ref_r = 0xFFC0; // max value for Y
			}
			return nsVDXPixmap::kPixFormat_YUV420_P010;
		}
	}
	return 0;
}

bool CodecNVENC_AV1::init_ctx(VDXPixmapLayout* layout)
{
	avctx->gop_size = -1;
	avctx->max_b_frames = -1;

	[[maybe_unused]] int ret = 0;
	ret = av_opt_set(avctx->priv_data, "preset", av1_nvenc_preset_names[codec_config.preset], 0);
	ret = av_opt_set(avctx->priv_data, "tune", av1_nvenc_tune_names[codec_config.tune], 0);

	if (codec_config.rc == CODEC_RC_VBR) {
		ret = av_opt_set(avctx->priv_data, "rc", "vbr", 0);
		avctx->bit_rate = codec_config.bitrate * 1000;
	} else {
		ret = av_opt_set(avctx->priv_data, "rc", "constqp", 0);
		ret = av_opt_set_int(avctx->priv_data, "qp", codec_config.qscale, 0);
	}
	return true;
}

LRESULT CodecNVENC_AV1::configure(HWND parent)
{
	ConfigNVENC_AV1 dlg;
	dlg.Show(parent, this);
	return ICERR_OK;
}
