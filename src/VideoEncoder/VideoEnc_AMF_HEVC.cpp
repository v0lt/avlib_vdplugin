/*
 * Copyright (C) 2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stdafx.h"

#include "VideoEnc_AMF_HEVC.h"
#include "../Helper.h"
#include "../resource.h"

const char* hevc_amf_preset_names[] = {
	"default",
	"speed",
	"balanced",
	"quality",
};

//
// ConfigAMF_HEVC
//

class ConfigAMF_HEVC : public ConfigBase {
public:
	ConfigAMF_HEVC() { dialog_id = IDD_ENC_AMF_HEVC; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override;
};

INT_PTR ConfigAMF_HEVC::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CodecAMF_HEVC::Config* config = (CodecAMF_HEVC::Config*)codec->config;
	switch (msg) {
	case WM_INITDIALOG:
	{
		SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_RESETCONTENT, 0, 0);
		for (const auto& preset_name : hevc_amf_preset_names) {
			SendDlgItemMessageA(mhdlg, IDC_ENC_PROFILE, CB_ADDSTRING, 0, (LPARAM)preset_name);
		}
		SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_SETCURSEL, config->preset, 0);
		break;
	}

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
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
// CodecAMF_HEVC
//

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\VideoEnc_AMF_HEVC"

void CodecAMF_HEVC::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		load_format_bitdepth(reg);
		reg.CheckString("preset", codec_config.preset, hevc_amf_preset_names);
		reg.CloseKey();
	}
}

void CodecAMF_HEVC::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		save_format_bitdepth(reg);
		reg.WriteString("preset", hevc_amf_preset_names[codec_config.preset]);
		reg.CloseKey();
	}
}

bool CodecAMF_HEVC::test_bits(int format, int bits)
{
	switch (format) {
	case format_yuv420:
		if (bits == 8)  return test_av_format(AV_PIX_FMT_YUV420P);
		if (bits == 10) return test_av_format(AV_PIX_FMT_P010);
		break;
	}
	return false;
}

int CodecAMF_HEVC::compress_input_info(VDXPixmapLayout* src)
{
	switch (src->format) {
	case nsVDXPixmap::kPixFormat_YUV420_Planar:
	case nsVDXPixmap::kPixFormat_YUV420_NV12: // also an acceptable format
	case nsVDXPixmap::kPixFormat_YUV420_P010:
		return 1;
	}
	return 0;
}

int CodecAMF_HEVC::compress_input_format(FilterModPixmapInfo* info)
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

bool CodecAMF_HEVC::init_ctx(VDXPixmapLayout* layout)
{
	avctx->gop_size = -1;
	avctx->max_b_frames = -1;

	avctx->profile = (config->bits == 10) ? AV_PROFILE_HEVC_MAIN_10 : AV_PROFILE_HEVC_MAIN;

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
	ret = av_opt_set(avctx->priv_data, "preset", hevc_amf_preset_names[codec_config.preset], 0);
	return true;
}

LRESULT CodecAMF_HEVC::configure(HWND parent)
{
	ConfigAMF_HEVC dlg;
	dlg.Show(parent, this);
	return ICERR_OK;
}
