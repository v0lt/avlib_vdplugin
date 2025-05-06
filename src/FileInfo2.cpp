/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "FileInfo2.h"
#include "InputFile2.h"
#include "VideoSource2.h"
#include "AudioSource2.h"
#include "resource.h"
#include "gopro.h"
#include "Utils/StringUtil.h"
#include "Helper.h"


static const char* vsnstr = "Version 2.1";

extern HINSTANCE hInstance;

bool VDFFInputFileInfoDialog::Show(VDXHWND parent, VDFFInputFile* pInput)
{
	source = pInput;
	segment_pos = 0;
	segment_count = 1;
	VDFFInputFile* f = pInput;
	while (1) {
		f = f->next_segment;
		if (!f) break;
		segment_count++;
	}

	/*
	pFormatCtx = pInput->getContext();
	int vs = pInput->find_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO);
	if(vs!=-1) pVideoStream = pFormatCtx->streams[vs];
	int as = pInput->find_stream(pFormatCtx, AVMEDIA_TYPE_AUDIO);
	if(as!=-1) pAudioStream = pFormatCtx->streams[as];
	*/

	LRESULT ret = VDXVideoFilterDialog::Show(hInstance, MAKEINTRESOURCEW(IDD_FF_INFO), (HWND)parent);
	return ret != 0;
}

void VDFFInputFileInfoDialog::load_segment()
{
	int pos = 0;
	int start_frame = 0;
	VDFFInputFile* f = source;
	while (1) {
		if (pos == segment_pos) {
			break;
		}
		if (f->video_source) {
			start_frame += f->video_source->m_sample_count;
		}
		f = f->next_segment;
		pos++;
	}

	segment = f;

	const wchar_t* p0 = wcsrchr(segment->m_path.c_str(), '\\');
	const wchar_t* p1 = wcsrchr(segment->m_path.c_str(), '/');
	const wchar_t* name = segment->m_path.c_str();
	if (p0 > name) name = p0 + 1;
	if (p1 > name) name = p1 + 1;

	std::wstring str;

	str = std::format(L"Segment {} of {}: {}", segment_pos + 1, segment_count, name);
	SetDlgItemTextW(mhdlg, IDC_SEGMENT, str.c_str());

	int end_frame = start_frame;
	if (segment->video_source) {
		end_frame += segment->video_source->m_sample_count - 1;
	}
	str = std::format(L"Timeline: frame {} to {}", start_frame, end_frame);
	SetDlgItemTextW(mhdlg, IDC_SEGMENT_TIMELINE, str.c_str());

	HWND prev = GetDlgItem(mhdlg, IDC_SEGMENT_PREV);
	HWND next = GetDlgItem(mhdlg, IDC_SEGMENT_NEXT);
	bool bprev = segment_pos > 0;
	bool bnext = segment_pos < segment_count - 1;
	EnableWindow(prev, TRUE);
	EnableWindow(next, TRUE);
	if (GetFocus() == prev && !bprev) {
		SetFocus(next);
	}
	if (GetFocus() == next && !bnext) {
		SetFocus(prev);
	}
	EnableWindow(prev, bprev);
	EnableWindow(next, bnext);
	SendMessageW(prev, BM_SETSTYLE, (WPARAM)BS_FLAT, TRUE);
	SendMessageW(next, BM_SETSTYLE, (WPARAM)BS_FLAT, TRUE);

	print_format();
	print_metadata();

	if (segment->video_source) {
		print_video();
	}
	if (segment->audio_source) {
		print_audio();
	}
}

INT_PTR VDFFInputFileInfoDialog::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
		load_segment();
		print_performance();
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			EndDialog(mhdlg, TRUE);
			return TRUE;

		case IDCANCEL:
			EndDialog(mhdlg, FALSE);
			return TRUE;

		case IDC_SEGMENT_PREV:
			if (segment_pos > 0) {
				segment_pos--;
				load_segment();
			}
			return TRUE;

		case IDC_SEGMENT_NEXT:
			if (segment_pos < segment_count - 1) {
				segment_pos++;
				load_segment();
			}
			return TRUE;

		}
	}

	return FALSE;
}

