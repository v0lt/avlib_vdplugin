/*
 * Copyright (C) 2025-2026 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stdafx.h"

#include "VideoEnc_QSV_H264.h"
#include "../Helper.h"
#include "../resource.h"

const char* h264_qsv_preset_names[] = {
	"veryfast",
	"faster",
	"fast",
	"medium",
	"slow",
	"slower",
	"veryslow",
};

//
// ConfigQSV_H264
//

class ConfigQSV_H264 : public ConfigBase {
public:
	ConfigQSV_H264() { dialog_id = IDD_ENC_QSV_H264; }
	void ChangeRateControl(CodecQSV_H264::Config* config);
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override;
};

void ConfigQSV_H264::ChangeRateControl(CodecQSV_H264::Config* pConfig)
{
	if (pConfig->rc == CODEC_RC_VBR) {
		SetBitrate(pConfig->bitrate);
	} else {
		SetQuality(pConfig->qscale);
	}
}

INT_PTR ConfigQSV_H264::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CodecQSV_H264::Config* config = (CodecQSV_H264::Config*)codec->config;
	switch (msg) {
	case WM_INITDIALOG:
	{
		SendDlgItemMessageW(mhdlg, IDC_ENC_PRESET, CB_RESETCONTENT, 0, 0);
		for (const auto& preset_name : h264_qsv_preset_names) {
			SendDlgItemMessageA(mhdlg, IDC_ENC_PRESET, CB_ADDSTRING, 0, (LPARAM)preset_name);
		}
		SendDlgItemMessageW(mhdlg, IDC_ENC_PRESET, CB_SETCURSEL, config->preset, 0);

		SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_ADDSTRING, 0, (LPARAM)L"Constant QP");
		SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_ADDSTRING, 0, (LPARAM)L"Variable bitrate (VBR)");
		SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_SETCURSEL, (WPARAM)config->rc, 0);

		ChangeRateControl(config);
		break;
	}

	case WM_HSCROLL:
		if ((HWND)lParam == GetDlgItem(mhdlg, IDC_ENC_RATECONTROL_SLIDER)) {
			int value = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL_SLIDER, TBM_GETPOS, 0, 0);
			if (config->rc == CODEC_RC_VBR) {
				config->bitrate = pos2scale(value);
				SetDlgItemInt(mhdlg, IDC_ENC_RATECONTROL_VALUE, config->bitrate, FALSE);
			} else {
				config->qscale = value;
				SetDlgItemInt(mhdlg, IDC_ENC_RATECONTROL_VALUE, config->qscale, FALSE);
			}
			break;
		}
		return FALSE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_BUTTON_DEFAULT:
			codec->reset_config();
			init_format();
			init_bits();
			SendDlgItemMessageW(mhdlg, IDC_ENC_PRESET, CB_SETCURSEL, config->preset, 0);
			SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_SETCURSEL, config->rc, 0);
			ChangeRateControl(config);
			break;
		case IDC_ENC_PRESET:
			if (HIWORD(wParam) == LBN_SELCHANGE) {
				config->preset = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_PRESET, CB_GETCURSEL, 0, 0);
				return TRUE;
			}
			break;
		case IDC_ENC_RATECONTROL:
			if (HIWORD(wParam) == LBN_SELCHANGE) {
				config->rc = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_GETCURSEL, 0, 0);
				ChangeRateControl(config);
				return TRUE;
			}
			break;
		}
	}
	return ConfigBase::DlgProc(msg, wParam, lParam);
}

//
// CodecQSV_H264
//

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\VideoEnc_QSV_H264"

void CodecQSV_H264::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		load_format_bitdepth(reg);
		reg.CheckString("preset", codec_config.preset, h264_qsv_preset_names);
		reg.ReadInt("rate_control", codec_config.rc, 0, 1);
		reg.ReadInt("qscale", codec_config.qscale, 0, 51);
		reg.ReadInt("bitrate", codec_config.bitrate, MIN_VIDEO_BITRATE, MAX_VIDEO_BITRATE);
		reg.CloseKey();
	}
}

void CodecQSV_H264::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		save_format_bitdepth(reg);
		reg.WriteString("preset", h264_qsv_preset_names[codec_config.preset]);
		reg.WriteInt("rate_control", codec_config.rc);
		reg.WriteInt("qscale", codec_config.qscale);
		reg.WriteInt("bitrate", codec_config.bitrate);
		reg.CloseKey();
	}
}

bool CodecQSV_H264::test_bits(int format, int bits)
{
	if (format == format_yuv420 && bits == 8) {
		return test_av_format(AV_PIX_FMT_NV12);
	}
	return false;
}


int CodecQSV_H264::compress_input_info(VDXPixmapLayout* src)
{
	if (src->format == nsVDXPixmap::kPixFormat_YUV420_NV12) {
		return 1;
	}
	return 0;
}

int CodecQSV_H264::compress_input_format(FilterModPixmapInfo* info)
{
	if (config->format == format_yuv420) {
		if (config->bits == 8) {
			return nsVDXPixmap::kPixFormat_YUV420_NV12;
		}
	}
	return 0;
}

bool CodecQSV_H264::init_ctx(VDXPixmapLayout* layout)
{
	avctx->gop_size = -1;
	avctx->max_b_frames = -1;

	[[maybe_unused]] int ret = 0;
	ret = av_opt_set(avctx->priv_data, "preset", h264_qsv_preset_names[codec_config.preset], 0);
	if (codec_config.rc == CODEC_RC_VBR) {
		avctx->bit_rate = codec_config.bitrate * 1000;
	} else {
		avctx->flags |= AV_CODEC_FLAG_QSCALE;
		avctx->global_quality = FF_QP2LAMBDA * codec_config.qscale;
	}

	return true;
}

LRESULT CodecQSV_H264::configure(HWND parent)
{
	ConfigQSV_H264 dlg;
	dlg.Show(parent, this);
	return ICERR_OK;
}
