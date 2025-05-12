/*
 * Copyright (C) 2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "VideoEnc_QSV_HEVC.h"
#include "../Helper.h"
#include "../resource.h"
#include "../registry.h"

const int hevc_qsv_formats[] = {
	CodecBase::format_yuv420,
};

const int hevc_qsv_bitdepths[] = { 8, 10 };

const char* hevc_qsv_preset_names[] = {
	"veryfast",
	"faster",
	"fast",
	"medium",
	"slow",
	"slower",
	"veryslow",
};

//
// ConfigQSV_HEVC
//

class ConfigQSV_HEVC : public ConfigBase {
public:
	ConfigQSV_HEVC() { dialog_id = IDD_ENC_QSV_HEVC; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	virtual void init_format();
	virtual void change_format(int sel);
};

INT_PTR ConfigQSV_HEVC::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CodecQSV_HEVC::Config* config = (CodecQSV_HEVC::Config*)codec->config;
	switch (msg) {
	case WM_INITDIALOG:
	{
		SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_RESETCONTENT, 0, 0);
		for (const auto& preset_name : hevc_qsv_preset_names) {
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

void ConfigQSV_HEVC::init_format()
{
	SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_RESETCONTENT, 0, 0);
	for (const auto& format : hevc_qsv_formats) {
		SendDlgItemMessageA(mhdlg, IDC_ENC_COLORSPACE, CB_ADDSTRING, 0, (LPARAM)GetFormatName(format));
	}
	int sel = 0; // format_yuv420
	SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_SETCURSEL, sel, 0);
}

void ConfigQSV_HEVC::change_format(int sel)
{
	if (sel >= 0 && sel < std::size(hevc_qsv_formats)) {
		codec->config->format = hevc_qsv_formats[sel];
		init_bits();
	}
}

//
// CodecQSV_HEVC
//

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\VideoEnc_QSV_HEVC"

void CodecQSV_HEVC::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		reg.ReadInt("format", codec_config.format, hevc_qsv_formats);
		reg.ReadInt("bitdepth", codec_config.bits, hevc_qsv_bitdepths);
		reg.CheckString("preset", codec_config.preset, hevc_qsv_preset_names);
		reg.CloseKey();
	}
}

void CodecQSV_HEVC::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		reg.WriteInt("format", codec_config.format);
		reg.WriteInt("bitdepth", codec_config.bits);
		reg.WriteString("preset", hevc_qsv_preset_names[codec_config.preset]);
		reg.CloseKey();
	}
}

bool CodecQSV_HEVC::test_bits(int format, int bits)
{
	switch (format) {
	case format_yuv420:
		if (bits == 8)  return test_av_format(AV_PIX_FMT_NV12);
		if (bits == 10) return test_av_format(AV_PIX_FMT_P010);
		break;
	}
	return false;
}

int CodecQSV_HEVC::compress_input_info(VDXPixmapLayout* src)
{
	switch (src->format) {
	case nsVDXPixmap::kPixFormat_YUV420_NV12:
	case nsVDXPixmap::kPixFormat_YUV420_P010:
		return 1;
	}
	return 0;
}

LRESULT CodecQSV_HEVC::compress_input_format(FilterModPixmapInfo* info)
{
	if (config->format == format_yuv420) {
		if (config->bits == 8) {
			return nsVDXPixmap::kPixFormat_YUV420_NV12;
		}
		if (config->bits == 10) {
			if (info) {
				info->ref_r = 0xFFC0;
			}
			return nsVDXPixmap::kPixFormat_YUV420_P010;
		}
	}
	return 0;
}

bool CodecQSV_HEVC::init_ctx(VDXPixmapLayout* layout)
{
	avctx->gop_size = -1;
	avctx->max_b_frames = -1;

	[[maybe_unused]] int ret = 0;
	ret = av_opt_set(avctx->priv_data, "preset", hevc_qsv_preset_names[codec_config.preset], 0);
	return true;
}

LRESULT CodecQSV_HEVC::configure(HWND parent)
{
	ConfigQSV_HEVC dlg;
	dlg.Show(parent, this);
	return ICERR_OK;
}
