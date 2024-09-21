/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2024 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "a_compress.h"
#include "export.h"
#include <string>
#include <Ks.h>
#include <KsMedia.h>
#include <vd2/VDXFrame/VideoFilterDialog.h>
#include "resource.h"
#include <commctrl.h>
#include "Utils.h"

void init_av();
extern HINSTANCE hInstance;

VDFFAudio::VDFFAudio(const VDXInputDriverContext& pContext)
	: mContext(pContext)
{
	pkt = av_packet_alloc();
}

VDFFAudio::~VDFFAudio()
{
	cleanup();
	av_packet_free(&pkt);
}

void VDFFAudio::cleanup()
{
	if (ctx) {
		av_freep(&ctx->extradata);
		avcodec_free_context(&ctx);
	}
	if (frame) {
		av_frame_free(&frame);
	}
	av_packet_unref(pkt);
	if (swr) {
		swr_free(&swr);
	}
	if (sample_buf) {
		av_freep(&sample_buf[0]);
		free(sample_buf);
		free(in_buf);
		sample_buf = nullptr;
		in_buf = nullptr;
	}
	if (out_format) {
		free(out_format);
		out_format = nullptr;
		out_format_size = 0;
	}
}

void VDFFAudio::SetConfig(void* data, size_t size)
{
	size_t rsize = GetConfigSize();
	if (size != rsize) return;
	int src_version = *(int*)data;
	if (src_version != config->version) {
		reset_config();
		return;
	}
	memcpy(config, data, rsize);
	return;
}

void VDFFAudio::InitContext()
{
	if (config->flags & flag_constant_rate) {
		ctx->bit_rate = config->bitrate * 1000;
	}
	else {
		ctx->flags |= AV_CODEC_FLAG_QSCALE;
		ctx->global_quality = FF_QP2LAMBDA * config->quality;
	}
}

void VDFFAudio::export_wav()
{
	wav_compatible = false;
	if (out_format) {
		free(out_format);
		out_format = nullptr;
		out_format_size = 0;
	}
	IOWBuffer io;
	int buf_size = 4096;
	void* buf = av_malloc(buf_size);
	AVIOContext* avio_ctx = avio_alloc_context((unsigned char*)buf, buf_size, 1, &io, 0, &IOWBuffer::Write, &IOWBuffer::Seek);
	AVFormatContext* ofmt = avformat_alloc_context();
	ofmt->pb = avio_ctx;
	ofmt->oformat = av_guess_format("wav", 0, 0);
	AVStream* st = avformat_new_stream(ofmt, 0);
	st->time_base = av_make_q(1, ctx->sample_rate);
	avcodec_parameters_from_context(st->codecpar, ctx);
	if (avformat_write_header(ofmt, 0) < 0) goto cleanup;
	if (av_write_trailer(ofmt) < 0) goto cleanup;

	{
		out_format_size = *(int*)(io.data + 16);
		WAVEFORMATEXTENSIBLE* ff = (WAVEFORMATEXTENSIBLE*)(io.data + 20);
		out_format = (WAVEFORMATEXTENSIBLE*)malloc(out_format_size);
		memcpy(out_format, ff, out_format_size);
		wav_compatible = true;

		if (codec->id == AV_CODEC_ID_AAC) {
			// is this ffmpeg' job to calculate right size?
			if (max_packet > out_format->Format.nBlockAlign)
				out_format->Format.nBlockAlign = max_packet;
		}
	}

cleanup:
	av_free(avio_ctx->buffer);
	av_free(avio_ctx);
	avformat_free_context(ofmt);

	if (!out_format) {
		// make our private descriptor
		out_format_size = sizeof(WAVEFORMATEX_VDFF) + ctx->extradata_size;
		out_format = (WAVEFORMATEXTENSIBLE*)malloc(out_format_size);
		out_format->Format.wFormatTag        = WAVE_FORMAT_EXTENSIBLE;
		out_format->Format.nChannels         = ctx->ch_layout.nb_channels;
		out_format->Format.nSamplesPerSec    = ctx->sample_rate;
		out_format->Format.nAvgBytesPerSec   = (DWORD)(ctx->bit_rate / 8);
		out_format->Format.nBlockAlign       = ctx->block_align;
		out_format->Format.wBitsPerSample    = ctx->bits_per_coded_sample;
		out_format->Format.cbSize            = WORD(out_format_size - sizeof(WAVEFORMATEX));
		out_format->Samples.wSamplesPerBlock = ctx->frame_size;
		out_format->dwChannelMask            = 0;
		out_format->SubFormat                = KSDATAFORMAT_SUBTYPE_VDFF;

		WAVEFORMATEX_VDFF* ff = (WAVEFORMATEX_VDFF*)out_format;
		ff->codec_id = ctx->codec_id;
		if (ctx->extradata_size) {
			char* p = (char*)(ff + 1);
			memcpy(p, ctx->extradata, ctx->extradata_size);
		}
	}
}

