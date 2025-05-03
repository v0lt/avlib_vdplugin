/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "AudioEnc_mp3.h"
#include <commctrl.h>
extern "C" {
#include <libavutil/opt.h>
}
#include "../Helper.h"
#include "../resource.h"
#include "../registry.h"

const int mp3_bitrates[] = { 8, 16, 24, 32, 40, 48, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 };

class AConfigMp3 : public AConfigBase
{
public:
	VDFFAudio_mp3::Config* codec_config = nullptr;

	AConfigMp3() { dialog_id = IDD_ENC_MP3; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void init_quality();
	void change_quality();
};

void AConfigMp3::init_quality()
{
	if (codec_config->constant_rate) {
		size_t x = 0;
		for (; x < std::size(mp3_bitrates); x++) {
			if (mp3_bitrates[x] == codec_config->bitrate) {
				break;
			}
		}

		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMIN, FALSE, 0);
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMAX, TRUE, std::size(mp3_bitrates) - 1);
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETPOS, TRUE, x);
		auto str = std::format(L"{} kbit/s", codec_config->bitrate);
		SetDlgItemTextW(mhdlg, IDC_ENC_QUALITY_VALUE, str.c_str());
		SetDlgItemTextW(mhdlg, IDC_ENC_QUALITY_LABEL, L"Bitrate");
	}
	else {
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMIN, FALSE, 0);
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMAX, TRUE, 9);
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETPOS, TRUE, codec_config->quality);
		SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, codec_config->quality, false);
		SetDlgItemTextW(mhdlg, IDC_ENC_QUALITY_LABEL, L"Quality (high-low)");
	}
}

void AConfigMp3::change_quality()
{
	int x = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_GETPOS, 0, 0);
	if (codec_config->constant_rate) {
		codec_config->bitrate = mp3_bitrates[x];
		auto str = std::format(L"{} kbit/s", codec_config->bitrate);
		SetDlgItemTextW(mhdlg, IDC_ENC_QUALITY_VALUE, str.c_str());
	}
	else {
		codec_config->quality = x;
		SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, codec_config->quality, false);
	}
}

INT_PTR AConfigMp3::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
	{
		codec_config = (VDFFAudio_mp3::Config*)codec->config;
		init_quality();
		CheckDlgButton(mhdlg, IDC_ENC_JOINT_STEREO, codec_config->jointstereo ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(mhdlg, IDC_ENC_CBR, codec_config->constant_rate ? BST_CHECKED : BST_UNCHECKED);
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
			codec_config->jointstereo = IsDlgButtonChecked(mhdlg, IDC_ENC_JOINT_STEREO) ? true : false;
			break;
		case IDC_ENC_CBR:
			codec_config->constant_rate = IsDlgButtonChecked(mhdlg, IDC_ENC_CBR) ? true : false;
			init_quality();
			break;
		}
	}
	return AConfigBase::DlgProc(msg, wParam, lParam);
}

//-----------------------------------------------------------------------------------

#define REG_KEY_APP "Software\\VirtualDub2\\avlib\\AudioEnc_MP3"

void VDFFAudio_mp3::load_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.OpenKeyRead() == ERROR_SUCCESS) {
		reg.ReadInt("bitrate", codec_config.bitrate, mp3_bitrates, std::size(mp3_bitrates));
		reg.ReadInt("quality", codec_config.quality, 0, 10);
		reg.ReadBool("constant_rate", codec_config.constant_rate);
		reg.ReadBool("jointstereo", codec_config.jointstereo);
		reg.CloseKey();
	}
}

void VDFFAudio_mp3::save_config()
{
	RegistryPrefs reg(REG_KEY_APP);
	if (reg.CreateKeyWrite() == ERROR_SUCCESS) {
		reg.WriteInt("bitrate", codec_config.bitrate);
		reg.WriteInt("quality", codec_config.quality);
		reg.WriteBool("constant_rate", codec_config.constant_rate);
		reg.WriteBool("jointstereo", codec_config.jointstereo);
		reg.CloseKey();
	}
}

void VDFFAudio_mp3::CreateCodec()
{
	codec = avcodec_find_encoder(AV_CODEC_ID_MP3);
}

void VDFFAudio_mp3::InitContext()
{
	if (codec_config.constant_rate) {
		avctx->bit_rate = codec_config.bitrate * 1000;
	}
	else {
		avctx->flags |= AV_CODEC_FLAG_QSCALE;
		avctx->global_quality = FF_QP2LAMBDA * codec_config.quality;
		avctx->bit_rate = avctx->sample_rate * 4; // this estimate is fake, but leaving bit_rate=0 is worse
	}
	av_opt_set_int(avctx->priv_data, "joint_stereo", codec_config.jointstereo, 0);
}

void VDFFAudio_mp3::ShowConfig(VDXHWND parent)
{
	AConfigMp3 cfg;
	cfg.Show((HWND)parent, this);
}

//-----------------------------------------------------------------------------------

bool VDXAPIENTRY ff_create_mp3enc(const VDXInputDriverContext* pContext, IVDXAudioEnc** ppDriver)
{
	VDFFAudio* p = new VDFFAudio_mp3(*pContext);
	*ppDriver = p;
	p->AddRef();
	return true;
}

VDXAudioEncDefinition ff_mp3enc = {
	sizeof(VDXAudioEncDefinition),
	0, //flags
	L"FFmpeg Lame MP3",
	"ffmpeg_mp3",
	ff_create_mp3enc
};

VDXPluginInfo ff_mp3enc_info = {
	sizeof(VDXPluginInfo),
	L"FFmpeg Lame MP3",
	L"Anton Shekhovtsov",
	L"Encode audio to MP3 format.",
	1,
	kVDXPluginType_AudioEnc,
	0,
	12, // min api version
	kVDXPlugin_APIVersion,
	kVDXPlugin_AudioEncAPIVersion,  // min output api version
	kVDXPlugin_AudioEncAPIVersion,
	&ff_mp3enc
};