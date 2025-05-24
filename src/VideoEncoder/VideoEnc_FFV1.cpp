/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stdafx.h"

#include "VideoEnc_FFV1.h"
#include "../Helper.h"
#include "../resource.h"

const int ffv1_levels[] = { 0, 1, 3 };
const int ffv1_slice_tab[] = { 0, 4, 6, 9, 12, 16, 24, 30, 36, 42 };

//
// ConfigFFV1
//

class ConfigFFV1 : public ConfigBase {
public:
	ConfigFFV1() { dialog_id = IDD_ENC_FFV1; idc_message = IDC_ENC_MESSAGE; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override;
	void change_format(int sel) override;
	void apply_level();
	void init_slices();
	void init_coder();
	void change_bits() override;
};

void ConfigFFV1::change_format(int sel)
{
	if (sel >= 0 && sel < (int)std::size(codec->formats)) {
		codec->config->format = codec->formats[sel];
		adjust_bits();
		init_bits();
		change_bits();
	}
}

void ConfigFFV1::apply_level()
{
	CodecFFV1::Config* config = (CodecFFV1::Config*)codec->config;

	EnableWindow(GetDlgItem(mhdlg, IDC_ENC_SLICES), config->level >= 3);
	EnableWindow(GetDlgItem(mhdlg, IDC_ENC_SLICECRC), config->level >= 3);
	if (config->level < 3) {
		config->slices = 0;
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
		if (ffv1_slice_tab[i] == config->slices) {
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
				config->slices = ffv1_slice_tab[x];
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

//
// CodecFFV1
//

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\VideoEnc_FFV1"

void CodecFFV1::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		load_format_bitdepth(reg);
		reg.ReadInt("level", codec_config.level, ffv1_levels);
		reg.ReadInt("slices", codec_config.slices, ffv1_slice_tab);
		reg.ReadInt("coder", codec_config.coder, 0, 1);
		reg.ReadInt("context", codec_config.context, 0, 1);
		reg.ReadInt("slicecrc", codec_config.slicecrc, 0, 1);
		reg.CloseKey();
	}
}

void CodecFFV1::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		save_format_bitdepth(reg);
		reg.WriteInt("level", codec_config.level);
		reg.WriteInt("slices", codec_config.slices);
		reg.WriteInt("coder", codec_config.coder);
		reg.WriteInt("context", codec_config.context);
		reg.WriteInt("slicecrc", codec_config.slicecrc);
		reg.CloseKey();
	}
}

int CodecFFV1::compress_input_info(VDXPixmapLayout* src)
{
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

bool CodecFFV1::init_ctx(VDXPixmapLayout* layout)
{
	avctx->strict_std_compliance = -2;
	avctx->level = codec_config.level;
	avctx->slices = codec_config.slices;
	av_opt_set_int(avctx->priv_data, "slicecrc", codec_config.slicecrc, 0);
	av_opt_set_int(avctx->priv_data, "context", codec_config.context, 0);
	av_opt_set_int(avctx->priv_data, "coder", codec_config.coder, 0);
	return true;
}

LRESULT CodecFFV1::configure(HWND parent)
{
	ConfigFFV1 dlg;
	dlg.Show(parent, this);
	return ICERR_OK;
}
