/*
 * Copyright (C) 2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "VideoEnc_H265_nvenc.h"
#include "../Helper.h"
#include "../resource.h"
#include "../registry.h"

const int hevc_nvenc_formats[] = {
	CodecBase::format_yuv420,
	CodecBase::format_yuv444,
};

const int hevc_nvenc_bitdepths[] = { 8, 10 };

const char* hevc_nvenc_preset_names[] = {
	"p1",
	"p2",
	"p3",
	"p4",
	"p5",
	"p6",
	"p7",
};

const char* hevc_nvenc_tune_names[] = {
	"hq",
	"uhq",
	"ll",
	"ull",
	"lossless",
};

//
// ConfigH265_NVENC
//

class ConfigH265_NVENC : public ConfigBase {
public:
	ConfigH265_NVENC() { dialog_id = IDD_ENC_NVENC_HEVC; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	virtual void init_format();
	virtual void change_format(int sel);
};

INT_PTR ConfigH265_NVENC::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CodecH265_NVENC::Config* config = (CodecH265_NVENC::Config*)codec->config;
	switch (msg) {
	case WM_INITDIALOG:
	{
		SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_RESETCONTENT, 0, 0);
		for (const auto& preset_name : hevc_nvenc_preset_names) {
			SendDlgItemMessageA(mhdlg, IDC_ENC_PROFILE, CB_ADDSTRING, 0, (LPARAM)preset_name);
		}
		SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_SETCURSEL, config->preset, 0);

		SendDlgItemMessageW(mhdlg, IDC_ENC_TUNE, CB_RESETCONTENT, 0, 0);
		for (const auto& tune_name : hevc_nvenc_tune_names) {
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

void ConfigH265_NVENC::init_format()
{
	SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_RESETCONTENT, 0, 0);
	for (const auto& format : hevc_nvenc_formats) {
		SendDlgItemMessageA(mhdlg, IDC_ENC_COLORSPACE, CB_ADDSTRING, 0, (LPARAM)GetFormatName(format));
	}
	int sel = 0; // format_yuv420
	if (codec->config->format == CodecBase::format_yuv444) {
		sel = 1;
	}
	SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_SETCURSEL, sel, 0);
}

void ConfigH265_NVENC::change_format(int sel)
{
	if (sel >= 0 && sel < std::size(hevc_nvenc_formats)) {
		codec->config->format = hevc_nvenc_formats[sel];
		init_bits();
	}
}

//
// CodecH265_NVENC
//

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\VideoEnc_H265_nvenc"

void CodecH265_NVENC::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		reg.ReadInt("format", codec_config.format, hevc_nvenc_formats);
		reg.ReadInt("bitdepth", codec_config.bits, hevc_nvenc_bitdepths);
		reg.CheckString("preset", codec_config.preset, hevc_nvenc_preset_names);
		reg.CheckString("tune", codec_config.tune, hevc_nvenc_tune_names);
		reg.CloseKey();
	}
}

void CodecH265_NVENC::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		reg.WriteInt("format", codec_config.format);
		reg.WriteInt("bitdepth", codec_config.bits);
		reg.WriteString("preset", hevc_nvenc_preset_names[codec_config.preset]);
		reg.WriteString("tune", hevc_nvenc_tune_names[codec_config.tune]);
		reg.CloseKey();
	}
}

int CodecH265_NVENC::compress_input_info(VDXPixmapLayout* src)
{
	switch (src->format) {
	case nsVDXPixmap::kPixFormat_YUV420_Planar:
	case nsVDXPixmap::kPixFormat_YUV420_NV12:
	case nsVDXPixmap::kPixFormat_YUV420_P010:
	case nsVDXPixmap::kPixFormat_YUV420_P016:
	case nsVDXPixmap::kPixFormat_YUV444_Planar:
	case nsVDXPixmap::kPixFormat_YUV444_Planar16:
		return 1;
	}
	return 0;
}

bool CodecH265_NVENC::init_ctx(VDXPixmapLayout* layout)
{
	avctx->gop_size = -1;
	avctx->max_b_frames = -1;

	[[maybe_unused]] int ret = 0;
	ret = av_opt_set(avctx->priv_data, "preset", hevc_nvenc_preset_names[codec_config.preset], 0);
	ret = av_opt_set(avctx->priv_data, "tune", hevc_nvenc_tune_names[codec_config.tune], 0);
	return true;
}

LRESULT CodecH265_NVENC::configure(HWND parent)
{
	ConfigH265_NVENC dlg;
	dlg.Show(parent, this);
	return ICERR_OK;
}
