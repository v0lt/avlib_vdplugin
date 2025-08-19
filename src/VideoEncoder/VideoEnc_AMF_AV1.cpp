/*
 * Copyright (C) 2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stdafx.h"

#include "VideoEnc_AMF_AV1.h"
#include "../Helper.h"
#include "../resource.h"

const char* av1_amf_preset_names[] = {
	"default",
	"speed",
	"balanced",
	"quality",
	"high_quality",
};

//
// ConfigAMF_AV1
//

class ConfigAMF_AV1 : public ConfigBase {
public:
	ConfigAMF_AV1() { dialog_id = IDD_ENC_AMF_AV1; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override;
};

INT_PTR ConfigAMF_AV1::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CodecAMF_AV1::Config* config = (CodecAMF_AV1::Config*)codec->config;
	switch (msg) {
	case WM_INITDIALOG:
	{
		SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_RESETCONTENT, 0, 0);
		for (const auto& preset_name : av1_amf_preset_names) {
			SendDlgItemMessageA(mhdlg, IDC_ENC_PROFILE, CB_ADDSTRING, 0, (LPARAM)preset_name);
		}
		SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_SETCURSEL, config->preset, 0);

		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMIN, FALSE, 0);
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMAX, TRUE, 255);
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETPOS, TRUE, config->qscale);
		SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, config->qscale, false);
		break;
	}

	case WM_HSCROLL:
		if ((HWND)lParam == GetDlgItem(mhdlg, IDC_ENC_QUALITY)) {
			config->qscale = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_GETPOS, 0, 0);
			SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, config->qscale, false);
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
			SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETPOS, TRUE, config->qscale);
			SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, config->qscale, false);
			break;
		case IDC_ENC_PROFILE:
			if (HIWORD(wParam) == LBN_SELCHANGE) {
				config->preset = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_GETCURSEL, 0, 0);
				return TRUE;
			}
			break;
		}
	}
	return ConfigBase::DlgProc(msg, wParam, lParam);
}

//
// CodecAMF_AV1
//

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\VideoEnc_AMF_AV1"

void CodecAMF_AV1::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		load_format_bitdepth(reg);
		reg.CheckString("preset", codec_config.preset, av1_amf_preset_names);
		reg.ReadInt("qscale", codec_config.qscale, 0, 255);
		reg.CloseKey();
	}
}

void CodecAMF_AV1::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		save_format_bitdepth(reg);
		reg.WriteString("preset", av1_amf_preset_names[codec_config.preset]);
		reg.WriteInt("qscale", codec_config.qscale);
		reg.CloseKey();
	}
}

bool CodecAMF_AV1::test_bits(int format, int bits)
{
	switch (format) {
	case format_yuv420:
		if (bits == 8)  return test_av_format(AV_PIX_FMT_YUV420P);
		if (bits == 10) return test_av_format(AV_PIX_FMT_P010);
		break;
	}
	return false;
}

int CodecAMF_AV1::compress_input_info(VDXPixmapLayout* src)
{
	switch (src->format) {
	case nsVDXPixmap::kPixFormat_YUV420_Planar:
	case nsVDXPixmap::kPixFormat_YUV420_NV12: // also an acceptable format
	case nsVDXPixmap::kPixFormat_YUV420_P010:
		return 1;
	}
	return 0;
}

int CodecAMF_AV1::compress_input_format(FilterModPixmapInfo* info)
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

bool CodecAMF_AV1::init_ctx(VDXPixmapLayout* layout)
{
	avctx->gop_size = -1;
	avctx->max_b_frames = -1;

	avctx->profile = AV_PROFILE_AV1_MAIN;

	switch (avctx->colorspace) {
		case AVCOL_SPC_BT470BG:
			avctx->color_primaries = AVCOL_PRI_SMPTE170M;
			avctx->color_trc = AVCOL_TRC_SMPTE170M;
			break;
		case AVCOL_SPC_BT709:
		default:
			avctx->color_primaries = AVCOL_PRI_BT709;
			avctx->color_trc = AVCOL_TRC_BT709;
			break;
	}

	[[maybe_unused]] int ret = 0;
	ret = av_opt_set(avctx->priv_data, "preset", av1_amf_preset_names[codec_config.preset], 0);

	ret = av_opt_set(avctx->priv_data, "rc", "cqp", 0);
	ret = av_opt_set_int(avctx->priv_data, "qp_i", codec_config.qscale, 0);
	ret = av_opt_set_int(avctx->priv_data, "qp_p", codec_config.qscale, 0);
	ret = av_opt_set_int(avctx->priv_data, "qp_b", codec_config.qscale, 0);
	return true;
}

LRESULT CodecAMF_AV1::configure(HWND parent)
{
	ConfigAMF_AV1 dlg;
	dlg.Show(parent, this);
	return ICERR_OK;
}
