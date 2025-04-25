/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "AudioEnc_flac.h"
#include <commctrl.h>
extern "C" {
#include <libavutil/opt.h>
}
#include "../Helper.h"
#include "../resource.h"

class AConfigFlac : public AConfigBase
{
public:
	VDFFAudio_flac::Config* codec_config = nullptr;

	AConfigFlac() { dialog_id = IDD_ENC_FLAC; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void init_quality();
	void change_quality();
};

void AConfigFlac::init_quality()
{
	SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMIN, FALSE, 0);
	SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMAX, TRUE, 12);
	SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETPOS, TRUE, codec_config->quality);
	SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, codec_config->quality, false);
}

void AConfigFlac::change_quality()
{
	int x = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_GETPOS, 0, 0);
	codec_config->quality = x;
	SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, codec_config->quality, false);
}

INT_PTR AConfigFlac::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
	{
		codec_config = (VDFFAudio_flac::Config*)codec->config;
		init_quality();
		CheckDlgButton(mhdlg, IDC_ENC_JOINT_STEREO, codec_config->flags & VDFFAudio_flac::flag_jointstereo);
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
		case IDC_ENC_JOINT_STEREO:
			codec_config->flags &= ~VDFFAudio_flac::flag_jointstereo;
			if (IsDlgButtonChecked(mhdlg, IDC_ENC_JOINT_STEREO)) {
				codec_config->flags |= VDFFAudio_flac::flag_jointstereo;
			}
			break;
		}
	}
	return AConfigBase::DlgProc(msg, wParam, lParam);
}

//-----------------------------------------------------------------------------------

void VDFFAudio_flac::reset_config()
{
	codec_config.clear();
	codec_config.version = 1;
	codec_config.flags = flag_jointstereo;
	codec_config.bitrate = 0;
	codec_config.quality = 5;
}

void VDFFAudio_flac::CreateCodec()
{
	codec = avcodec_find_encoder(AV_CODEC_ID_FLAC);
}

void VDFFAudio_flac::InitContext()
{
	avctx->compression_level = codec_config.quality;
	av_opt_set_int(avctx->priv_data, "ch_mode", (codec_config.flags & flag_jointstereo) ? -1 : 0, 0);
}

void VDFFAudio_flac::ShowConfig(VDXHWND parent)
{
	AConfigFlac cfg;
	cfg.Show((HWND)parent, this);
}

//-----------------------------------------------------------------------------------

bool VDXAPIENTRY ff_create_flacenc(const VDXInputDriverContext* pContext, IVDXAudioEnc** ppDriver)
{
	VDFFAudio* p = new VDFFAudio_flac(*pContext);
	*ppDriver = p;
	p->AddRef();
	return true;
}

VDXAudioEncDefinition ff_flacenc = {
	sizeof(VDXAudioEncDefinition),
	0, //flags
	L"FFmpeg FLAC lossless",
	"ffmpeg_flac",
	ff_create_flacenc
};

VDXPluginInfo ff_flacenc_info = {
	sizeof(VDXPluginInfo),
	L"FFmpeg FLAC lossless",
	L"Anton Shekhovtsov",
	L"Encode audio to FLAC format.",
	1,
	kVDXPluginType_AudioEnc,
	0,
	12, // min api version
	kVDXPlugin_APIVersion,
	kVDXPlugin_AudioEncAPIVersion,  // min output api version
	kVDXPlugin_AudioEncAPIVersion,
	&ff_flacenc
};
