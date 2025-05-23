/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stdafx.h"

#pragma once
extern "C" {
#include <libavutil/opt.h>
}
#include "../Helper.h"
#include "../resource.h"

#include "VideoEnc_FFV1.h"
#include "VideoEnc_FFVHuff.h"
#include "VideoEnc_ProRes.h"
#include "VideoEnc_VP8.h"
#include "VideoEnc_VP9.h"
#include "VideoEnc_SVT_AV1.h"
#include "VideoEnc_x264.h"
#include "VideoEnc_x265.h"
#include "VideoEnc_NVENC_H264.h"
#include "VideoEnc_NVENC_HEVC.h"
#include "VideoEnc_NVENC_AV1.h"
#include "VideoEnc_QSV_H264.h"
#include "VideoEnc_QSV_HEVC.h"
#include "VideoEnc_AMF_H264.h"
#include "VideoEnc_AMF_HEVC.h"
#include "VideoEnc_AMF_AV1.h"

void init_av();

extern "C" LRESULT WINAPI DriverProc(DWORD_PTR dwDriverId, HDRVR hDriver, UINT uMsg, LPARAM lParam1, LPARAM lParam2)
{
	CodecBase* codec = (CodecBase*)dwDriverId;

	switch (uMsg) {
	case DRV_LOAD:
		init_av();
		return DRV_OK;

	case DRV_FREE:
		return DRV_OK;

	case DRV_OPEN:
	{
		ICOPEN* icopen = (ICOPEN*)lParam2;
		if (!icopen || icopen->fccType != ICTYPE_VIDEO) {
			return 0;
		}

		CodecBase* new_codec = nullptr;
		switch (icopen->fccHandler) {
		case 0:
		case CodecFFV1::id_tag:       new_codec = new CodecFFV1;       break;
		case CodecFFVHuff::id_tag:       new_codec = new CodecFFVHuff;       break;
		case CodecProres::id_tag:     new_codec = new CodecProres;     break;
		case CodecVP8::id_tag:        new_codec = new CodecVP8;        break;
		case CodecVP9::id_tag:        new_codec = new CodecVP9;        break;
		case CodecSVT_AV1::id_tag:    new_codec = new CodecSVT_AV1;    break;
		case CodecX264::id_tag:       new_codec = new CodecX264;       break;
		case CodecX265::id_tag:       new_codec = new CodecX265;       break;
		case CodecH265LS::id_tag:     new_codec = new CodecH265LS;     break;
		case CodecNVENC_H264::id_tag: new_codec = new CodecNVENC_H264; break;
		case CodecNVENC_HEVC::id_tag: new_codec = new CodecNVENC_HEVC; break;
		case CodecNVENC_AV1::id_tag:  new_codec = new CodecNVENC_AV1;  break;
		case CodecQSV_H264::id_tag:   new_codec = new CodecQSV_H264;   break;
		case CodecQSV_HEVC::id_tag:   new_codec = new CodecQSV_HEVC;   break;
		case CodecAMF_H264::id_tag:   new_codec = new CodecAMF_H264;   break;
		case CodecAMF_HEVC::id_tag:   new_codec = new CodecAMF_HEVC;   break;
		case CodecAMF_AV1::id_tag:    new_codec = new CodecAMF_AV1;    break;
		}
		if (new_codec) {
			if (!new_codec->init()) {
				delete new_codec;
				new_codec = nullptr;
			}
		}
		if (!new_codec) {
			icopen->dwError = ICERR_MEMORY;
			return 0;
		}

		icopen->dwError = ICERR_OK;
		return (LRESULT)new_codec;
	}

	case DRV_CLOSE:
		delete codec;
		return DRV_OK;

	case ICM_GETSTATE:
	{
		int rsize = codec->config_size();
		if (lParam1 == 0) {
			return rsize;
		}
		if (lParam2 != rsize) {
			return ICERR_BADSIZE;
		}
		memcpy((void*)lParam1, codec->config, rsize);
		return ICERR_OK;
	}

	case ICM_SETSTATE:
		if (lParam1 == 0) {
			codec->reset_config();
			return 0;
		}
		if (!codec->load_config((void*)lParam1, lParam2)) {
			return 0;
		}
		return codec->config_size();

	case ICM_GETINFO:
	{
		ICINFO* icinfo = (ICINFO*)lParam1;
		if (lParam2 < sizeof(ICINFO)) {
			return 0;
		}
		icinfo->dwSize = sizeof(ICINFO);
		icinfo->fccType = ICTYPE_VIDEO;
		icinfo->dwVersion = 0;
		icinfo->dwVersionICM = ICVERSION;
		codec->getinfo(*icinfo);
		return sizeof(ICINFO);
	}

	case ICM_CONFIGURE:
		if (lParam1 != -1) {
			return codec->configure((HWND)lParam1);
		}
		return ICERR_OK;

	case ICM_ABOUT:
		return ICERR_UNSUPPORTED;

	case ICM_COMPRESS_END:
		return codec->compress_end();

	case ICM_COMPRESS_FRAMES_INFO:
		return codec->compress_frames_info((ICCOMPRESSFRAMES*)lParam1);
	}

	return ICERR_UNSUPPORTED;
}

