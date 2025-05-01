/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "AudioEnc_opus.h"
#include <commctrl.h>
extern "C" {
#include <libavutil/opt.h>
}
#include "../Helper.h"
#include "../resource.h"
#include "../registry.h"

class AConfigOpus : public AConfigBase
{
public:
	VDFFAudio_opus::Config* codec_config = nullptr;

	AConfigOpus() { dialog_id = IDD_ENC_OPUS; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void init_rate();
	void init_quality();
	void change_quality();
	void change_bitrate();
};

void AConfigOpus::init_quality()
{
	SendDlgItemMessageW(mhdlg, IDC_ENC_BITRATE, TBM_SETRANGEMIN, FALSE, 6);
	SendDlgItemMessageW(mhdlg, IDC_ENC_BITRATE, TBM_SETRANGEMAX, TRUE, 256);
	SendDlgItemMessageW(mhdlg, IDC_ENC_BITRATE, TBM_SETPOS, TRUE, codec_config->bitrate_per_channel);
	auto str = std::format(L"{} kbit/s", codec_config->bitrate_per_channel);
	SetDlgItemTextW(mhdlg, IDC_ENC_BITRATE_VALUE, str.c_str());

	SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMIN, FALSE, 0);
	SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMAX, TRUE, 10);
	SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETPOS, TRUE, codec_config->quality);
	SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, codec_config->quality, false);
}

void AConfigOpus::change_quality()
{
	int x = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_GETPOS, 0, 0);
	codec_config->quality = x;
	SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, codec_config->quality, false);
}

void AConfigOpus::change_bitrate()
{
	int x = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_BITRATE, TBM_GETPOS, 0, 0);
	codec_config->bitrate_per_channel = x;
	auto str = std::format(L"{} kbit/s", codec_config->bitrate_per_channel);
	SetDlgItemTextW(mhdlg, IDC_ENC_BITRATE_VALUE, str.c_str());
}

void AConfigOpus::init_rate()
{
	CheckDlgButton(mhdlg, IDC_ENC_CBR, (codec_config->bitrate_mode == 0) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(mhdlg, IDC_ENC_ABR, (codec_config->bitrate_mode == 2) ? BST_CHECKED : BST_UNCHECKED);
}

INT_PTR AConfigOpus::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
	{
		codec_config = (VDFFAudio_opus::Config*)codec->config;
		init_quality();
		init_rate();
		break;
	}

	case WM_HSCROLL:
		if ((HWND)lParam == GetDlgItem(mhdlg, IDC_ENC_QUALITY)) {
			change_quality();
			break;
		}
		if ((HWND)lParam == GetDlgItem(mhdlg, IDC_ENC_BITRATE)) {
			change_bitrate();
			break;
		}
		return FALSE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_ENC_CBR:
			codec_config->bitrate_mode = IsDlgButtonChecked(mhdlg, IDC_ENC_CBR) ? 0 : 1;
			init_rate();
			break;
		case IDC_ENC_ABR:
			codec_config->bitrate_mode = IsDlgButtonChecked(mhdlg, IDC_ENC_ABR) ? 2 : 1;
			init_rate();
			break;
		}
	}
	return AConfigBase::DlgProc(msg, wParam, lParam);
}

//-----------------------------------------------------------------------------------

void VDFFAudio_opus::reset_config()
{
	codec_config.version = 2;
	codec_config.bitrate_per_channel = 64;
	codec_config.quality = 10;
}

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\AudioEnc_Opus"

void VDFFAudio_opus::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		reg.ReadInt("bitrate_per_channel", codec_config.bitrate_per_channel, 6, 256);
		reg.ReadInt("quality", codec_config.quality, 0, 10);
		reg.ReadInt8("bitrate_mode", codec_config.bitrate_mode, 0, 2);
		reg.CloseKey();
	}
}

void VDFFAudio_opus::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		reg.WriteInt("bitrate_per_channel", codec_config.bitrate_per_channel);
		reg.WriteInt("quality", codec_config.quality);
		reg.WriteInt8("bitrate_mode", codec_config.bitrate_mode);
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

VDXAudioEncDefinition ff_opusenc = {
	sizeof(VDXAudioEncDefinition),
	0, //flags
	L"FFmpeg Opus (libopus)",
	"ffmpeg_opus",
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