void VDFFInputFileInfoDialog::print_format()
{
	AVFormatContext* pFormatCtx = segment->getContext();
	const AVInputFormat* pInputFormat = pFormatCtx->iformat;

	SetDlgItemTextA(mhdlg, IDC_STATICVerNumber, vsnstr);
	std::wstring str;

	SetDlgItemTextA(mhdlg, IDC_FORMATNAME, pInputFormat->long_name);

	if (segment->is_image) {
		int n = segment->video_source->m_sample_count;
		if (n > 1) {
			str = std::format(L"{} images", n);
			SetDlgItemTextW(mhdlg, IDC_DURATION, str.c_str());
		}
		else {
			SetDlgItemTextW(mhdlg, IDC_DURATION, L"single image");
		}
	}
	else {
		double seconds = 0;
		if (segment->is_anim_image) {
			VDXFraction fr = segment->video_source->m_streamInfo.mInfo.mSampleRate;
			seconds = double(segment->video_source->m_sample_count) * fr.mDenominator / fr.mNumerator;
		}
		else {
			if (pFormatCtx->duration == AV_NOPTS_VALUE)
				seconds = -1;
			else
				seconds = pFormatCtx->duration / (double)AV_TIME_BASE;
		}

		if (seconds == -1) {
			SetDlgItemTextW(mhdlg, IDC_DURATION, L"unknown");
		}
		else {
			int hours = (int)(seconds / 3600);
			seconds -= (hours * 3600);
			int minutes = (int)(seconds / 60);
			seconds -= (minutes * 60);

			str = std::format(L"{} h : {} min : {:.2f} sec", hours, minutes, seconds);
			SetDlgItemTextW(mhdlg, IDC_DURATION, str.c_str());
		}
	}

	str = std::format(L"{} kb/sec", pFormatCtx->bit_rate / 1000);
	SetDlgItemTextW(mhdlg, IDC_BITRATE, str.c_str());

	str = std::to_wstring(pFormatCtx->nb_streams);
	SetDlgItemTextW(mhdlg, IDC_STREAMSCOUNT, str.c_str());
}

void VDFFInputFileInfoDialog::print_video()
{
	AVCodecContext* pVideoCtx = segment->video_source->m_pCodecCtx;
	if (!pVideoCtx) {
		return;
	}
	AVStream* pVideoStream = segment->video_source->m_pStream;
	AVCodecParameters* codecpar = pVideoStream->codecpar;


	const AVCodec* pCodec = avcodec_find_decoder(codecpar->codec_id);
	const char* codec_name = "N/A";
	if (pCodec) {
		codec_name = pCodec->name;
	} else if (codecpar->codec_id == AV_CODEC_ID_MPEG2TS) {
		codec_name = "mpeg2ts";
	}

	SetDlgItemTextA(mhdlg, IDC_VIDEO_CODECNAME, codec_name);

	if (pVideoCtx->pix_fmt != AV_PIX_FMT_NONE)
	{
		char bufA[256];
		strcpy_s(bufA, av_get_pix_fmt_name(pVideoCtx->pix_fmt));
		const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(pVideoCtx->pix_fmt);
		bool is_rgb = (desc->flags & AV_PIX_FMT_FLAG_RGB) != 0;

		if (!is_rgb) {
			const char* spc = "?";
			const char* r = nullptr;
			if (pVideoCtx->colorspace == AVCOL_SPC_UNSPECIFIED) spc = nullptr;
			if (pVideoCtx->colorspace == AVCOL_SPC_BT709) spc = "bt.709";
			if (pVideoCtx->colorspace == AVCOL_SPC_BT470BG) spc = "bt.601";
			if (pVideoCtx->colorspace == AVCOL_SPC_SMPTE170M) spc = "bt.601";
			if (pVideoCtx->colorspace == AVCOL_SPC_SMPTE240M) spc = "bt.601";
			if (pVideoCtx->colorspace == AVCOL_SPC_FCC) spc = "FCC";
			if (pVideoCtx->colorspace == AVCOL_SPC_YCOCG) spc = "YCoCg";
			if (pVideoCtx->colorspace == AVCOL_SPC_BT2020_NCL) spc = "bt.2020-ncl";
			if (pVideoCtx->colorspace == AVCOL_SPC_BT2020_CL) spc = "bt.2020-cl";
			if (pVideoCtx->color_range == AVCOL_RANGE_JPEG) r = "full";
			if (spc) {
				strcat_s(bufA, " (");
				strcat_s(bufA, spc);
				if (r) {
					strcat_s(bufA, ":");
					strcat_s(bufA, r);
				}
				strcat_s(bufA, ")");
			}
		}

		SetDlgItemTextA(mhdlg, IDC_VIDEO_PIXFMT, bufA);

	}
	else {
		SetDlgItemTextW(mhdlg, IDC_VIDEO_PIXFMT, L"N/A");
	}

	std::wstring str;

	if (segment->is_image) {
		str = std::format(L"{} x {}", pVideoCtx->width, pVideoCtx->height);
		SetDlgItemTextW(mhdlg, IDC_VIDEO_WXH, str.c_str());
	}
	else {
		VDXFraction fr = segment->video_source->m_streamInfo.mInfo.mSampleRate;
		str = std::format(L"{} x {}, {:.3f} fps", pVideoCtx->width, pVideoCtx->height, (double)fr.mNumerator / fr.mDenominator);
		if (segment->video_source->average_fr) {
			str.append(L" (average)");
		}
		SetDlgItemTextW(mhdlg, IDC_VIDEO_WXH, str.c_str());
	}

	AVRational ar = av_make_q(1, 1);
	if (pVideoStream->sample_aspect_ratio.num) {
		ar = pVideoStream->sample_aspect_ratio;
	}
	else if (pVideoCtx->sample_aspect_ratio.num) {
		ar = pVideoCtx->sample_aspect_ratio;
	}
	AVRational ar1;
	av_reduce(&ar1.num, &ar1.den, ar.num, ar.den, INT_MAX);
	str = std::format(L"{} : {}", ar1.num, ar1.den);
	SetDlgItemTextW(mhdlg, IDC_VIDEO_ASPECTRATIO, str.c_str());

	if (pVideoCtx->bit_rate) {
		str = std::format(L"{} kb/sec", pVideoCtx->bit_rate / 1000);
		SetDlgItemTextW(mhdlg, IDC_VIDEO_BITRATE, str.c_str());
	} else {
		SetDlgItemTextW(mhdlg, IDC_VIDEO_BITRATE, L"N/A");
	}
}

