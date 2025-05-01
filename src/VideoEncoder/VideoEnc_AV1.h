/*
 * Copyright (C) 2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "VideoEnc.h"

class ConfigAV1 : public ConfigBase {
public:
	ConfigAV1() { dialog_id = IDD_ENC_AV1; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	virtual void init_format();
	virtual void change_format(int sel);
};

const char* stv_av1_preset_names[] = {
	"0 - slow",
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	"8",
	"9",
	"10",
	"11",
	"12",
	"13 - fast",
};

struct CodecAV1 : public CodecBase {
	enum { id_tag = MKTAG('A', 'V', '0', '1') };

	struct Config : public CodecBase::Config {
		int preset; // 0-13
		int crf;    // 0-63

		Config() { reset(); }
		void reset() {
			version = 1;
			format = format_yuv420;
			bits = 8;
			preset = 5;
			crf = 35;
		}
	} codec_config;

	CodecAV1() {
		codec_name = "libsvtav1";
		codec_tag = MKTAG('A', 'V', '0', '1');
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
		info.dwFlags = VIDCF_COMPRESSFRAMES | VIDCF_FASTTEMPORALC;
		wcscpy_s(info.szName, L"av1");
		wcscpy_s(info.szDescription, L"FFmpeg / SVT-AV1");
	}

	bool init_ctx(VDXPixmapLayout* layout)
	{
		avctx->gop_size = -1;
		avctx->max_b_frames = -1;

		[[maybe_unused]] int ret = 0;
		ret = av_opt_set_int(avctx->priv_data, "preset", codec_config.preset, 0);
		ret = av_opt_set_int(avctx->priv_data, "crf", codec_config.crf, 0);
		return true;
	}

	LRESULT configure(HWND parent)
	{
		ConfigAV1 dlg;
		dlg.Show(parent, this);
		return ICERR_OK;
	}
};

INT_PTR ConfigAV1::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CodecAV1::Config* config = (CodecAV1::Config*)codec->config;
	switch (msg) {
		case WM_INITDIALOG:
		{
			SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_RESETCONTENT, 0, 0);
			for (const auto& preset_name : stv_av1_preset_names) {
				SendDlgItemMessageA(mhdlg, IDC_ENC_PROFILE, CB_ADDSTRING, 0, (LPARAM)preset_name);
			}
			SendDlgItemMessageW(mhdlg, IDC_ENC_PROFILE, CB_SETCURSEL, config->preset, 0);

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

void ConfigAV1::init_format()
{
	const char* color_names[] = {
		"YUV 4:2:0",
	};

	SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_RESETCONTENT, 0, 0);
	for (const auto& color_name : color_names) {
		SendDlgItemMessageA(mhdlg, IDC_ENC_COLORSPACE, CB_ADDSTRING, 0, (LPARAM)color_name);
	}
	int sel = 0; // format_yuv420
	SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_SETCURSEL, sel, 0);
}

void ConfigAV1::change_format(int sel)
{
	codec->config->format = CodecBase::format_yuv420;
	init_bits();
}
