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

class AConfigOpus : public AConfigBase
{
public:
	VDFFAudio_opus::Config* codec_config = nullptr;

	AConfigOpus() { dialog_id = IDD_ENC_OPUS; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void init_flags();
	void init_quality();
	void change_quality();
	void change_bitrate();
};

void AConfigOpus::init_quality()
{
	SendDlgItemMessageW(mhdlg, IDC_ENC_BITRATE, TBM_SETRANGEMIN, FALSE, 6);
	SendDlgItemMessageW(mhdlg, IDC_ENC_BITRATE, TBM_SETRANGEMAX, TRUE, 256);
	SendDlgItemMessageW(mhdlg, IDC_ENC_BITRATE, TBM_SETPOS, TRUE, codec_config->bitrate);
	auto str = std::format(L"{} kbit/s", codec_config->bitrate);
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
	codec_config->bitrate = x;
	auto str = std::format(L"{} kbit/s", codec_config->bitrate);
	SetDlgItemTextW(mhdlg, IDC_ENC_BITRATE_VALUE, str.c_str());
}

void AConfigOpus::init_flags()
{
	CheckDlgButton(mhdlg, IDC_ENC_CBR, codec_config->flags & VDFFAudio::flag_constant_rate);
	CheckDlgButton(mhdlg, IDC_ENC_ABR, codec_config->flags & VDFFAudio_opus::flag_limited_rate);
}

INT_PTR AConfigOpus::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
	{
		codec_config = (VDFFAudio_opus::Config*)codec->config;
		init_quality();
		init_flags();
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
			codec_config->flags &= ~VDFFAudio::flag_constant_rate;
			codec_config->flags &= ~VDFFAudio_opus::flag_limited_rate;
			if (IsDlgButtonChecked(mhdlg, IDC_ENC_CBR)) {
				codec_config->flags |= VDFFAudio::flag_constant_rate;
			}
			init_flags();
			break;
		case IDC_ENC_ABR:
			codec_config->flags &= ~VDFFAudio::flag_constant_rate;
			codec_config->flags &= ~VDFFAudio_opus::flag_limited_rate;
			if (IsDlgButtonChecked(mhdlg, IDC_ENC_ABR)) {
				codec_config->flags |= VDFFAudio_opus::flag_limited_rate;
			}
			init_flags();
			break;
		}
	}
	return AConfigBase::DlgProc(msg, wParam, lParam);
}

//-----------------------------------------------------------------------------------

void VDFFAudio_opus::reset_config()
{
	codec_config.clear();
	codec_config.version = 1;
	codec_config.bitrate = 64;
	codec_config.quality = 10;
}

void VDFFAudio_opus::CreateCodec()
{
	codec = avcodec_find_encoder_by_name("libopus");
}

void VDFFAudio_opus::InitContext()
{
	avctx->bit_rate = config->bitrate * 1000 * avctx->ch_layout.nb_channels;
	avctx->compression_level = config->quality;

	if (config->flags & flag_constant_rate) {
		av_opt_set_int(avctx->priv_data, "vbr", 0, 0);
	}
	else if (config->flags & flag_limited_rate) {
		av_opt_set_int(avctx->priv_data, "vbr", 2, 0);
	}
	else {
		av_opt_set_int(avctx->priv_data, "vbr", 1, 0);
	}
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
