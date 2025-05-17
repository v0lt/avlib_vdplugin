/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "VideoEnc_x265.h"
#include "../resource.h"
#include "../registry.h"

const int x265_bitdepths[] = { 8, 10, 12 };

const char* x265_preset_names[] = {
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

const char* x265_tune_names[] = {
	"none",
	"psnr",
	"ssim",
	"grain",
	"fastdecode",
	"zerolatency",
};

//
// ConfigX265
//

class ConfigX265 : public ConfigBase {
public:
	ConfigX265() { dialog_id = IDD_ENC_X265; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override;
};

INT_PTR ConfigX265::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CodecX265::Config* config = (CodecX265::Config*)codec->config;
	switch (msg) {
	case WM_INITDIALOG:
	{
		SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_RESETCONTENT, 0, 0);
		for (const auto& preset_name : x265_preset_names) {
			SendDlgItemMessageA(mhdlg, IDC_ENC_PROFILE, CB_ADDSTRING, 0, (LPARAM)preset_name);
		}
		SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_SETCURSEL, config->preset, 0);

		SendDlgItemMessageW(mhdlg, IDC_ENC_TUNE, CB_RESETCONTENT, 0, 0);
		for (const auto& tune_name : x265_tune_names) {
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
// CodecX265
//

#define REG_KEY_H265 "Software\\VirtualDub2\\avlib\\VideoEnc_x265"

void CodecX265::load_config()
{
	RegistryPrefs reg(REG_KEY_H265);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		reg.ReadInt("format", codec_config.format, formats);
		reg.ReadInt("bitdepth", codec_config.bits, x265_bitdepths);
		reg.CheckString("preset", codec_config.preset, x265_preset_names);
		reg.CheckString("tune", codec_config.tune, x265_tune_names);
		reg.ReadInt("crf", codec_config.crf, 0, 51);
		reg.CloseKey();
	}
}

void CodecX265::save_config()
{
	RegistryPrefs reg(REG_KEY_H265);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		reg.WriteInt("format", codec_config.format);
		reg.WriteInt("bitdepth", codec_config.bits);
		reg.WriteString("preset", x265_preset_names[codec_config.preset]);
		reg.WriteString("tune", x265_tune_names[codec_config.tune]);
		reg.WriteInt("crf", codec_config.crf);
		reg.CloseKey();
	}
}

int CodecX265::compress_input_info(VDXPixmapLayout* src)
{
	switch (src->format) {
	case nsVDXPixmap::kPixFormat_RGB888:
	case nsVDXPixmap::kPixFormat_XRGB64:
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

bool CodecX265::init_ctx(VDXPixmapLayout* layout)
{
	avctx->gop_size = -1;
	avctx->max_b_frames = -1;

	[[maybe_unused]] int ret = 0;
	ret = av_opt_set(avctx->priv_data, "preset", x265_preset_names[codec_config.preset], 0);
	if (codec_config.tune) {
		ret = av_opt_set(avctx->priv_data, "tune", x265_tune_names[codec_config.tune], 0);
	}
	ret = av_opt_set_double(avctx->priv_data, "crf", (double)codec_config.crf, 0);
	return true;
}

LRESULT CodecX265::configure(HWND parent)
{
	ConfigX265 dlg;
	dlg.Show(parent, this);
	return ICERR_OK;
}

//
// ConfigH265LS
//

class ConfigH265LS : public ConfigBase {
public:
	ConfigH265LS() { dialog_id = IDD_ENC_X265LS; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override;
};

INT_PTR ConfigH265LS::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CodecH265LS::Config* config = (CodecH265LS::Config*)codec->config;
	switch (msg) {
	case WM_INITDIALOG:
	{
		SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_RESETCONTENT, 0, 0);
		for (const auto& preset_name : x265_preset_names) {
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
// CodecH265LS
//

#define REG_KEY_H265LS "Software\\VirtualDub2\\avlib\\VideoEnc_x265LS"

void CodecH265LS::load_config()
{
	RegistryPrefs reg(REG_KEY_H265LS);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		reg.ReadInt("format", codec_config.format, formats);
		reg.ReadInt("bitdepth", codec_config.bits, x265_bitdepths);
		reg.CheckString("preset", codec_config.preset, x265_preset_names);
		reg.CloseKey();
	}
}

void CodecH265LS::save_config()
{
	RegistryPrefs reg(REG_KEY_H265LS);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		reg.WriteInt("format", codec_config.format);
		reg.WriteInt("bitdepth", codec_config.bits);
		reg.WriteString("preset", x265_preset_names[codec_config.preset]);
		reg.CloseKey();
	}
}

int CodecH265LS::compress_input_info(VDXPixmapLayout* src)
{
	switch (src->format) {
	case nsVDXPixmap::kPixFormat_RGB888:
	case nsVDXPixmap::kPixFormat_XRGB64:
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

bool CodecH265LS::init_ctx(VDXPixmapLayout* layout)
{
	avctx->gop_size = -1;
	avctx->max_b_frames = -1;

	[[maybe_unused]] int ret = 0;
	ret = av_opt_set(avctx->priv_data, "preset", x265_preset_names[codec_config.preset], 0);
	ret = av_opt_set(avctx->priv_data, "x265-params", "lossless=1", 0);
	return true;
}

LRESULT CodecH265LS::configure(HWND parent)
{
	ConfigH265LS dlg;
	dlg.dialog_id = IDD_ENC_X265LS;
	dlg.Show(parent, this);
	return ICERR_OK;
}
