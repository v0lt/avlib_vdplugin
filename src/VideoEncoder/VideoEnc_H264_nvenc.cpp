/*
 * Copyright (C) 2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "VideoEnc_H264_nvenc.h"
#include "../Helper.h"
#include "../resource.h"
#include "../registry.h"

const int h264_nvenc_formats[] = {
	CodecBase::format_yuv420,
	CodecBase::format_yuv444,
};

const char* h264_nvenc_preset_names[] = {
	"p1",
	"p2",
	"p3",
	"p4",
	"p5",
	"p6",
	"p7",
};

const char* h264_nvenc_tune_names[] = {
	"hq",
	"ll",
	"ull",
	"lossless",
};

//
// ConfigH264_NVENC
//

class ConfigH264_NVENC : public ConfigBase {
public:
	ConfigH264_NVENC() { dialog_id = IDD_ENC_NVENC_H264; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	virtual void init_format();
	virtual void change_format(int sel);
};

INT_PTR ConfigH264_NVENC::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CodecH264_NVENC::Config* config = (CodecH264_NVENC::Config*)codec->config;
	switch (msg) {
	case WM_INITDIALOG:
	{
		SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_RESETCONTENT, 0, 0);
		for (const auto& preset_name : h264_nvenc_preset_names) {
			SendDlgItemMessageA(mhdlg, IDC_ENC_PROFILE, CB_ADDSTRING, 0, (LPARAM)preset_name);
		}
		SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_SETCURSEL, config->preset, 0);

		SendDlgItemMessageW(mhdlg, IDC_ENC_TUNE, CB_RESETCONTENT, 0, 0);
		for (const auto& tune_name : h264_nvenc_tune_names) {
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

void ConfigH264_NVENC::init_format()
{
	SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_RESETCONTENT, 0, 0);
	for (const auto& format : h264_nvenc_formats) {
		SendDlgItemMessageA(mhdlg, IDC_ENC_COLORSPACE, CB_ADDSTRING, 0, (LPARAM)GetFormatName(format));
	}
	int sel = 0; // format_yuv420
	if (codec->config->format == CodecBase::format_yuv444) {
		sel = 1;
	}
	SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_SETCURSEL, sel, 0);
}

void ConfigH264_NVENC::change_format(int sel)
{
	if (sel >= 0 && sel < std::size(h264_nvenc_formats)) {
		codec->config->format = h264_nvenc_formats[sel];
	}
}

//
// CodecH264_NVENC
//

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\VideoEnc_H264_nvenc"

void CodecH264_NVENC::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		reg.ReadInt("format", codec_config.format, h264_nvenc_formats);
		reg.CheckString("preset", codec_config.preset, h264_nvenc_preset_names);
		reg.CheckString("tune", codec_config.tune, h264_nvenc_tune_names);
		reg.CloseKey();
	}
}

void CodecH264_NVENC::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		reg.WriteInt("format", codec_config.format);
		reg.WriteString("preset", h264_nvenc_preset_names[codec_config.preset]);
		reg.WriteString("tune", h264_nvenc_tune_names[codec_config.tune]);
		reg.CloseKey();
	}
}

int CodecH264_NVENC::compress_input_info(VDXPixmapLayout* src)
{
	switch (src->format) {
	case nsVDXPixmap::kPixFormat_YUV420_Planar:
	case nsVDXPixmap::kPixFormat_YUV420_NV12:
	case nsVDXPixmap::kPixFormat_YUV444_Planar:
		return 1;
	}
	return 0;
}

bool CodecH264_NVENC::init_ctx(VDXPixmapLayout* layout)
{
	avctx->gop_size = -1;
	avctx->max_b_frames = -1;

	[[maybe_unused]] int ret = 0;
	ret = av_opt_set(avctx->priv_data, "preset", h264_nvenc_preset_names[codec_config.preset], 0);
	ret = av_opt_set(avctx->priv_data, "tune", h264_nvenc_tune_names[codec_config.tune], 0);
	return true;
}

LRESULT CodecH264_NVENC::configure(HWND parent)
{
	ConfigH264_NVENC dlg;
	dlg.Show(parent, this);
	return ICERR_OK;
}
