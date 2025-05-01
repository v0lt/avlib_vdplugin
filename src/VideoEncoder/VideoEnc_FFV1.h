/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "VideoEnc.h"

int ffv1_slice_tab[] = { 0,4,6,9,12,16,24,30,36,42 };

class ConfigFFV1 : public ConfigBase {
public:
	ConfigFFV1() { dialog_id = IDD_ENC_FFV1; idc_message = IDC_ENC_MESSAGE; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void apply_level();
	void init_slices();
	void init_coder();
	virtual void change_bits();
};

struct CodecFFV1 : public CodecBase {
	enum { id_tag = MKTAG('F', 'F', 'V', '1') };
	struct Config : public CodecBase::Config {
		int level;
		int slice;
		int coder;
		int context;
		int slicecrc;

		Config() { set_default(); }
		void set_default() {
			version = 1;
			format = format_yuv422;
			bits = 10;
			level = 3;
			slice = 0;
			coder = 1;
			context = 0;
			slicecrc = 1;
		}
	} codec_config;

	CodecFFV1() {
		config = &codec_config;
		codec_name = "ffv1";
		codec_tag = MKTAG('F', 'F', 'V', '1');
	}

	int config_size() { return sizeof(Config); }
	void reset_config() { codec_config.set_default(); }

	virtual bool test_av_format(AVPixelFormat format) {
		switch (format) {
		case AV_PIX_FMT_GRAY9:
		case AV_PIX_FMT_YUV444P9:
		case AV_PIX_FMT_YUV422P9:
		case AV_PIX_FMT_YUV420P9:
		case AV_PIX_FMT_YUVA444P9:
		case AV_PIX_FMT_YUVA422P9:
		case AV_PIX_FMT_YUVA420P9:
		case AV_PIX_FMT_GRAY10:
		case AV_PIX_FMT_YUV444P10:
		case AV_PIX_FMT_YUV440P10:
		case AV_PIX_FMT_YUV420P10:
		case AV_PIX_FMT_YUV422P10:
		case AV_PIX_FMT_YUVA444P10:
		case AV_PIX_FMT_YUVA422P10:
		case AV_PIX_FMT_YUVA420P10:
		case AV_PIX_FMT_GRAY12:
		case AV_PIX_FMT_YUV444P12:
		case AV_PIX_FMT_YUV440P12:
		case AV_PIX_FMT_YUV420P12:
		case AV_PIX_FMT_YUV422P12:
		case AV_PIX_FMT_YUV444P14:
		case AV_PIX_FMT_YUV420P14:
		case AV_PIX_FMT_YUV422P14:
		case AV_PIX_FMT_GRAY16:
		case AV_PIX_FMT_YUV444P16:
		case AV_PIX_FMT_YUV422P16:
		case AV_PIX_FMT_YUV420P16:
		case AV_PIX_FMT_YUVA444P16:
		case AV_PIX_FMT_YUVA422P16:
		case AV_PIX_FMT_YUVA420P16:
		case AV_PIX_FMT_RGB48:
		case AV_PIX_FMT_RGBA64:
		case AV_PIX_FMT_GBRP9:
		case AV_PIX_FMT_GBRP10:
		case AV_PIX_FMT_GBRAP10:
		case AV_PIX_FMT_GBRP12:
		case AV_PIX_FMT_GBRAP12:
		case AV_PIX_FMT_GBRP14:
		case AV_PIX_FMT_GBRP16:
		case AV_PIX_FMT_GBRAP16:
			if (codec_config.level < 1) {
				return false;
			}
		}
		return CodecBase::test_av_format(format);
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

	void getinfo(ICINFO& info) {
		info.fccHandler = id_tag;
		info.dwFlags = VIDCF_COMPRESSFRAMES | VIDCF_FASTTEMPORALC;
		wcscpy_s(info.szName, L"FFV1");
		wcscpy_s(info.szDescription, L"FFmpeg / FFV1 lossless codec");
	}

	bool init_ctx(VDXPixmapLayout* layout)
	{
		avctx->strict_std_compliance = -2;
		avctx->level = codec_config.level;
		avctx->slices = codec_config.slice;
		av_opt_set_int(avctx->priv_data, "slicecrc", codec_config.slicecrc, 0);
		av_opt_set_int(avctx->priv_data, "context", codec_config.context, 0);
		av_opt_set_int(avctx->priv_data, "coder", codec_config.coder, 0);
		return true;
	}

	LRESULT configure(HWND parent)
	{
		ConfigFFV1 dlg;
		dlg.Show(parent, this);
		return ICERR_OK;
	}
};

void ConfigFFV1::apply_level()
{
	CodecFFV1::Config* config = (CodecFFV1::Config*)codec->config;

	EnableWindow(GetDlgItem(mhdlg, IDC_ENC_SLICES), config->level >= 3);
	EnableWindow(GetDlgItem(mhdlg, IDC_ENC_SLICECRC), config->level >= 3);
	if (config->level < 3) {
		config->slice = 0;
		config->slicecrc = 0;
	}
	init_slices();
	adjust_bits();
	init_bits();
	change_bits();
}

void ConfigFFV1::init_slices()
{
	CodecFFV1::Config* config = (CodecFFV1::Config*)codec->config;
	size_t x = 0;
	for (size_t i = 1; i < std::size(ffv1_slice_tab); i++) {
		if (ffv1_slice_tab[i] == config->slice) {
			x = i;
		}
	}
	SendDlgItemMessageW(mhdlg, IDC_ENC_SLICES, CB_SETCURSEL, x, 0);
	CheckDlgButton(mhdlg, IDC_ENC_SLICECRC, config->slicecrc == 1 ? BST_CHECKED : BST_UNCHECKED);
}

void ConfigFFV1::init_coder()
{
	CodecFFV1::Config* config = (CodecFFV1::Config*)codec->config;
	CheckDlgButton(mhdlg, IDC_ENC_CODER0, config->coder == 0 ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(mhdlg, IDC_ENC_CODER1, config->coder == 1 ? BST_CHECKED : BST_UNCHECKED);
}

void ConfigFFV1::change_bits()
{
	CodecFFV1::Config* config = (CodecFFV1::Config*)codec->config;
	EnableWindow(GetDlgItem(mhdlg, IDC_ENC_CODER0), config->bits == 8);
	if (config->bits > 8) {
		config->coder = 1;
	}
	init_coder();
	ConfigBase::change_bits();
}

INT_PTR ConfigFFV1::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CodecFFV1::Config* config = (CodecFFV1::Config*)codec->config;

	switch (msg) {
	case WM_INITDIALOG:
	{
		const char* v_names[] = {
			"FFV1.0",
			"FFV1.1",
			"FFV1.3",
		};
		SendDlgItemMessageW(mhdlg, IDC_LEVEL, CB_RESETCONTENT, 0, 0);
		for (const auto& v_name : v_names) {
			SendDlgItemMessageA(mhdlg, IDC_LEVEL, CB_ADDSTRING, 0, (LPARAM)v_name);
		}
		int x = 0;
		if (config->level == 1) {
			x = 1;
		}
		else if (config->level == 3) {
			x = 2;
		}
		SendDlgItemMessageW(mhdlg, IDC_LEVEL, CB_SETCURSEL, x, 0);

		SendDlgItemMessageW(mhdlg, IDC_ENC_SLICES, CB_RESETCONTENT, 0, 0);
		SendDlgItemMessageW(mhdlg, IDC_ENC_SLICES, CB_ADDSTRING, 0, (LPARAM)L"default");
		for (const auto& slice_tab : ffv1_slice_tab) {
			auto str = std::to_wstring(slice_tab);
			SendDlgItemMessageW(mhdlg, IDC_ENC_SLICES, CB_ADDSTRING, 0, (LPARAM)str.c_str());
		}
		CheckDlgButton(mhdlg, IDC_ENC_CONTEXT, config->context == 1 ? BST_CHECKED : BST_UNCHECKED);
		apply_level();
		break;
	}

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_LEVEL:
			if (HIWORD(wParam) == LBN_SELCHANGE) {
				int x = (int)SendDlgItemMessageW(mhdlg, IDC_LEVEL, CB_GETCURSEL, 0, 0);
				config->level = 0;
				if (x == 1) {
					config->level = 1;
				}
				else if (x == 2) {
					config->level = 3;
				}
				apply_level();
				return TRUE;
			}
			break;
		case IDC_ENC_SLICES:
			if (HIWORD(wParam) == LBN_SELCHANGE) {
				int x = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_SLICES, CB_GETCURSEL, 0, 0);
				config->slice = ffv1_slice_tab[x];
				return TRUE;
			}
			break;
		case IDC_ENC_SLICECRC:
			config->slicecrc = IsDlgButtonChecked(mhdlg, IDC_ENC_SLICECRC) ? 1 : 0;
			break;
		case IDC_ENC_CONTEXT:
			config->context = IsDlgButtonChecked(mhdlg, IDC_ENC_CONTEXT) ? 1 : 0;
			break;
		case IDC_ENC_CODER0:
		case IDC_ENC_CODER1:
			config->coder = IsDlgButtonChecked(mhdlg, IDC_ENC_CODER0) ? 0 : 1;
			break;
		}
	}
	return ConfigBase::DlgProc(msg, wParam, lParam);
}