void VDFFInputFileInfoDialog::print_audio()
{
	AVCodecContext* pAudioCtx = segment->audio_source->m_pCodecCtx;
	if (!pAudioCtx) {
		return;
	}
	AVStream* pAudioStream = segment->audio_source->m_pStream;

	const AVCodec* pCodec = avcodec_find_decoder(pAudioCtx->codec_id);
	const char* codec_name = pCodec ? pCodec->name: "N/A";
	SetDlgItemTextA(mhdlg, IDC_AUDIO_CODECNAME, codec_name);

	std::string str(80, '\0');
	av_channel_layout_describe(&pAudioCtx->ch_layout, str.data(), str.length());
	str_truncate_after_null(str);
	str += std::format(" ({}), ", pAudioCtx->ch_layout.nb_channels);
	if (pAudioCtx->sample_fmt != AV_SAMPLE_FMT_NONE) {
		str.append(av_get_sample_fmt_name(pAudioCtx->sample_fmt));
	} else {
		str.append("N/A");
	}
	SetDlgItemTextA(mhdlg, IDC_AUDIO_CHANNELS, str.c_str());


	str = std::format("{} Hz", pAudioCtx->sample_rate);
	SetDlgItemTextA(mhdlg, IDC_AUDIO_SAMPLERATE, str.c_str());

	int bits_per_sample = av_get_bits_per_sample(pAudioCtx->codec_id);
	int64_t bit_rate = bits_per_sample ? pAudioCtx->sample_rate * pAudioCtx->ch_layout.nb_channels * bits_per_sample : pAudioCtx->bit_rate;
	if (bit_rate) {
		str = std::format("{} kb/sec", bit_rate / 1000);
		SetDlgItemTextA(mhdlg, IDC_AUDIO_BITRATE, str.c_str());
	} else {
		SetDlgItemTextA(mhdlg, IDC_AUDIO_BITRATE, "N/A");
	}
}

struct MetaInfo {
	bool creation_time;
};

bool skip_useless_meta(MetaInfo& info, AVDictionaryEntry* t)
{
	const char* useful_metas[] = {
		"major_brand",
		"minor_version",
		"compatible_brands",
		"title",
		"TITLE",
		"comment",
		"COMMENT",
		"encoder",
		"description",
		"language",
		"software",
	};
	for (const auto& meta : useful_metas) {
		if (strcmp(t->key, meta) == 0) {
			return false;
		}
	}

	if (strcmp(t->key, "creation_time") == 0) {
		if (!info.creation_time) {
			info.creation_time = true;
			return false;
		}
	}

	return true;
}

