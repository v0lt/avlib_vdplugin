/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "VideoEnc_VP8.h"
#include "../resource.h"
#include "../registry.h"

const int vp8_formats[] = {
	CodecBase::format_yuv420,
	CodecBase::format_yuva420,
};

//
// ConfigVP8
//

class ConfigVP8 : public ConfigBase {
public:
	ConfigVP8() { dialog_id = IDD_ENC_VP8; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	virtual void init_format();
	virtual void change_format(int sel);
};

INT_PTR ConfigVP8::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CodecVP8::Config* config = (CodecVP8::Config*)codec->config;
	switch (msg) {
	case WM_INITDIALOG:
	{
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMIN, FALSE, 4);
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMAX, TRUE, 63);
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
	}
	return ConfigBase::DlgProc(msg, wParam, lParam);
}

void ConfigVP8::init_format()
{
	const char* color_names[] = {
		"YUV 4:2:0",
		"YUV 4:2:0 + Alpha",
	};

	SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_RESETCONTENT, 0, 0);
	for (const auto& color_name : color_names) {
		SendDlgItemMessageA(mhdlg, IDC_ENC_COLORSPACE, CB_ADDSTRING, 0, (LPARAM)color_name);
	}
	int sel = 0; // format_yuv420
	if (codec->config->format == CodecBase::format_yuva420) {
		sel = 1;
	}
	SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_SETCURSEL, sel, 0);
	EnableWindow(GetDlgItem(mhdlg, IDC_ENC_COLORSPACE), false); //! need to pass side_data
}

void ConfigVP8::change_format(int sel)
{
	if (sel >= 0 && sel < std::size(vp8_formats)) {
		codec->config->format = vp8_formats[sel];
	}
}

//
// CodecVP8
//

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\VideoEnc_VP8"

void CodecVP8::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		reg.ReadInt("crf", codec_config.crf, 4, 63);
		reg.CloseKey();
	}
}

void CodecVP8::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		reg.WriteInt("crf", codec_config.crf);
		reg.CloseKey();
	}
}

bool CodecVP8::init_ctx(VDXPixmapLayout* layout)
{
	avctx->gop_size = -1;
	avctx->max_b_frames = -1;
	avctx->bit_rate = 0x400000000000;

	[[maybe_unused]] int ret = 0;
	ret = av_opt_set_int(avctx->priv_data, "crf", codec_config.crf, 0);
	ret = av_opt_set_int(avctx->priv_data, "max-intra-rate", 0, 0);
	if (codec_config.format == format_yuva420) {
		av_opt_set_int(avctx->priv_data, "auto-alt-ref", 0, 0);
	}
	avctx->qmin = codec_config.crf;
	avctx->qmax = codec_config.crf;
	return true;
}

LRESULT CodecVP8::configure(HWND parent)
{
	ConfigVP8 dlg;
	dlg.Show(parent, this);
	return ICERR_OK;
}
