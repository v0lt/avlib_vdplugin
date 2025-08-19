/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stdafx.h"

#include "VideoEnc_VP9.h"
#include "../resource.h"

//
// ConfigVP9
//

class ConfigVP9 : public ConfigBase {
public:
	ConfigVP9() { dialog_id = IDD_ENC_VP9; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override;
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
		SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, config->crf, FALSE);
		break;
	}

	case WM_HSCROLL:
		if ((HWND)lParam == GetDlgItem(mhdlg, IDC_ENC_QUALITY)) {
			config->crf = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_GETPOS, 0, 0);
			SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, config->crf, FALSE);
			break;
		}
		return FALSE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_BUTTON_DEFAULT:
			codec->reset_config();
			init_format();
			init_bits();
			SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETPOS, TRUE, config->crf);
			SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, config->crf, FALSE);
			break;
		}
	}
	return ConfigBase::DlgProc(msg, wParam, lParam);
}

//
// CodecVP9
//

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\VideoEnc_VP9"

void CodecVP9::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		load_format_bitdepth(reg);
		reg.ReadInt("crf", codec_config.crf, 0, 63);
		reg.CloseKey();
	}
}

void CodecVP9::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		save_format_bitdepth(reg);
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