void VDFFAudio::select_fmt(AVSampleFormat* list)
{
	int best_pos = -1;

	for (int y = 0; ; y++) {
		AVSampleFormat yfmt = codec->sample_fmts[y];
		if (yfmt == AV_SAMPLE_FMT_NONE) break;

		for (int x = 0; ; x++) {
			AVSampleFormat xfmt = list[x];
			if (xfmt == AV_SAMPLE_FMT_NONE) break;
			if (xfmt == yfmt) {
				if (best_pos == -1 || x < best_pos) {
					best_pos = x;
					ctx->sample_fmt = yfmt;
				}
			}
		}
	}
}

void VDFFAudio::SetInputFormat(VDXWAVEFORMATEX* format)
{
	cleanup();
	if (!format) {
		return;
	}

	std::string errstr;

	init_av();
	CreateCodec();

	ctx = avcodec_alloc_context3(codec);

	av_channel_layout_default(&ctx->ch_layout, format->mChannels);
	ctx->sample_rate = format->mSamplesPerSec;
	ctx->sample_fmt = codec->sample_fmts[0];

	AVSampleFormat in_fmt = AV_SAMPLE_FMT_S16;
	if (format->mBitsPerSample == 8) {
		in_fmt = AV_SAMPLE_FMT_U8;
	}
	if (format->mFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
		in_fmt = AV_SAMPLE_FMT_FLT;
	}
	else if (format->mFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		WAVEFORMATEXTENSIBLE* fx = (WAVEFORMATEXTENSIBLE*)(format);
		if (fx->dwChannelMask && av_popcount(fx->dwChannelMask) == format->mChannels) {
			av_channel_layout_from_mask(&ctx->ch_layout, fx->dwChannelMask);
		}
		if (fx->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) in_fmt = AV_SAMPLE_FMT_FLT;
	}

	if (in_fmt == AV_SAMPLE_FMT_FLT) {
		AVSampleFormat list[] = {
			AV_SAMPLE_FMT_FLT,
			AV_SAMPLE_FMT_FLTP,
			AV_SAMPLE_FMT_S32,
			AV_SAMPLE_FMT_S32P,
			AV_SAMPLE_FMT_S16,
			AV_SAMPLE_FMT_S16P,
			AV_SAMPLE_FMT_NONE
		};
		select_fmt(list);
	}
	if (in_fmt == AV_SAMPLE_FMT_S16) {
		AVSampleFormat list[] = {
			AV_SAMPLE_FMT_S16,
			AV_SAMPLE_FMT_S16P,
			AV_SAMPLE_FMT_FLT,
			AV_SAMPLE_FMT_FLTP,
			AV_SAMPLE_FMT_NONE
		};
		select_fmt(list);
	}

	//if(ofmt->oformat->flags & AVFMT_GLOBALHEADER)
	//	ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	InitContext();

	int ret = avcodec_open2(ctx, codec, nullptr);
	if (ret < 0) {
		errstr = AVError2Str(ret);
		mContext.mpCallbacks->SetError("FFMPEG: Cannot open codec (%s).", errstr.c_str());
		cleanup();
		return;
	}

	export_wav();
	if (!out_format) {
		mContext.mpCallbacks->SetError("FFMPEG: Audio format is not compatible.");
		cleanup();
		return;
	}

	swr = swr_alloc();

	ret = av_opt_set_chlayout(swr, "in_chlayout", &ctx->ch_layout, 0);
	ret = av_opt_set_int(swr, "in_sample_rate", ctx->sample_rate, 0);
	ret = av_opt_set_sample_fmt(swr, "in_sample_fmt", in_fmt, 0);

	ret = av_opt_set_chlayout(swr, "out_chlayout", &ctx->ch_layout, 0);
	ret = av_opt_set_int(swr, "out_sample_rate", ctx->sample_rate, 0);
	ret = av_opt_set_sample_fmt(swr, "out_sample_fmt", ctx->sample_fmt, 0);

	ret = swr_init(swr);
	if (ret < 0) {
		errstr = AVError2Str(ret);
		mContext.mpCallbacks->SetError("FFMPEG: Audio resampler error (%s).", errstr.c_str());
		cleanup();
		return;
	}

	frame = av_frame_alloc();
	av_channel_layout_copy(&frame->ch_layout, &ctx->ch_layout);
	frame->format = ctx->sample_fmt;
	frame->sample_rate = ctx->sample_rate;
	frame->pts = 0;
	frame->nb_samples = 0;

	frame_size = ctx->frame_size;
	sample_buf = (uint8**)malloc(ctx->ch_layout.nb_channels * sizeof(void*));
	av_samples_alloc(sample_buf, 0, ctx->ch_layout.nb_channels, frame_size, ctx->sample_fmt, 0);

	av_samples_get_buffer_size(&src_linesize, ctx->ch_layout.nb_channels, 1, in_fmt, 1);
	in_buf = (uint8*)malloc(src_linesize * frame_size);
}

