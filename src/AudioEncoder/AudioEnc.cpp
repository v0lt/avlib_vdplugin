/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stdafx.h"

#include "AudioEnc.h"

#include <cassert>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}
#include "../Helper.h"
#include "../resource.h"
#include "../iobuffer.h"

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
	if (avctx) {
		av_freep(&avctx->extradata);
		avcodec_free_context(&avctx);
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
	if (size != rsize) {
		return;
	}
	int src_version = *(int*)data;
	if (src_version != config->version) {
		return;
	}
	memcpy(config, data, rsize);
	return;
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
	AVIOContext* avio_ctx = avio_alloc_context((unsigned char*)buf, buf_size, 1, &io, nullptr, &IOWBuffer::Write, &IOWBuffer::Seek);
	AVFormatContext* ofmt = avformat_alloc_context();
	ofmt->pb = avio_ctx;
	ofmt->oformat = av_guess_format("wav", nullptr, nullptr);
	AVStream* st = avformat_new_stream(ofmt, nullptr);
	st->time_base = av_make_q(1, avctx->sample_rate);
	avcodec_parameters_from_context(st->codecpar, avctx);
	if (avformat_write_header(ofmt, nullptr) < 0) {
		goto cleanup;
	}
	if (av_write_trailer(ofmt) < 0) {
		goto cleanup;
	}

	{
		out_format_size = *(int*)(io.data + 16);
		WAVEFORMATEXTENSIBLE* ff = (WAVEFORMATEXTENSIBLE*)(io.data + 20);
		out_format = (WAVEFORMATEXTENSIBLE*)malloc(out_format_size);
		memcpy(out_format, ff, out_format_size);
		wav_compatible = true;

		if (codec->id == AV_CODEC_ID_AAC) {
			// is this ffmpeg' job to calculate right size?
			if (max_packet > out_format->Format.nBlockAlign) {
				out_format->Format.nBlockAlign = max_packet;
			}
		}
	}

cleanup:
	av_free(avio_ctx->buffer);
	av_free(avio_ctx);
	avformat_free_context(ofmt);

	if (!out_format) {
		// make our private descriptor
		out_format_size = sizeof(WAVEFORMATEX_VDFF) + avctx->extradata_size;
		out_format = (WAVEFORMATEXTENSIBLE*)malloc(out_format_size);
		out_format->Format.wFormatTag        = WAVE_FORMAT_EXTENSIBLE;
		out_format->Format.nChannels         = avctx->ch_layout.nb_channels;
		out_format->Format.nSamplesPerSec    = avctx->sample_rate;
		out_format->Format.nAvgBytesPerSec   = (DWORD)(avctx->bit_rate / 8);
		out_format->Format.nBlockAlign       = avctx->block_align;
		out_format->Format.wBitsPerSample    = avctx->bits_per_coded_sample;
		out_format->Format.cbSize            = WORD(out_format_size - sizeof(WAVEFORMATEX));
		out_format->Samples.wSamplesPerBlock = avctx->frame_size;
		out_format->dwChannelMask            = 0;
		out_format->SubFormat                = KSDATAFORMAT_SUBTYPE_VDFF;

		WAVEFORMATEX_VDFF* ff = (WAVEFORMATEX_VDFF*)out_format;
		ff->codec_id = avctx->codec_id;
		if (avctx->extradata_size) {
			char* p = (char*)(ff + 1);
			memcpy(p, avctx->extradata, avctx->extradata_size);
		}
	}
}

