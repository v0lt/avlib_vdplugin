/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "VideoEnc_VP9.h"
#include "../resource.h"
#include "../registry.h"

//
// ConfigVP9
//

class ConfigVP9 : public ConfigBase {
public:
	ConfigVP9() { dialog_id = IDD_ENC_VP9; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	virtual void init_format();
	virtual void change_format(int sel);
};

INT_PTR ConfigVP9::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CodecVP9::Config* config = (CodecVP9::Config*)codec->config;
	switch (msg) {
	case WM_INITDIALOG:
	{
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMIN, FALSE, 0);
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

void ConfigVP9::init_format()
{
	const char* color_names[] = {
		"RGB",
		"YUV 4:2:0",
		"YUV 4:2:2",
		"YUV 4:4:4",
	};

	SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_RESETCONTENT, 0, 0);
	for (const auto& color_name : color_names) {
		SendDlgItemMessageA(mhdlg, IDC_ENC_COLORSPACE, CB_ADDSTRING, 0, (LPARAM)color_name);
	}
	int sel = 0; // format_rgb
	if (codec->config->format == CodecBase::format_yuv420) {
		sel = 1;
	}
	else if (codec->config->format == CodecBase::format_yuv422) {
		sel = 2;
	}
	else if (codec->config->format == CodecBase::format_yuv444) {
		sel = 3;
	}
	SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_SETCURSEL, sel, 0);
}

void ConfigVP9::change_format(int sel)
{
	int format = CodecBase::format_rgb;
	if (sel == 1) {
		format = CodecBase::format_yuv420;
	}
	else if (sel == 2) {
		format = CodecBase::format_yuv422;
	}
	else if (sel == 3) {
		format = CodecBase::format_yuv444;
	}
	codec->config->format = format;
	init_bits();
}

//
// CodecVP9
//

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\VideoEnc_VP9"

void CodecVP9::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {

		reg.CloseKey();
	}
}

void CodecVP9::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {

		reg.CloseKey();
	}
}

bool CodecVP9::init_ctx(VDXPixmapLayout* layout)
{
	avctx->gop_size = -1;
	avctx->max_b_frames = -1;

	[[maybe_unused]] int ret = 0;
	ret = av_opt_set_int(avctx->priv_data, "crf", codec_config.crf, 0);
	ret = av_opt_set_int(avctx->priv_data, "max-intra-rate", 0, 0);
	avctx->qmin = codec_config.crf;
	avctx->qmax = codec_config.crf;
	return true;
}

LRESULT CodecVP9::configure(HWND parent)
{
	ConfigVP9 dlg;
	dlg.Show(parent, this);
	return ICERR_OK;
}