void VDFFAudio::GetStreamInfo(VDXStreamInfo& si) const
{
	si.avcodec_version = LIBAVCODEC_VERSION_INT;
	si.block_align = ctx->block_align;
	si.frame_size = ctx->frame_size;
	si.initial_padding = ctx->initial_padding;
	si.trailing_padding = ctx->trailing_padding;

	// proof: ff_parse_specific_params
	// au_rate = par->sample_rate
	// au_scale = par->frame_size (depends on codec but here it must be known)
	// au_ssize = par->block_align

	si.aviHeader.dwSampleSize = ctx->block_align;
	si.aviHeader.dwScale = ctx->frame_size;
	si.aviHeader.dwRate = ctx->sample_rate;
}

int VDFFAudio::SuggestFileFormat(const char* name)
{
	if (strcmp(name, "wav") == 0 || strcmp(name, "avi") == 0) {
		if (wav_compatible) return vd2::kFormat_Good; else return vd2::kFormat_Reject;
	}
	return vd2::kFormat_Unknown;
}

bool VDFFAudio::Convert(bool flush, bool requireOutput)
{
	if (pkt->size) return true;

	if (in_pos >= frame_size || (in_pos > 0 && flush)) {
		const uint8_t* src[] = { in_buf };
		int done_swr = swr_convert(swr, sample_buf, frame_size, src, in_pos);

		frame->pts += frame->nb_samples;
		frame->nb_samples = in_pos;
		av_samples_fill_arrays(frame->data, frame->linesize, sample_buf[0], ctx->ch_layout.nb_channels, frame_size, ctx->sample_fmt, 0);
		avcodec_send_frame(ctx, frame);

		in_pos = 0;

	}
	else if (flush) {
		avcodec_send_frame(ctx, 0);
	}

	av_packet_unref(pkt);
	avcodec_receive_packet(ctx, pkt);
	for (int i = 0; i < pkt->side_data_elems; i++) {
		AVPacketSideData& s = pkt->side_data[i];
		if (s.type == AV_PKT_DATA_NEW_EXTRADATA) export_wav();
	}
	if (pkt->size > max_packet) max_packet = pkt->size;
	if (flush) export_wav();

	if (pkt->size) return true;

	return false;
}

unsigned VDFFAudio::GetInputLevel() const
{
	return in_pos * src_linesize;
}

unsigned VDFFAudio::GetInputSpace() const
{
	return (frame_size - in_pos) * src_linesize;
}

unsigned VDFFAudio::GetOutputLevel() const
{
	return pkt->size;
}

void* VDFFAudio::LockInputBuffer(unsigned& bytes)
{
	bytes = (frame_size - in_pos) * src_linesize;
	return in_buf + in_pos * src_linesize;
}

void VDFFAudio::UnlockInputBuffer(unsigned bytes)
{
	in_pos += bytes / src_linesize;
	total_in += bytes / src_linesize;
}

const void* VDFFAudio::LockOutputBuffer(unsigned& bytes)
{
	return 0;
}

void VDFFAudio::UnlockOutputBuffer(unsigned bytes)
{
}