void VDFFAudio::select_fmt(const AVSampleFormat* list)
{
	int best_pos = -1;

	const enum AVSampleFormat* sample_fmts = nullptr;
	int ret = avcodec_get_supported_config(avctx, codec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, (const void**)&sample_fmts, nullptr);
	if (ret < 0) {
		return;
	}
	if (!sample_fmts) {
		assert(0); // unsupported but valid value.
		return;
	}

	for (int y = 0; ; y++) {
		AVSampleFormat yfmt = sample_fmts[y];
		if (yfmt == AV_SAMPLE_FMT_NONE) {
			break;
		}

		for (int x = 0; ; x++) {
			AVSampleFormat xfmt = list[x];
			if (xfmt == AV_SAMPLE_FMT_NONE) {
				break;
			}
			if (xfmt == yfmt) {
				if (best_pos == -1 || x < best_pos) {
					best_pos = x;
					avctx->sample_fmt = yfmt;
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

	avctx = avcodec_alloc_context3(codec);

	av_channel_layout_default(&avctx->ch_layout, format->mChannels);
	avctx->sample_rate = format->mSamplesPerSec;

	const enum AVSampleFormat* sample_fmts = nullptr;
	int ret = avcodec_get_supported_config(avctx, codec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, (const void**)&sample_fmts, nullptr);
	if (ret < 0) {
		return;
	}
	if (!sample_fmts) {
		assert(0); // unsupported but valid value.
		return;
	}
	avctx->sample_fmt = sample_fmts[0]; // hmm

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
			av_channel_layout_from_mask(&avctx->ch_layout, fx->dwChannelMask);
		}
		if (fx->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
			in_fmt = AV_SAMPLE_FMT_FLT;
		}
	}

	if (in_fmt == AV_SAMPLE_FMT_FLT) {
		const AVSampleFormat list[] = {
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
		const AVSampleFormat list[] = {
			AV_SAMPLE_FMT_S16,
			AV_SAMPLE_FMT_S16P,
			AV_SAMPLE_FMT_FLT,
			AV_SAMPLE_FMT_FLTP,
			AV_SAMPLE_FMT_NONE
		};
		select_fmt(list);
	}

	//if(ofmt->oformat->flags & AVFMT_GLOBALHEADER)
	//	avctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	avctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	InitContext();

	ret = avcodec_open2(avctx, codec, nullptr);
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

	ret = av_opt_set_chlayout(swr, "in_chlayout", &avctx->ch_layout, 0);
	ret = av_opt_set_int(swr, "in_sample_rate", avctx->sample_rate, 0);
	ret = av_opt_set_sample_fmt(swr, "in_sample_fmt", in_fmt, 0);

	ret = av_opt_set_chlayout(swr, "out_chlayout", &avctx->ch_layout, 0);
	ret = av_opt_set_int(swr, "out_sample_rate", avctx->sample_rate, 0);
	ret = av_opt_set_sample_fmt(swr, "out_sample_fmt", avctx->sample_fmt, 0);

	ret = swr_init(swr);
	if (ret < 0) {
		errstr = AVError2Str(ret);
		mContext.mpCallbacks->SetError("FFMPEG: Audio resampler error (%s).", errstr.c_str());
		cleanup();
		return;
	}

	frame = av_frame_alloc();
	av_channel_layout_copy(&frame->ch_layout, &avctx->ch_layout);
	frame->format = avctx->sample_fmt;
	frame->sample_rate = avctx->sample_rate;
	frame->pts = 0;
	frame->nb_samples = 0;

	frame_size = avctx->frame_size;
	sample_buf = (uint8_t**)malloc(avctx->ch_layout.nb_channels * sizeof(void*));
	av_samples_alloc(sample_buf, 0, avctx->ch_layout.nb_channels, frame_size, avctx->sample_fmt, 0);

	av_samples_get_buffer_size(&src_linesize, avctx->ch_layout.nb_channels, 1, in_fmt, 1);
	in_buf = (uint8_t*)malloc(src_linesize * frame_size);
}

void VDFFAudio::GetStreamInfo(VDXStreamInfo& si) const
{
	si.avcodec_version = LIBAVCODEC_VERSION_INT;
	si.block_align = avctx->block_align;
	si.frame_size = avctx->frame_size;
	si.initial_padding = avctx->initial_padding;
	si.trailing_padding = avctx->trailing_padding;

	// proof: ff_parse_specific_params
	// au_rate = par->sample_rate
	// au_scale = par->frame_size (depends on codec but here it must be known)
	// au_ssize = par->block_align

	si.aviHeader.dwSampleSize = avctx->block_align;
	si.aviHeader.dwScale = avctx->frame_size;
	si.aviHeader.dwRate = avctx->sample_rate;
}

int VDFFAudio::SuggestFileFormat(const char* name)
{
	if (strcmp(name, "wav") == 0 || strcmp(name, "avi") == 0) {
		if (wav_compatible) {
			return vd2::kFormat_Good;
		} else {
			return vd2::kFormat_Reject;
		}
	}
	return vd2::kFormat_Unknown;
}

bool VDFFAudio::Convert(bool flush, bool requireOutput)
{
	if (pkt->size) {
		return true;
	}

	if (in_pos >= frame_size || (in_pos > 0 && flush)) {
		const uint8_t* src[] = { in_buf };
		int done_swr = swr_convert(swr, sample_buf, frame_size, src, in_pos);

		frame->pts += frame->nb_samples;
		frame->nb_samples = in_pos;
		av_samples_fill_arrays(frame->data, frame->linesize, sample_buf[0], avctx->ch_layout.nb_channels, frame_size, avctx->sample_fmt, 0);
		avcodec_send_frame(avctx, frame);

		in_pos = 0;

	}
	else if (flush) {
		avcodec_send_frame(avctx, nullptr);
	}

	av_packet_unref(pkt);
	avcodec_receive_packet(avctx, pkt);
	for (int i = 0; i < pkt->side_data_elems; i++) {
		AVPacketSideData& s = pkt->side_data[i];
		if (s.type == AV_PKT_DATA_NEW_EXTRADATA) {
			export_wav();
		}
	}
	if (pkt->size > max_packet) {
		max_packet = pkt->size;
	}
	if (flush) {
		export_wav();
	}

	if (pkt->size) {
		return true;
	}

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

void AConfigBase::Show(HWND parent, VDFFAudio* codec)
{
	this->codec = codec;
	size_t rsize = codec->GetConfigSize();
	old_param.reset(new uint8_t[rsize]);
	memcpy(old_param.get(), codec->config, rsize);
	VDXVideoFilterDialog::Show(hInstance, MAKEINTRESOURCEW(dialog_id), parent);
}

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
			codec->save_config();
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
