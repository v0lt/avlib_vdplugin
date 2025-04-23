/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <vd2/plugin/vdinputdriver.h>
#include <vd2/VDXFrame/Unknown.h>
#include <vector>
#include <mmreg.h>
#include "stdint.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/opt.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

class VDFFInputFile;

class VDFFAudioSource : public vdxunknown<IVDXStreamSource>, public IVDXAudioSource
{
public:
	VDFFAudioSource(const VDXInputDriverContext& context);
	~VDFFAudioSource();

	int VDXAPIENTRY AddRef() override;
	int VDXAPIENTRY Release() override;
	void* VDXAPIENTRY AsInterface(uint32_t iid) override;

	void VDXAPIENTRY GetStreamSourceInfo(VDXStreamSourceInfo& srcInfo) override { srcInfo = m_streamInfo; }
	bool VDXAPIENTRY Read(int64_t lStart, uint32_t lCount, void* lpBuffer, uint32_t cbBuffer, uint32_t* lBytesRead, uint32_t* lSamplesRead) override;

	const void* VDXAPIENTRY GetDirectFormat() override { return &mRawFormat; }
	int  VDXAPIENTRY GetDirectFormatLen() override { return sizeof(WAVEFORMATEXTENSIBLE); }
	void VDXAPIENTRY SetTargetFormat(const VDXWAVEFORMATEX* format) override;

	ErrorMode VDXAPIENTRY GetDecodeErrorMode() override { return IVDXStreamSource::kErrorModeReportAll; }
	void VDXAPIENTRY SetDecodeErrorMode(ErrorMode mode) override {}
	bool VDXAPIENTRY IsDecodeErrorModeSupported(ErrorMode mode) override { return mode == IVDXStreamSource::kErrorModeReportAll; }

	bool VDXAPIENTRY IsVBR() override { return false; }
	int64_t VDXAPIENTRY TimeToPositionVBR(int64_t us) override { return 0; }
	int64_t VDXAPIENTRY PositionToTimeVBR(int64_t samples) override { return 0; }

	void VDXAPIENTRY GetAudioSourceInfo(VDXAudioSourceInfo& info) override { info.mFlags = 0; }

private:
	const VDXInputDriverContext& mContext;
	WAVEFORMATEXTENSIBLE mRawFormat = {};

	VDFFInputFile* m_pSource = nullptr;
	AVRational time_base = {};
	int64_t start_time   = 0;
	int64_t time_adjust  = 0;

	AVFormatContext* m_pFormatCtx = nullptr;
public:
	AVStream*       m_pStream   = nullptr;
	AVCodecContext* m_pCodecCtx = nullptr;
	VDXStreamSourceInfo m_streamInfo = {};
	int m_streamIndex    = 0;
	int64_t sample_count = 0;
private:
	SwrContext*      m_pSwrCtx    = nullptr;
	AVFrame*         m_pFrame     = nullptr;
	int src_linesize       = 0;
	uint64_t out_layout    = 0;
	AVSampleFormat out_fmt = AV_SAMPLE_FMT_NONE;
	uint64_t swr_layout    = 0;
	int swr_rate           = 0;
	AVSampleFormat swr_fmt = AV_SAMPLE_FMT_NONE;

	struct BufferPage {
		enum { size = 0x8000 }; // max usable value 0xFFFF

		uint16_t a0 = 0, a1 = 0; // range a
		uint16_t b0 = 0, b1 = 0; // range b
		uint8_t* aud_data = nullptr;

		int copy(int s0, uint32_t count, void* dst, int sample_size);
		int alloc(int s0, uint32_t count, int& changed);
		int empty(int s0, uint32_t count);
	};

	std::vector<BufferPage> buffer;
	int used_pages     = 0;
	int used_pages_max = 0;
	int first_page     = 0;
	int last_page      = 0;

	int64_t next_sample  = 0;
	int64_t first_sample = AV_NOPTS_VALUE;
	int discard_samples  = 0;
	bool trust_sample_pos = false;;
	bool use_keys = false;

	struct ReadInfo {
		int64_t first_sample = -1;
		int64_t last_sample  = -1;
	};

public:
	int initStream(VDFFInputFile* pSource, int streamIndex);
	AVFormatContext* OpenAudioFile(std::wstring_view path, int streamIndex);
private:
	void init_start_time();
	int read_packet(AVPacket* pkt, ReadInfo& ri);
	void insert_silence(int64_t start, uint32_t count);
	void write_silence(void* dst, uint32_t count);
	void invalidate(int64_t start, uint32_t count);
	void alloc_page(int i);
	void reset_cache();
	int reset_swr();
	int64_t frame_to_pts(sint64 start, AVStream* video);
};