extern "C" LRESULT WINAPI VDDriverProc(DWORD_PTR dwDriverId, HDRVR hDriver, UINT uMsg, LPARAM lParam1, LPARAM lParam2)
{
	CodecBase* codec = (CodecBase*)dwDriverId;

	switch (uMsg) {
	case VDICM_ENUMFORMATS:
		if (lParam1 == 0) return CodecFFV1::id_tag;
		if (lParam1 == CodecFFV1::id_tag)       return CodecFFVHuff::id_tag;
		if (lParam1 == CodecFFVHuff::id_tag)       return CodecProres::id_tag;
		if (lParam1 == CodecProres::id_tag)     return CodecVP8::id_tag;
		if (lParam1 == CodecVP8::id_tag)        return CodecVP9::id_tag;
		if (lParam1 == CodecVP9::id_tag)        return CodecSVT_AV1::id_tag;
		if (lParam1 == CodecSVT_AV1::id_tag)    return CodecX264::id_tag;
		if (lParam1 == CodecX264::id_tag)       return CodecX265::id_tag;
		if (lParam1 == CodecX265::id_tag)       return CodecH265LS::id_tag;
		if (lParam1 == CodecH265LS::id_tag)     return CodecNVENC_H264::id_tag;
		if (lParam1 == CodecNVENC_H264::id_tag) return CodecNVENC_HEVC::id_tag;
		if (lParam1 == CodecNVENC_HEVC::id_tag) return CodecNVENC_AV1::id_tag;
		if (lParam1 == CodecNVENC_AV1::id_tag)  return CodecQSV_H264::id_tag;
		if (lParam1 == CodecQSV_H264::id_tag)   return CodecQSV_HEVC::id_tag;
		if (lParam1 == CodecQSV_HEVC::id_tag)   return CodecAMF_H264::id_tag;
		if (lParam1 == CodecAMF_H264::id_tag)   return CodecAMF_HEVC::id_tag;
		if (lParam1 == CodecAMF_HEVC::id_tag)   return CodecAMF_AV1::id_tag;
		return 0;

	case VDICM_GETHANDLER:
		if (codec) return codec->codec_tag;
		return ICERR_UNSUPPORTED;

	case VDICM_COMPRESS_INPUT_FORMAT:
		return codec->compress_input_format((FilterModPixmapInfo*)lParam1);

	case VDICM_COMPRESS_INPUT_INFO:
		return codec->compress_input_info((VDXPixmapLayout*)lParam1);

	case VDICM_COMPRESS_QUERY:
		return codec->compress_query((BITMAPINFO*)lParam2, (VDXPixmapLayout*)lParam1);

	case VDICM_COMPRESS_GET_FORMAT:
		return codec->compress_get_format((BITMAPINFO*)lParam2, (VDXPixmapLayout*)lParam1);

	case VDICM_COMPRESS_GET_SIZE:
		return codec->compress_get_size((BITMAPINFO*)lParam2);

	case VDICM_COMPRESS_BEGIN:
		return codec->compress_begin((BITMAPINFO*)lParam2, (VDXPixmapLayout*)lParam1);

	case VDICM_STREAMCONTROL:
		codec->streamControl((VDXStreamControl*)lParam1);
		return 0;

	case VDICM_GETSTREAMINFO:
		codec->getStreamInfo((VDXStreamInfo*)lParam1);
		return 0;

	case VDICM_COMPRESS:
		return codec->compress1((ICCOMPRESS*)lParam1, (VDXPixmapLayout*)lParam2);

	case VDICM_COMPRESS2:
		return codec->compress2((ICCOMPRESS*)lParam1, (VDXPictureCompress*)lParam2);

	case VDICM_COMPRESS_TRUNCATE:
		codec->frame_total = 0;
		return 0;

	case VDICM_LOGPROC:
		codec->logProc = (VDLogProc)lParam1;
		return 0;

	case VDICM_COMPRESS_MATRIX_INFO:
	{
		int colorSpaceMode = (int)lParam1;
		int colorRangeMode = (int)lParam2;

		switch (colorSpaceMode) {
		case nsVDXPixmap::kColorSpaceMode_601:
			codec->colorspace = AVCOL_SPC_BT470BG;
			break;
		case nsVDXPixmap::kColorSpaceMode_709:
			codec->colorspace = AVCOL_SPC_BT709;
			break;
		default:
			codec->colorspace = AVCOL_SPC_UNSPECIFIED;
		}

		switch (colorRangeMode) {
		case nsVDXPixmap::kColorRangeMode_Limited:
			codec->color_range = AVCOL_RANGE_MPEG;
			break;
		case nsVDXPixmap::kColorRangeMode_Full:
			codec->color_range = AVCOL_RANGE_JPEG;
			break;
		default:
			codec->color_range = AVCOL_RANGE_UNSPECIFIED;
		}

		return ICERR_OK;
	}
	}

	return ICERR_UNSUPPORTED;
}
