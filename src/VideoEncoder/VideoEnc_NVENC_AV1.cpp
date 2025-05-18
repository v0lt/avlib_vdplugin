/*
 * Copyright (C) 2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

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
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override;
};

INT_PTR ConfigNVENC_AV1::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CodecNVENC_AV1::Config* config = (CodecNVENC_AV1::Config*)codec->config;
	switch (msg) {
	case WM_INITDIALOG:
	{
		SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_RESETCONTENT, 0, 0);
		for (const auto& preset_name : av1_nvenc_preset_names) {
			SendDlgItemMessageA(mhdlg, IDC_ENC_PROFILE, CB_ADDSTRING, 0, (LPARAM)preset_name);
		}
		SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_SETCURSEL, config->preset, 0);

		SendDlgItemMessageW(mhdlg, IDC_ENC_TUNE, CB_RESETCONTENT, 0, 0);
		for (const auto& tune_name : av1_nvenc_tune_names) {
			SendDlgItemMessageA(mhdlg, IDC_ENC_TUNE, CB_ADDSTRING, 0, (LPARAM)tune_name);
		}
		SendDlgItemMessageW(mhdlg, IDC_ENC_TUNE, CB_SETCURSEL, config->tune, 0);
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
	return true;
}

LRESULT CodecNVENC_AV1::configure(HWND parent)
{
	ConfigNVENC_AV1 dlg;
	dlg.Show(parent, this);
	return ICERR_OK;
}