unsigned VDFFAudio::CopyOutput(void* dst, unsigned bytes, sint64& duration)
{
	// output is delayed, need more input
	if (!pkt->size) {
		duration = -1;
		return 0;
	}

	// host must resize buffer
	if (int(bytes) < pkt->size) {
		duration = pkt->duration;
		return pkt->size;
	}

	total_out += pkt->duration;
	duration = pkt->duration;
	bytes = pkt->size;
	memcpy(dst, pkt->data, bytes);
	av_packet_unref(pkt);
	return bytes;
}

//-----------------------------------------------------------------------------------

class AConfigBase : public VDXVideoFilterDialog
{
public:
	VDFFAudio* codec = nullptr;
	std::unique_ptr<uint8_t[]> old_param;

	int dialog_id    = -1;

	virtual ~AConfigBase()
	{
	}

	void Show(HWND parent, VDFFAudio* codec)
	{
		this->codec = codec;
		size_t rsize = codec->GetConfigSize();
		old_param.reset(new uint8_t[rsize]);
		memcpy(old_param.get(), codec->config, rsize);
		VDXVideoFilterDialog::Show(hInstance, MAKEINTRESOURCEW(dialog_id), parent);
	}

	virtual INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
};

INT_PTR AConfigBase::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
	{
		SetDlgItemTextA(mhdlg, IDC_ENCODER_LABEL, LIBAVCODEC_IDENT);
		return TRUE;
	}

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			EndDialog(mhdlg, TRUE);
			return TRUE;

		case IDCANCEL:
			memcpy(codec->config, old_param.get(), codec->GetConfigSize());
			EndDialog(mhdlg, FALSE);
			return TRUE;
		}
	}

	return FALSE;
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
		ctx->bit_rate = config->bitrate * 1000 * ctx->ch_layout.nb_channels;
	}
}

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
	SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMIN, FALSE, 32);
	SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMAX, TRUE, 288);
	SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETPOS, TRUE, codec_config->bitrate);
	wchar_t buf[80];
	swprintf_s(buf, L"%d k", codec_config->bitrate);
	SetDlgItemTextW(mhdlg, IDC_ENC_QUALITY_VALUE, buf);
	SetDlgItemTextW(mhdlg, IDC_ENC_QUALITY_LABEL, L"Bitrate/channel");
}

void AConfigAAC::change_quality()
{
	int x = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_GETPOS, 0, 0);
	codec_config->bitrate = x & ~15;
	wchar_t buf[80];
	swprintf_s(buf, L"%d k", codec_config->bitrate);
	SetDlgItemTextW(mhdlg, IDC_ENC_QUALITY_VALUE, buf);
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
		if ((HWND)lParam == GetDlgItem(mhdlg, IDC_ENC_QUALITY)) {
			change_quality();
			break;
		}
		return FALSE;
	}
	return AConfigBase::DlgProc(msg, wParam, lParam);
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
	if (!p) return false;
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

//-----------------------------------------------------------------------------------

int mp3_bitrate[] = { 8, 16, 24, 32, 40, 48, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 };

void VDFFAudio_mp3::reset_config()
{
	codec_config.clear();
	codec_config.bitrate = 320;
}

void VDFFAudio_mp3::CreateCodec()
{
	codec = avcodec_find_encoder(AV_CODEC_ID_MP3);
}

void VDFFAudio_mp3::InitContext()
{
	VDFFAudio::InitContext();
	av_opt_set_int(ctx->priv_data, "joint_stereo", codec_config.flags & flag_jointstereo, 0);

	// this estimate is fake, but leaving bit_rate=0 is worse
	if (!(codec_config.flags & VDFFAudio::flag_constant_rate)) ctx->bit_rate = ctx->sample_rate * 4;
}

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
	if (codec_config->flags & VDFFAudio::flag_constant_rate) {
		int x = 0;
		for (; x < std::size(mp3_bitrate); x++) {
			if (mp3_bitrate[x] == codec_config->bitrate) {
				break;
			}
		}

		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMIN, FALSE, 0);
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMAX, TRUE, std::size(mp3_bitrate) - 1);
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETPOS, TRUE, x);
		wchar_t buf[80];
		swprintf_s(buf, L"%d k", codec_config->bitrate);
		SetDlgItemTextW(mhdlg, IDC_ENC_QUALITY_VALUE, buf);
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
	if (codec_config->flags & VDFFAudio::flag_constant_rate) {
		codec_config->bitrate = mp3_bitrate[x];
		wchar_t buf[80];
		swprintf_s(buf, L"%d k", codec_config->bitrate);
		SetDlgItemTextW(mhdlg, IDC_ENC_QUALITY_VALUE, buf);
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
		CheckDlgButton(mhdlg, IDC_ENC_JOINT_STEREO, codec_config->flags & VDFFAudio_mp3::flag_jointstereo);
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
		case IDC_ENC_JOINT_STEREO:
			codec_config->flags &= ~VDFFAudio_mp3::flag_jointstereo;
			if (IsDlgButtonChecked(mhdlg, IDC_ENC_JOINT_STEREO))
				codec_config->flags |= VDFFAudio_mp3::flag_jointstereo;
			break;
		case IDC_ENC_CBR:
			codec_config->flags &= ~VDFFAudio::flag_constant_rate;
			if (IsDlgButtonChecked(mhdlg, IDC_ENC_CBR))
				codec_config->flags |= VDFFAudio::flag_constant_rate;
			init_quality();
			break;
		}
	}
	return AConfigBase::DlgProc(msg, wParam, lParam);
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
	if (!p) return false;
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
	ctx->compression_level = codec_config.quality;
	av_opt_set_int(ctx->priv_data, "ch_mode", (codec_config.flags & flag_jointstereo) ? -1 : 0, 0);
}

