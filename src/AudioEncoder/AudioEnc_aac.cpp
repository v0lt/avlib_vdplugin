/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "AudioEnc_aac.h"
#include <commctrl.h>
#include "../Helper.h"
#include "../resource.h"

class AConfigAAC : public AConfigBase
{
public:
	VDFFAudio_aac::Config* codec_config = nullptr;

	AConfigAAC() { dialog_id = IDD_ENC_AAC; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void init_quality();
	void change_quality();
};

void AConfigAAC::init_quality()
{
	SendDlgItemMessageW(mhdlg, IDC_ENC_BITRATE, TBM_SETRANGEMIN, FALSE, 32);
	SendDlgItemMessageW(mhdlg, IDC_ENC_BITRATE, TBM_SETRANGEMAX, TRUE, 288);
	SendDlgItemMessageW(mhdlg, IDC_ENC_BITRATE, TBM_SETPOS, TRUE, codec_config->bitrate);
	auto str = std::format(L"{} kbit/s", codec_config->bitrate);
	SetDlgItemTextW(mhdlg, IDC_ENC_BITRATE_VALUE, str.c_str());
}

void AConfigAAC::change_quality()
{
	int x = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_BITRATE, TBM_GETPOS, 0, 0);
	codec_config->bitrate = x & ~15;
	auto str = std::format(L"{} kbit/s", codec_config->bitrate);
	SetDlgItemTextW(mhdlg, IDC_ENC_BITRATE_VALUE, str.c_str());
}

INT_PTR AConfigAAC::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
	{
		codec_config = (VDFFAudio_aac::Config*)codec->config;
		init_quality();
		break;
	}

	case WM_HSCROLL:
		if ((HWND)lParam == GetDlgItem(mhdlg, IDC_ENC_BITRATE)) {
			change_quality();
			break;
		}
		return FALSE;
	}
	return AConfigBase::DlgProc(msg, wParam, lParam);
}

//-----------------------------------------------------------------------------------

void VDFFAudio_aac::reset_config()
{
	codec_config.clear();
	codec_config.version = 2;
	codec_config.flags = VDFFAudio::flag_constant_rate;
	codec_config.bitrate = 128;
}

void VDFFAudio_aac::CreateCodec()
{
	codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
}

void VDFFAudio_aac::InitContext()
{
	VDFFAudio::InitContext();
	if (config->flags & flag_constant_rate) {
		avctx->bit_rate = config->bitrate * 1000 * avctx->ch_layout.nb_channels;
	}
}

void VDFFAudio_aac::ShowConfig(VDXHWND parent)
{
	AConfigAAC cfg;
	cfg.Show((HWND)parent, this);
}

//-----------------------------------------------------------------------------------

bool VDXAPIENTRY ff_create_aacenc(const VDXInputDriverContext* pContext, IVDXAudioEnc** ppDriver)
{
	VDFFAudio* p = new VDFFAudio_aac(*pContext);
	*ppDriver = p;
	p->AddRef();
	return true;
}

VDXAudioEncDefinition ff_aacenc = {
	sizeof(VDXAudioEncDefinition),
	0, //flags
	L"FFmpeg AAC",
	"ffmpeg_aac",
	ff_create_aacenc
};

VDXPluginInfo ff_aacenc_info = {
	sizeof(VDXPluginInfo),
	L"FFmpeg AAC",
	L"Anton Shekhovtsov",
	L"Encode audio to AAC format.",
	1,
	kVDXPluginType_AudioEnc,
	0,
	12, // min api version
	kVDXPlugin_APIVersion,
	kVDXPlugin_AudioEncAPIVersion,  // min output api version
	kVDXPlugin_AudioEncAPIVersion,
	&ff_aacenc
};
