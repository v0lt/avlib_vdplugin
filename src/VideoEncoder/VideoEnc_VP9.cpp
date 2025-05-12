/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "VideoEnc_VP9.h"
#include "../resource.h"
#include "../registry.h"

const int vp9_formats[] = {
	CodecBase::format_rgb,
	CodecBase::format_yuv420,
	CodecBase::format_yuv422,
	CodecBase::format_yuv444,
};

const int vp9_bitdepths[] = { 8, 10, 12 };

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
	SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_RESETCONTENT, 0, 0);
	for (const auto& format : vp9_formats) {
		LRESULT idx = SendDlgItemMessageA(mhdlg, IDC_ENC_COLORSPACE, CB_ADDSTRING, 0, (LPARAM)GetFormatName(format));
		if (idx >= 0 && format == codec->config->format) {
			SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_SETCURSEL, idx, 0);
		}
	}
}

void ConfigVP9::change_format(int sel)
{
	if (sel >= 0 && sel < std::size(vp9_formats)) {
		codec->config->format = vp9_formats[sel];
		init_bits();
	}
}

//
// CodecVP9
//

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\VideoEnc_VP9"

void CodecVP9::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		reg.ReadInt("format", codec_config.format, vp9_formats);
		reg.ReadInt("bitdepth", codec_config.bits, vp9_bitdepths);
		reg.ReadInt("crf", codec_config.crf, 0, 63);
		reg.CloseKey();
	}
}

void CodecVP9::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		reg.WriteInt("format", codec_config.format);
		reg.WriteInt("bitdepth", codec_config.bits);
		reg.WriteInt("crf", codec_config.crf);
		reg.CloseKey();
	}
}

int CodecVP9::compress_input_info(VDXPixmapLayout* src)
{
	switch (src->format) {
	case nsVDXPixmap::kPixFormat_YUV420_Planar:
	case nsVDXPixmap::kPixFormat_YUV422_Planar:
	case nsVDXPixmap::kPixFormat_YUV444_Planar:
	case nsVDXPixmap::kPixFormat_YUV420_Planar16:
	case nsVDXPixmap::kPixFormat_YUV422_Planar16:
	case nsVDXPixmap::kPixFormat_YUV444_Planar16:
	case nsVDXPixmap::kPixFormat_XRGB8888:
	case nsVDXPixmap::kPixFormat_XRGB64:
		return 1;
	}
	return 0;
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