//-----------------------------------------------------------------------------------

bool VDXAPIENTRY ff_create_flacenc(const VDXInputDriverContext* pContext, IVDXAudioEnc** ppDriver)
{
	VDFFAudio* p = new VDFFAudio_flac(*pContext);
	if (!p) return false;
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
			if (IsDlgButtonChecked(mhdlg, IDC_ENC_JOINT_STEREO))
				codec_config->flags |= VDFFAudio_flac::flag_jointstereo;
			break;
		}
	}
	return AConfigBase::DlgProc(msg, wParam, lParam);
}

void VDFFAudio_flac::ShowConfig(VDXHWND parent)
{
	AConfigFlac cfg;
	cfg.Show((HWND)parent, this);
}

//-----------------------------------------------------------------------------------

void VDFFAudio_alac::reset_config()
{
	codec_config.clear();
	codec_config.version = 1;
	codec_config.bitrate = 0;
	codec_config.quality = 2;
}

void VDFFAudio_alac::CreateCodec()
{
	codec = avcodec_find_encoder(AV_CODEC_ID_ALAC);
}

void VDFFAudio_alac::InitContext()
{
	ctx->compression_level = codec_config.quality;
}

//-----------------------------------------------------------------------------------

bool VDXAPIENTRY ff_create_alacenc(const VDXInputDriverContext* pContext, IVDXAudioEnc** ppDriver)
{
	VDFFAudio* p = new VDFFAudio_alac(*pContext);
	if (!p) return false;
	*ppDriver = p;
	p->AddRef();
	return true;
}

VDXAudioEncDefinition ff_alacenc = {
	sizeof(VDXAudioEncDefinition),
	0, //flags
	L"FFmpeg ALAC (Apple Lossless)",
	"ffmpeg_alac",
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


class AConfigAlac : public AConfigBase
{
public:
	VDFFAudio_alac::Config* codec_config = nullptr;

	AConfigAlac() { dialog_id = IDD_ENC_ALAC; }
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void init_quality();
	void change_quality();
};

void AConfigAlac::init_quality()
{
	SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMIN, FALSE, 0);
	SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMAX, TRUE, 2);
	SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETPOS, TRUE, codec_config->quality);
	SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, codec_config->quality, false);
}

void AConfigAlac::change_quality()
{
	int x = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_GETPOS, 0, 0);
	codec_config->quality = x;
	SetDlgItemInt(mhdlg, IDC_ENC_QUALITY_VALUE, codec_config->quality, false);
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

void VDFFAudio_alac::ShowConfig(VDXHWND parent)
{
	AConfigAlac cfg;
	cfg.Show((HWND)parent, this);
}

//-----------------------------------------------------------------------------------

