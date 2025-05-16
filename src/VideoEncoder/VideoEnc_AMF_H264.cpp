/*
 * Copyright (C) 2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "VideoEnc_AMF_H264.h"
#include "../Helper.h"
#include "../resource.h"
#include "../registry.h"

const char* h264_amf_preset_names[] = {
	"default",
	"speed",
	"balanced",
	"quality",
};

//
// ConfigAMF_H264
//

class ConfigAMF_H264 : public ConfigBase {
public:
	ConfigAMF_H264() { dialog_id = IDD_ENC_AMF_H264; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	virtual void init_format();
	virtual void change_format(int sel);
};

INT_PTR ConfigAMF_H264::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CodecAMF_H264::Config* config = (CodecAMF_H264::Config*)codec->config;
	switch (msg) {
	case WM_INITDIALOG:
	{
		SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_RESETCONTENT, 0, 0);
		for (const auto& preset_name : h264_amf_preset_names) {
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

void ConfigAMF_H264::init_format()
{
	SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessageA(mhdlg, IDC_ENC_COLORSPACE, CB_ADDSTRING, 0, (LPARAM)GetFormatName(CodecBase::format_yuv420));
	int sel = 0; // format_yuv420
	SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_SETCURSEL, sel, 0);
}

void ConfigAMF_H264::change_format(int sel)
{
	codec->config->format = CodecBase::format_yuv420;
}

//
// CodecAMF_H264
//

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\VideoEnc_AMF_H264"

void CodecAMF_H264::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		reg.CheckString("preset", codec_config.preset, h264_amf_preset_names);
		reg.CloseKey();
	}
}

void CodecAMF_H264::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		reg.WriteString("preset", h264_amf_preset_names[codec_config.preset]);
		reg.CloseKey();
	}
}

bool CodecAMF_H264::test_bits(int format, int bits)
{
	if (format == format_yuv420 && bits == 8) {
		return test_av_format(AV_PIX_FMT_YUV420P);
	}
	return false;
}


int CodecAMF_H264::compress_input_info(VDXPixmapLayout* src)
{
	switch (src->format) {
	case nsVDXPixmap::kPixFormat_YUV420_Planar:
	case nsVDXPixmap::kPixFormat_YUV420_NV12: // also an acceptable format
		return 1;
	}
	return 0;
}

int CodecAMF_H264::compress_input_format(FilterModPixmapInfo* info)
{
	if (config->format == format_yuv420) {
		if (config->bits == 8) {
			return nsVDXPixmap::kPixFormat_YUV420_Planar;
		}
	}
	return 0;
}

bool CodecAMF_H264::init_ctx(VDXPixmapLayout* layout)
{
	avctx->gop_size = -1;
	avctx->max_b_frames = -1;

	[[maybe_unused]] int ret = 0;
	ret = av_opt_set(avctx->priv_data, "preset", h264_amf_preset_names[codec_config.preset], 0);
	return true;
}

LRESULT CodecAMF_H264::configure(HWND parent)
{
	ConfigAMF_H264 dlg;
	dlg.Show(parent, this);
	return ICERR_OK;
}