void VDFFInputFileInfoDialog::print_metadata()
{
	AVFormatContext* pFormatCtx = segment->m_pFormatCtx;
	AVStream* pVideoStream = nullptr;
	AVStream* pAudioStream = nullptr;
	if (segment->video_source) {
		pVideoStream = segment->video_source->m_pStream;
	}
	if (segment->audio_source) {
		pAudioStream = segment->audio_source->m_pStream;
	}

	std::wstring s;

	MetaInfo info = { 0 };
	bool bValueAnsi = false;
	
	auto ConverMetaValue = [&bValueAnsi](char* value)
	{
		return bValueAnsi ? ConvertAnsiToWide(value) : ConvertUtf8ToWide(value);
	};

	if (pFormatCtx && pFormatCtx->metadata) {
		bValueAnsi = (pFormatCtx->iformat == av_find_input_format("avi"));
		AVDictionaryEntry* t = nullptr;

		s += L"File:\r\n";
		while (t = av_dict_get(pFormatCtx->metadata, "", t, AV_DICT_IGNORE_SUFFIX)) {
			if (skip_useless_meta(info, t)) {
				continue;
			}
			s += L"    " + A2WStr(t->key) + L" = " + ConverMetaValue(t->value) + L"\r\n";
		}
	}
	if (pVideoStream && pVideoStream->metadata) {
		AVDictionaryEntry* t = nullptr;
		s += L"Video:\r\n";
		while (t = av_dict_get(pVideoStream->metadata, "", t, AV_DICT_IGNORE_SUFFIX)) {
			if (skip_useless_meta(info, t)) {
				continue;
			}
			s += L"    " + A2WStr(t->key) + L" = " + ConverMetaValue(t->value) + L"\r\n";
		}
	}
	if (pAudioStream && pAudioStream->metadata) {
		AVDictionaryEntry* t = nullptr;
		s += L"Audio:\r\n";
		while (t = av_dict_get(pAudioStream->metadata, "", t, AV_DICT_IGNORE_SUFFIX)) {
			if (skip_useless_meta(info, t)) {
				continue;
			}
			s += L"    " + A2WStr(t->key) + L" = " + ConverMetaValue(t->value) + L"\r\n";
		}
	}

	GoproInfo gi;
	gi.find_info(segment->m_path);

	if (gi.type) {
		s += L"GoPro info: " + A2WStr(gi.type->Name) + L"\r\n";
		s += L"    firmware = " + A2WStr(gi.firmware) + L"\r\n";
		s += L"    tserial# = " + A2WStr(gi.cam_serial) + L"\r\n";
	}

	int tab[2] = { 42,90 };
	SendDlgItemMessageW(mhdlg, IDC_METADATA, EM_SETTABSTOPS, 2, (LPARAM)tab);
	SetDlgItemTextW(mhdlg, IDC_METADATA, s.c_str());
}

void VDFFInputFileInfoDialog::print_performance()
{
	VDFFInputFile* f1 = source;

	int buf_count = 0;
	int buf_max = 0;
	int index_quality = 2;
	int decoded_count = 0;
	bool all_key = true;
	bool has_vfr = false;

	while (f1 && f1->video_source) {
		VDFFVideoSource* v1 = f1->video_source;
		buf_max += v1->buffer_reserve;
		for (size_t i = 0; i < v1->buffer.size(); i++) {
			if (v1->buffer[i].refs) {
				buf_count++;
			}
		}

		if (!v1->trust_index) {
			if (index_quality > 1) index_quality = 1;
			if (!v1->sparse_index) {
				if (index_quality > 0) index_quality = 0;
			}
		}
		if (v1->keyframe_gap != 1) all_key = false;
		if (v1->has_vfr) has_vfr = true;
		decoded_count += v1->decoded_count;

		f1 = f1->next_segment;
	}

	if (buf_max == 0) {
		SetDlgItemTextW(mhdlg, IDC_MEMORY_INFO, nullptr);
		SetDlgItemTextW(mhdlg, IDC_STATS, nullptr);
		SetDlgItemTextW(mhdlg, IDC_INDEX_INFO, nullptr);
		return;
	}

	int64_t mem_max = 0;
	if (source->video_source) {
		mem_max = source->video_source->frame_size;
	}
	mem_max *= buf_max;
	mem_max = (mem_max + 512 * 1024) / (1024 * 1024);

	int buf_used = (buf_count * 200 + buf_max) / (buf_max * 2);

	std::wstring str;

	str = std::format(L"Memory cache: {} frames / {}M reserved, {}% used", buf_max, mem_max, buf_used);
	SetDlgItemTextW(mhdlg, IDC_MEMORY_INFO, str.c_str());

	str = std::format(L"Frames decoded: {}", decoded_count);
	SetDlgItemTextW(mhdlg, IDC_STATS, str.c_str());

	if (segment->is_image) {
		SetDlgItemTextW(mhdlg, IDC_INDEX_INFO, L"Seeking: image list (random access)");
	}
	else {
		std::wstring msg;
		if (index_quality > 0) {
			if (all_key) {
				msg = L"Seeking: index present, optimal random access";
			}
			else if (index_quality == 2) {
				msg = L"Seeking: index present, all frames";
			}
			else {
				msg = L"Seeking: index present";
			}

			if (has_vfr) {
				msg += L" (vfr)";
			}
		}
		else {
			msg = L"Seeking: index missing, reverse scan may be slow";
		}

		SetDlgItemTextW(mhdlg, IDC_INDEX_INFO, msg.c_str());
	}
}
