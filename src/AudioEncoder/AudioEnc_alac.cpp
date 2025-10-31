/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stdafx.h"

#include "AudioEnc_alac.h"
#include "../Helper.h"
#include "../resource.h"
#include "../registry.h"

class AConfigAlac : public AConfigBase
{
public:
	VDFFAudio_alac::Config* codec_config = nullptr;

	AConfigAlac() { dialog_id = IDD_ENC_ALAC; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override;
	void init_quality();
	void change_quality();
};

void AConfigAlac::init_quality()
{
	SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMIN, FALSE, 0);
	SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMAX, TRUE, 2);
	SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETPOS, TRUE, codec_config->compression_level);
	SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, codec_config->compression_level, FALSE);
}

void AConfigAlac::change_quality()
{
	int x = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_GETPOS, 0, 0);
	codec_config->compression_level = x;
	SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, codec_config->compression_level, FALSE);
}

INT_PTR AConfigAlac::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
	{
		codec_config = (VDFFAudio_alac::Config*)codec->config;
		init_quality();
		break;
	}

	case WM_HSCROLL:
		if ((HWND)lParam == GetDlgItem(mhdlg, IDC_ENC_QUALITY)) {
			change_quality();
			break;
		}
		return FALSE;
	}
	return AConfigBase::DlgProc(msg, wParam, lParam);
}

//-----------------------------------------------------------------------------------

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\AudioEnc_ALAC"

void VDFFAudio_alac::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		reg.ReadInt("compression_level", codec_config.compression_level, 0, 2);
		reg.CloseKey();
	}
}

void VDFFAudio_alac::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		reg.WriteInt("compression_level", codec_config.compression_level);
		reg.CloseKey();
	}
}

void VDFFAudio_alac::CreateCodec()
{
	codec = avcodec_find_encoder(AV_CODEC_ID_ALAC);
}

void VDFFAudio_alac::InitContext()
{
	avctx->compression_level = codec_config.compression_level;
}

void VDFFAudio_alac::ShowConfig(VDXHWND parent)
{
	AConfigAlac cfg;
	cfg.Show((HWND)parent, this);
}

//-----------------------------------------------------------------------------------

bool VDXAPIENTRY ff_create_alacenc(const VDXInputDriverContext* pContext, IVDXAudioEnc** ppDriver)
{
	VDFFAudio* p = new VDFFAudio_alac(*pContext);
	*ppDriver = p;
	p->AddRef();
	return true;
}

VDXAudioEncDefinition2 ff_alacenc = {
	sizeof(VDXAudioEncDefinition2),
	0, //flags
	L"FFmpeg ALAC (Apple Lossless)",
	L"ffmpeg_alac",
	ff_create_alacenc
};

VDXPluginInfo ff_alacenc_info = {
	sizeof(VDXPluginInfo),
	L"FFmpeg ALAC (Apple Lossless)",
	L"Anton Shekhovtsov",
	L"Encode audio to ALAC format.",
	1,
	kVDXPluginType_AudioEnc,
	0,
	12, // min api version
	kVDXPlugin_APIVersion,
	kVDXPlugin_AudioEncAPIVersion,  // min output api version
	kVDXPlugin_AudioEncAPIVersion,
	&ff_alacenc
};
