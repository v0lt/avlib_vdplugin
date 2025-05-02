/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "VideoEnc_FFVHuff.h"
#include "../resource.h"
#include "../registry.h"

//
// ConfigHUFF
//

class ConfigHUFF : public ConfigBase {
public:
	ConfigHUFF() {
		dialog_id = IDD_ENC_FFVHUFF;
		idc_message = IDC_ENC_MESSAGE;
	}
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
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

//
// CodecHUFF
//

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\VideoEnc_FFVHuff"

void CodecHUFF::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {

		reg.CloseKey();
	}
}

void CodecHUFF::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {

		reg.CloseKey();
	}
}

bool CodecHUFF::init_ctx(VDXPixmapLayout* layout)
{
	int pred = codec_config.prediction;
	if (pred == 2 && config->format == format_rgb && config->bits == 8) {
		pred = 0;
	}
	av_opt_set_int(avctx->priv_data, "pred", pred, 0);
	return true;
}

LRESULT CodecHUFF::configure(HWND parent)
{
	ConfigHUFF dlg;
	dlg.Show(parent, this);
	return ICERR_OK;
}
