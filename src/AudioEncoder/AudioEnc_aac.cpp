/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stdafx.h"

#include "AudioEnc_aac.h"
#include "../Helper.h"
#include "../resource.h"
#include "../registry.h"

class AConfigAAC : public AConfigBase
{
public:
	VDFFAudio_aac::Config* codec_config = nullptr;

	AConfigAAC() { dialog_id = IDD_ENC_AAC; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override;
	void init_quality();
	void change_quality();
};

void AConfigAAC::init_quality()
{
	SendDlgItemMessageW(mhdlg, IDC_ENC_BITRATE, TBM_SETRANGEMIN, FALSE, 32);
	SendDlgItemMessageW(mhdlg, IDC_ENC_BITRATE, TBM_SETRANGEMAX, TRUE, 288);
	SendDlgItemMessageW(mhdlg, IDC_ENC_BITRATE, TBM_SETPOS, TRUE, codec_config->bitrate_per_channel);
	auto str = std::format(L"{} kbit/s", codec_config->bitrate_per_channel);
	SetDlgItemTextW(mhdlg, IDC_ENC_BITRATE_VALUE, str.c_str());
}

void AConfigAAC::change_quality()
{
	int x = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_BITRATE, TBM_GETPOS, 0, 0);
	codec_config->bitrate_per_channel = x & ~15;
	auto str = std::format(L"{} kbit/s", codec_config->bitrate_per_channel);
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

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\AudioEnc_AAC"

void VDFFAudio_aac::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		reg.ReadInt("bitrate_per_channel", codec_config.bitrate_per_channel, 32, 288);
		codec_config.bitrate_per_channel &= ~15;
		reg.CloseKey();
	}
}

void VDFFAudio_aac::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		reg.WriteInt("bitrate_per_channel", codec_config.bitrate_per_channel);
		reg.CloseKey();
	}
}

void VDFFAudio_aac::CreateCodec()
{
	codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
}

void VDFFAudio_aac::InitContext()
{
	avctx->bit_rate = codec_config.bitrate_per_channel * 1000 * avctx->ch_layout.nb_channels;
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

VDXAudioEncDefinition2 ff_aacenc = {
	sizeof(VDXAudioEncDefinition2),
	0, //flags
	L"FFmpeg AAC",
	L"ffmpeg_aac",
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
