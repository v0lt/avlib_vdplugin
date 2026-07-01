/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2026 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stdafx.h"

#include "AudioEnc_opus.h"
extern "C" {
#include <libavutil/opt.h>
}
#include "../Helper.h"
#include "../resource.h"
#include "../registry.h"

class AConfigOpus : public AConfigBase
{
public:
	VDFFAudio_opus::Config* config = nullptr;

	AConfigOpus() { dialog_id = IDD_ENC_OPUS; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void init_quality();
	void change_quality();
	void change_bitrate();
};

void AConfigOpus::init_quality()
{
	SendDlgItemMessageW(mhdlg, IDC_ENC_BITRATE_SLIDER, TBM_SETRANGEMIN, FALSE, 6);
	SendDlgItemMessageW(mhdlg, IDC_ENC_BITRATE_SLIDER, TBM_SETRANGEMAX, TRUE, 256);
	SendDlgItemMessageW(mhdlg, IDC_ENC_BITRATE_SLIDER, TBM_SETPOS, TRUE, config->bitrate_per_channel);
	auto str = std::format(L"{} kbit/s", config->bitrate_per_channel);
	SetDlgItemTextW(mhdlg, IDC_ENC_BITRATE_VALUE, str.c_str());

	SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY_SLIDER, TBM_SETRANGEMIN, FALSE, 0);
	SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY_SLIDER, TBM_SETRANGEMAX, TRUE, 10);
	SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY_SLIDER, TBM_SETPOS, TRUE, config->quality);
	SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, config->quality, FALSE);
}

void AConfigOpus::change_quality()
{
	int x = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY_SLIDER, TBM_GETPOS, 0, 0);
	config->quality = x;
	SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, config->quality, FALSE);
}

void AConfigOpus::change_bitrate()
{
	int x = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_BITRATE_SLIDER, TBM_GETPOS, 0, 0);
	config->bitrate_per_channel = x;
	auto str = std::format(L"{} kbit/s", config->bitrate_per_channel);
	SetDlgItemTextW(mhdlg, IDC_ENC_BITRATE_VALUE, str.c_str());
}

INT_PTR AConfigOpus::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
	{
		config = (VDFFAudio_opus::Config*)codec->config;
		init_quality();
		SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_ADDSTRING, 0, (LPARAM)L"Constant bitrate");
		SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_ADDSTRING, 0, (LPARAM)L"Variable bitrate");
		SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_ADDSTRING, 0, (LPARAM)L"Constrained variable bitrate");
		SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_SETCURSEL, (WPARAM)config->bitrate_mode, 0);
		break;
	}

	case WM_HSCROLL:
		if ((HWND)lParam == GetDlgItem(mhdlg, IDC_ENC_QUALITY_SLIDER)) {
			change_quality();
			break;
		}
		if ((HWND)lParam == GetDlgItem(mhdlg, IDC_ENC_BITRATE_SLIDER)) {
			change_bitrate();
			break;
		}
		return FALSE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_BUTTON_DEFAULT:
			codec->reset_config();
			init_quality();
			SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_SETCURSEL, (WPARAM)config->bitrate_mode, 0);
			break;
		case IDC_ENC_RATECONTROL:
			if (HIWORD(wParam) == LBN_SELCHANGE) {
				config->bitrate_mode = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_RATECONTROL, CB_GETCURSEL, 0, 0);
				return TRUE;
			}
			break;
		}
	}
	return AConfigBase::DlgProc(msg, wParam, lParam);
}

//-----------------------------------------------------------------------------------

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\AudioEnc_Opus"

void VDFFAudio_opus::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		reg.ReadInt("bitrate_per_channel", codec_config.bitrate_per_channel, 6, 256);
		reg.ReadInt("quality", codec_config.quality, 0, 10);
		reg.ReadInt("bitrate_mode", codec_config.bitrate_mode, 0, 2);
		reg.CloseKey();
	}
}

void VDFFAudio_opus::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		reg.WriteInt("bitrate_per_channel", codec_config.bitrate_per_channel);
		reg.WriteInt("quality", codec_config.quality);
		reg.WriteInt("bitrate_mode", codec_config.bitrate_mode);
		reg.CloseKey();
	}
}

void VDFFAudio_opus::CreateCodec()
{
	codec = avcodec_find_encoder_by_name("libopus");
}

void VDFFAudio_opus::InitContext()
{
	avctx->bit_rate = codec_config.bitrate_per_channel * 1000 * avctx->ch_layout.nb_channels;
	avctx->compression_level = codec_config.quality;
	av_opt_set_int(avctx->priv_data, "vbr", codec_config.bitrate_mode, 0);
}

void VDFFAudio_opus::ShowConfig(VDXHWND parent)
{
	AConfigOpus cfg;
	cfg.Show((HWND)parent, this);
}

//-----------------------------------------------------------------------------------

bool VDXAPIENTRY ff_create_opusenc(const VDXInputDriverContext* pContext, IVDXAudioEnc** ppDriver)
{
	VDFFAudio* p = new VDFFAudio_opus(*pContext);
	*ppDriver = p;
	p->AddRef();
	return true;
}

VDXAudioEncDefinition2 ff_opusenc = {
	sizeof(VDXAudioEncDefinition2),
	0, //flags
	L"FFmpeg Opus (libopus)",
	L"ffmpeg_opus",
	ff_create_opusenc
};

VDXPluginInfo ff_opusenc_info = {
	sizeof(VDXPluginInfo),
	L"FFmpeg Opus (libopus)",
	L"Anton Shekhovtsov",
	L"Encode audio to Opus format.",
	1,
	kVDXPluginType_AudioEnc,
	0,
	12, // min api version
	kVDXPlugin_APIVersion,
	kVDXPlugin_AudioEncAPIVersion,  // min output api version
	kVDXPlugin_AudioEncAPIVersion,
	&ff_opusenc
};
