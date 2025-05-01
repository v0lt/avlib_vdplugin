/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "VideoEnc.h"

class ConfigHUFF : public ConfigBase {
public:
	ConfigHUFF() { dialog_id = IDD_ENC_FFVHUFF; idc_message = IDC_ENC_MESSAGE; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
};

struct CodecHUFF : public CodecBase {
	enum { id_tag = MKTAG('F', 'F', 'V', 'H') };
	struct Config : public CodecBase::Config {
		int prediction = 0;

		void reset() {
			version = 1;
			format = format_rgb;
			bits = 8;
			prediction = 0;
		}
	} codec_config;

	CodecHUFF() {
		codec_name = "ffvhuff";
		codec_tag = MKTAG('F', 'F', 'V', 'H');
		config = &codec_config;
		load_config();
	}

	int config_size() override { return sizeof(Config); }
	void reset_config() override { codec_config.reset(); }

	virtual void load_config() override {

	}
	virtual void save_config() override {

	}

	void getinfo(ICINFO& info) {
		info.fccHandler = id_tag;
		info.dwFlags = VIDCF_COMPRESSFRAMES;
		wcscpy_s(info.szName, L"FFVHUFF");
		wcscpy_s(info.szDescription, L"FFmpeg / HuffyuvFF lossless codec");
	}

	virtual int compress_input_info(VDXPixmapLayout* src) {
		switch (src->format) {
		case nsVDXPixmap::kPixFormat_RGB888:
		case nsVDXPixmap::kPixFormat_XRGB8888:
		case nsVDXPixmap::kPixFormat_XRGB64:
		case nsVDXPixmap::kPixFormat_YUV420_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_Planar:
		case nsVDXPixmap::kPixFormat_YUV444_Planar:
		case nsVDXPixmap::kPixFormat_YUV420_Planar16:
		case nsVDXPixmap::kPixFormat_YUV422_Planar16:
		case nsVDXPixmap::kPixFormat_YUV444_Planar16:
		case nsVDXPixmap::kPixFormat_YUV420_Alpha_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_Alpha_Planar:
		case nsVDXPixmap::kPixFormat_YUV444_Alpha_Planar:
		case nsVDXPixmap::kPixFormat_YUV420_Alpha_Planar16:
		case nsVDXPixmap::kPixFormat_YUV422_Alpha_Planar16:
		case nsVDXPixmap::kPixFormat_YUV444_Alpha_Planar16:
		case nsVDXPixmap::kPixFormat_Y8:
		case nsVDXPixmap::kPixFormat_Y16:
			return 1;
		}
		return 0;
	}

	bool init_ctx(VDXPixmapLayout* layout)
	{
		int pred = codec_config.prediction;
		if (pred == 2 && config->format == format_rgb && config->bits == 8) {
			pred = 0;
		}
		av_opt_set_int(avctx->priv_data, "pred", pred, 0);
		return true;
	}

	LRESULT configure(HWND parent)
	{
		ConfigHUFF dlg;
		dlg.Show(parent, this);
		return ICERR_OK;
	}
};

INT_PTR ConfigHUFF::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
	{
		const char* pred_names[] = {
			"left",
			"plane",
			"median",
		};

		SendDlgItemMessageW(mhdlg, IDC_PREDICTION, CB_RESETCONTENT, 0, 0);
		for (const auto& pred_name : pred_names) {
			SendDlgItemMessageA(mhdlg, IDC_PREDICTION, CB_ADDSTRING, 0, (LPARAM)pred_name);
		}
		CodecHUFF::Config* config = (CodecHUFF::Config*)codec->config;
		SendDlgItemMessageW(mhdlg, IDC_PREDICTION, CB_SETCURSEL, config->prediction, 0);
		break;
	}

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_PREDICTION:
			if (HIWORD(wParam) == LBN_SELCHANGE) {
				CodecHUFF::Config* config = (CodecHUFF::Config*)codec->config;
				config->prediction = (int)SendDlgItemMessageW(mhdlg, IDC_PREDICTION, CB_GETCURSEL, 0, 0);
				return TRUE;
			}
			break;
		}
	}
	return ConfigBase::DlgProc(msg, wParam, lParam);
}
