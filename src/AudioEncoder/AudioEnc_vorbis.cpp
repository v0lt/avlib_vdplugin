/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "AudioEnc_vorbis.h"
#include <commctrl.h>
#include "../Helper.h"
#include "../resource.h"

class AConfigVorbis : public AConfigBase
{
public:
	VDFFAudio_vorbis::Config* codec_config = nullptr;

	AConfigVorbis() { dialog_id = IDD_ENC_VORBIS; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void init_quality();
	void change_quality();
};

void AConfigVorbis::init_quality()
{
	if (codec_config->flags & VDFFAudio::flag_constant_rate) {
		//! WTF is valid bitrate range? neither ffmpeg nor xiph tell anything but it will fail with error otherwise
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMIN, FALSE, 32);
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMAX, TRUE, 240);
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETPOS, TRUE, codec_config->bitrate);
		auto str = std::format(L"{} kbit/s", codec_config->bitrate);
		SetDlgItemTextW(mhdlg, IDC_ENC_QUALITY_VALUE, str.c_str());
		SetDlgItemTextW(mhdlg, IDC_ENC_QUALITY_LABEL, L"Bitrate");
	}
	else {
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMIN, FALSE, -1);
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMAX, TRUE, 10);
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETPOS, TRUE, codec_config->quality);
		SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, codec_config->quality, true);
		SetDlgItemTextW(mhdlg, IDC_ENC_QUALITY_LABEL, L"Quality");
	}
}

void AConfigVorbis::change_quality()
{
	int x = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_GETPOS, 0, 0);
	if (codec_config->flags & VDFFAudio::flag_constant_rate) {
		codec_config->bitrate = x;
		auto str = std::format(L"{} kbit/s", codec_config->bitrate);
		SetDlgItemTextW(mhdlg, IDC_ENC_QUALITY_VALUE, str.c_str());
	}
	else {
		codec_config->quality = x;
		SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, codec_config->quality, true);
	}
}

INT_PTR AConfigVorbis::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
	{
		codec_config = (VDFFAudio_vorbis::Config*)codec->config;
		init_quality();
		CheckDlgButton(mhdlg, IDC_ENC_CBR, codec_config->flags & VDFFAudio::flag_constant_rate);
		break;
	}

	case WM_HSCROLL:
		if ((HWND)lParam == GetDlgItem(mhdlg, IDC_ENC_QUALITY)) {
			change_quality();
			break;
		}
		return FALSE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_ENC_CBR:
			codec_config->flags &= ~VDFFAudio::flag_constant_rate;
			if (IsDlgButtonChecked(mhdlg, IDC_ENC_CBR)) {
				codec_config->flags |= VDFFAudio::flag_constant_rate;
			}
			init_quality();
			break;
		}
	}
	return AConfigBase::DlgProc(msg, wParam, lParam);
}

//-----------------------------------------------------------------------------------

void VDFFAudio_vorbis::reset_config()
{
	codec_config.clear();
	codec_config.version = 2;
	codec_config.flags = 0;
	codec_config.bitrate = 160;
	codec_config.quality = 3;
}

int VDFFAudio_vorbis::SuggestFileFormat(const char* name)
{
	if (strcmp(name, "wav") == 0 || strcmp(name, "avi") == 0) {
		// disable now because seeking is broken
		return vd2::kFormat_Unwise;
	}
	return VDFFAudio::SuggestFileFormat(name);
}

void VDFFAudio_vorbis::CreateCodec()
{
	codec = avcodec_find_encoder(AV_CODEC_ID_VORBIS);
}

void VDFFAudio_vorbis::InitContext()
{
	if (config->flags & flag_constant_rate) {
		avctx->bit_rate = config->bitrate * 1000 * avctx->ch_layout.nb_channels;
		avctx->rc_min_rate = avctx->bit_rate;
		avctx->rc_max_rate = avctx->bit_rate;
	}
	else {
		avctx->flags |= AV_CODEC_FLAG_QSCALE;
		avctx->global_quality = FF_QP2LAMBDA * config->quality;
		avctx->bit_rate = 0;
		avctx->rc_min_rate = 0;
		avctx->rc_max_rate = 0;
	}
}

void VDFFAudio_vorbis::ShowConfig(VDXHWND parent)
{
	AConfigVorbis cfg;
	cfg.Show((HWND)parent, this);
}

//-----------------------------------------------------------------------------------

bool VDXAPIENTRY ff_create_vorbisenc(const VDXInputDriverContext* pContext, IVDXAudioEnc** ppDriver)
{
	VDFFAudio* p = new VDFFAudio_vorbis(*pContext);
	*ppDriver = p;
	p->AddRef();
	return true;
}

VDXAudioEncDefinition ff_vorbisenc = {
	sizeof(VDXAudioEncDefinition),
	0, //flags
	L"FFmpeg Vorbis (libvorbis)",
	"ffmpeg_vorbis",
	ff_create_vorbisenc
};

VDXPluginInfo ff_vorbisenc_info = {
	sizeof(VDXPluginInfo),
	L"FFmpeg Vorbis (libvorbis)",
	L"Anton Shekhovtsov",
	L"Encode audio to Vorbis format.",
	1,
	kVDXPluginType_AudioEnc,
	0,
	12, // min api version
	kVDXPlugin_APIVersion,
	kVDXPlugin_AudioEncAPIVersion,  // min output api version
	kVDXPlugin_AudioEncAPIVersion,
	&ff_vorbisenc
};
