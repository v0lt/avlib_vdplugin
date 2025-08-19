/*
 * Copyright (C) 2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stdafx.h"

#include "VideoEnc_QSV_HEVC.h"
#include "../Helper.h"
#include "../resource.h"

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
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override;
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

		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMIN, FALSE, 0);
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMAX, TRUE, 51);
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETPOS, TRUE, config->qscale);
		SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, config->qscale, FALSE);
		break;
	}

	case WM_HSCROLL:
		if ((HWND)lParam == GetDlgItem(mhdlg, IDC_ENC_QUALITY)) {
			config->qscale = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_GETPOS, 0, 0);
			SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, config->qscale, FALSE);
			break;
		}
		return FALSE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_BUTTON_DEFAULT:
			codec->reset_config();
			init_format();
			init_bits();
			SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_SETCURSEL, config->preset, 0);
			SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETPOS, TRUE, config->qscale);
			SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, config->qscale, FALSE);
			break;
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
// CodecQSV_HEVC
//

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\VideoEnc_QSV_HEVC"

void CodecQSV_HEVC::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		load_format_bitdepth(reg);
		reg.CheckString("preset", codec_config.preset, hevc_qsv_preset_names);
		reg.ReadInt("qscale", codec_config.qscale, 0, 51);
		reg.CloseKey();
	}
}

void CodecQSV_HEVC::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		save_format_bitdepth(reg);
		reg.WriteString("preset", hevc_qsv_preset_names[codec_config.preset]);
		reg.WriteInt("qscale", codec_config.qscale);
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

int CodecQSV_HEVC::compress_input_format(FilterModPixmapInfo* info)
{
	if (config->format == format_yuv420) {
		if (config->bits == 8) {
			return nsVDXPixmap::kPixFormat_YUV420_NV12;
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

bool CodecQSV_HEVC::init_ctx(VDXPixmapLayout* layout)
{
	avctx->gop_size = -1;
	avctx->max_b_frames = -1;

	[[maybe_unused]] int ret = 0;
	ret = av_opt_set(avctx->priv_data, "preset", hevc_qsv_preset_names[codec_config.preset], 0);
	avctx->flags |= AV_CODEC_FLAG_QSCALE;
	avctx->global_quality = FF_QP2LAMBDA * codec_config.qscale;
	return true;
}

LRESULT CodecQSV_HEVC::configure(HWND parent)
{
	ConfigQSV_HEVC dlg;
	dlg.Show(parent, this);
	return ICERR_OK;
}