void VDFFAudio_vorbis::reset_config()
{
	codec_config.clear();
	codec_config.version = 1;
	codec_config.flags = 0;
	codec_config.bitrate = 160;
	codec_config.quality = 500;
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
		ctx->bit_rate = config->bitrate * 1000 * ctx->ch_layout.nb_channels;
		ctx->rc_min_rate = ctx->bit_rate;
		ctx->rc_max_rate = ctx->bit_rate;
	}
	else {
		ctx->flags |= AV_CODEC_FLAG_QSCALE;
		ctx->global_quality = (FF_QP2LAMBDA * config->quality + 50) / 100;
		ctx->bit_rate = -1;
		ctx->rc_min_rate = -1;
		ctx->rc_max_rate = -1;
	}
}

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
		wchar_t buf[80];
		swprintf_s(buf, L"%dk", codec_config->bitrate);
		SetDlgItemTextW(mhdlg, IDC_ENC_QUALITY_VALUE, buf);
		SetDlgItemTextW(mhdlg, IDC_ENC_QUALITY_LABEL, L"Bitrate");
	}
	else {
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMIN, FALSE, 0);
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETRANGEMAX, TRUE, 110);
		SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_SETPOS, TRUE, (100 - codec_config->quality / 10));
		wchar_t buf[80];
		swprintf_s(buf, L"%1.1f", codec_config->quality * 0.01);
		SetDlgItemTextW(mhdlg, IDC_ENC_QUALITY_VALUE, buf);
		SetDlgItemTextW(mhdlg, IDC_ENC_QUALITY_LABEL, L"Quality (high-low)");
	}
}

void AConfigVorbis::change_quality()
{
	int x = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_QUALITY, TBM_GETPOS, 0, 0);
	if (codec_config->flags & VDFFAudio::flag_constant_rate) {
		codec_config->bitrate = x;
		wchar_t buf[80];
		swprintf_s(buf, L"%dk", codec_config->bitrate);
		SetDlgItemTextW(mhdlg, IDC_ENC_QUALITY_VALUE, buf);
	}
	else {
		codec_config->quality = (100 - x) * 10;
		wchar_t buf[80];
		swprintf_s(buf, L"%1.1f", codec_config->quality * 0.01);
		SetDlgItemTextW(mhdlg, IDC_ENC_QUALITY_VALUE, buf);
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
			if (IsDlgButtonChecked(mhdlg, IDC_ENC_CBR))
				codec_config->flags |= VDFFAudio::flag_constant_rate;
			init_quality();
			break;
		}
	}
	return AConfigBase::DlgProc(msg, wParam, lParam);
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
	if (!p) return false;
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

//-----------------------------------------------------------------------------------

void VDFFAudio_opus::reset_config()
{
	codec_config.clear();
	codec_config.bitrate = 64000;
	codec_config.quality = 10;
}

void VDFFAudio_opus::CreateCodec()
{
	codec = avcodec_find_encoder_by_name("libopus");
}

void VDFFAudio_opus::InitContext()
{
	ctx->bit_rate = config->bitrate * ctx->ch_layout.nb_channels;
	ctx->compression_level = config->quality;
	if (config->flags & flag_constant_rate)
		av_opt_set_int(ctx->priv_data, "vbr", 0, 0);
	else if (config->flags & flag_limited_rate)
		av_opt_set_int(ctx->priv_data, "vbr", 2, 0);
	else
		av_opt_set_int(ctx->priv_data, "vbr", 1, 0);
}

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
	SendDlgItemMessageW(mhdlg, IDC_ENC_BITRATE, TBM_SETRANGEMIN, FALSE, 500);
	SendDlgItemMessageW(mhdlg, IDC_ENC_BITRATE, TBM_SETRANGEMAX, TRUE, 256000);
	SendDlgItemMessageW(mhdlg, IDC_ENC_BITRATE, TBM_SETPOS, TRUE, codec_config->bitrate);
	SetDlgItemInt(mhdlg, IDC_ENC_BITRATE_VALUE, codec_config->bitrate, false);

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
	SetDlgItemInt(mhdlg, IDC_ENC_BITRATE_VALUE, codec_config->bitrate, false);
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
			if (IsDlgButtonChecked(mhdlg, IDC_ENC_CBR))
				codec_config->flags |= VDFFAudio::flag_constant_rate;
			init_flags();
			break;
		case IDC_ENC_ABR:
			codec_config->flags &= ~VDFFAudio::flag_constant_rate;
			codec_config->flags &= ~VDFFAudio_opus::flag_limited_rate;
			if (IsDlgButtonChecked(mhdlg, IDC_ENC_ABR))
				codec_config->flags |= VDFFAudio_opus::flag_limited_rate;
			init_flags();
			break;
		}
	}
	return AConfigBase::DlgProc(msg, wParam, lParam);
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
	if (!p) return false;
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
