/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "VideoEnc.h"

#include <commctrl.h>
#include <shellapi.h>

#include <functional>
#include <cassert>

extern "C" {
#include <libavutil/imgutils.h>
}
#include "../Helper.h"
#include "../resource.h"

extern HINSTANCE hInstance;

void copy_plane(const int lines, uint8_t* dst, size_t dst_pitch, const uint8_t* src, size_t src_pitch)
{
	if (dst_pitch == src_pitch) {
		memcpy(dst, src, dst_pitch * lines);
		return;
	}

	const size_t linesize = std::min(src_pitch, dst_pitch);

	for (int y = 0; y < lines; ++y) {
		memcpy(dst, src, linesize);
		src += src_pitch;
		dst += dst_pitch;
	}
}

void copy_rgb24(AVFrame* frame, const VDXPixmapLayout* layout, const void* data)
{
	if (frame->format == AV_PIX_FMT_RGB24) {
		for (int y = 0; y < layout->h; y++) {
			const uint8_t* s = (uint8_t*)data + layout->data + layout->pitch * y;
			uint8_t* d = frame->data[0] + frame->linesize[0] * y;

			for (int x = 0; x < layout->w; x++) {
				d[0] = s[2];
				d[1] = s[1];
				d[2] = s[0];

				s += 3;
				d += 3;
			}
		}
	}
	if (frame->format == AV_PIX_FMT_GBRP) {
		for (int y = 0; y < layout->h; y++) {
			const uint8_t* s = (uint8_t*)data + layout->data + layout->pitch * y;

			uint8_t* g = frame->data[0] + frame->linesize[0] * y;
			uint8_t* b = frame->data[1] + frame->linesize[1] * y;
			uint8_t* r = frame->data[2] + frame->linesize[2] * y;

			for (int x = 0; x < layout->w; x++) {
				b[x] = s[0];
				g[x] = s[1];
				r[x] = s[2];
				s += 3;
			}
		}
	}
}

bool planar_rgb16(int format) {
	switch (format) {
	case AV_PIX_FMT_GBRP16LE:
	case AV_PIX_FMT_GBRP14LE:
	case AV_PIX_FMT_GBRP12LE:
	case AV_PIX_FMT_GBRP10LE:
	case AV_PIX_FMT_GBRP9LE:
		return true;
	}
	return false;
}

bool planar_rgba16(int format) {
	switch (format) {
	case AV_PIX_FMT_GBRAP16LE:
	case AV_PIX_FMT_GBRAP12LE:
	case AV_PIX_FMT_GBRAP10LE:
		return true;
	}
	return false;
}

void copy_rgb64(AVFrame* frame, const VDXPixmapLayout* layout, const void* data)
{
	if (frame->format == AV_PIX_FMT_BGRA64) {
		copy_plane(layout->h, frame->data[0], frame->linesize[0], (uint8_t*)data + layout->data, layout->pitch);
		return;
	}

	if (planar_rgb16(frame->format)) {
		for (int y = 0; y < layout->h; y++) {
			const uint16_t* s = (uint16_t*)((uint8_t*)data + layout->data + layout->pitch * y);

			uint16_t* g = (uint16_t*)(frame->data[0] + frame->linesize[0] * y);
			uint16_t* b = (uint16_t*)(frame->data[1] + frame->linesize[1] * y);
			uint16_t* r = (uint16_t*)(frame->data[2] + frame->linesize[2] * y);

			for (int x = 0; x < layout->w; x++) {
				b[x] = s[0];
				g[x] = s[1];
				r[x] = s[2];
				s += 4;
			}
		}
		return;
	}

	if (planar_rgba16(frame->format)) {
		for (int y = 0; y < layout->h; y++) {
			const uint16_t* s = (uint16_t*)((uint8_t*)data + layout->data + layout->pitch * y);

			uint16_t* g = (uint16_t*)(frame->data[0] + frame->linesize[0] * y);
			uint16_t* b = (uint16_t*)(frame->data[1] + frame->linesize[1] * y);
			uint16_t* r = (uint16_t*)(frame->data[2] + frame->linesize[2] * y);
			uint16_t* a = (uint16_t*)(frame->data[3] + frame->linesize[3] * y);

			for (int x = 0; x < layout->w; x++) {
				b[x] = s[0];
				g[x] = s[1];
				r[x] = s[2];
				a[x] = s[3];
				s += 4;
			}
		}
		return;
	}

	assert(0);
}

void copy_yuv(AVFrame* frame, const VDXPixmapLayout* layout, const void* data)
{
	int w2 = layout->w;
	int h2 = layout->h;
	switch (layout->format) {
	case nsVDXPixmap::kPixFormat_YUV420_Planar:
	case nsVDXPixmap::kPixFormat_YUV420_Planar16:
		w2 = (w2 + 1) / 2;
		h2 = h2 / 2;
		break;
	case nsVDXPixmap::kPixFormat_YUV422_Planar:
	case nsVDXPixmap::kPixFormat_YUV422_Planar16:
		w2 = (w2 + 1) / 2;
		break;
	}

	copy_plane(layout->h, frame->data[0], frame->linesize[0], (uint8_t*)data + layout->data,  layout->pitch);
	copy_plane(h2,        frame->data[1], frame->linesize[1], (uint8_t*)data + layout->data2, layout->pitch2);
	copy_plane(h2,        frame->data[2], frame->linesize[2], (uint8_t*)data + layout->data3, layout->pitch3);

	switch (layout->format) {
	case nsVDXPixmap::kPixFormat_YUV420_Alpha_Planar:
	case nsVDXPixmap::kPixFormat_YUV422_Alpha_Planar:
	case nsVDXPixmap::kPixFormat_YUV444_Alpha_Planar:
	case nsVDXPixmap::kPixFormat_YUV420_Alpha_Planar16:
	case nsVDXPixmap::kPixFormat_YUV422_Alpha_Planar16:
	case nsVDXPixmap::kPixFormat_YUV444_Alpha_Planar16:
	{
		const VDXPixmapLayoutAlpha* layout2 = (const VDXPixmapLayoutAlpha*)layout;
		copy_plane(layout->h, frame->data[3], frame->linesize[3], (uint8_t*)data + layout2->data4, layout2->pitch4);
		break;
	}
	}
}

void copy_nv12_p01x(AVFrame* frame, const VDXPixmapLayout* layout, const void* data)
{
	int h2 = layout->h / 2;
	copy_plane(layout->h, frame->data[0], frame->linesize[0], (uint8_t*)data + layout->data, layout->pitch);
	copy_plane(h2, frame->data[1], frame->linesize[1], (uint8_t*)data + layout->data2, layout->pitch2);
}

bool CodecBase::init()
{
	codec = avcodec_find_encoder_by_name(codec_name);
	return codec != 0;
}

bool CodecBase::load_config(void* data, size_t size) {
	size_t rsize = config_size();
	if (size != rsize) {
		return false;
	}
	int src_version = *(int*)data;
	if (src_version != config->version) {
		return false;
	}
	memcpy(config, data, rsize);
	return true;
}

bool CodecBase::test_av_format(AVPixelFormat format) {
	const enum AVPixelFormat* pix_fmts = nullptr;
	int ret = avcodec_get_supported_config(avctx, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0, (const void**)&pix_fmts, nullptr);
	if (ret < 0) {
		return false;
	}
	if (!pix_fmts) {
		assert(0); // unsupported but valid value.
		return false;
	}

	for (int i = 0; ; i++) {
		AVPixelFormat f = pix_fmts[i];
		if (f == AV_PIX_FMT_NONE) {
			break;
		}
		if (f == format) {
			return true;
		}
	}
	return false;
}

bool CodecBase::test_bits(int format, int bits) {
	switch (format) {
	case format_rgba:
		if (bits == 8) return test_av_format(AV_PIX_FMT_RGB32);
		if (bits == 10) return test_av_format(AV_PIX_FMT_GBRAP10LE);
		if (bits == 12) return test_av_format(AV_PIX_FMT_GBRAP12LE);
		if (bits == 16) return test_av_format(AV_PIX_FMT_BGRA64) || test_av_format(AV_PIX_FMT_GBRAP16LE);
		break;
	case format_rgb:
		if (bits == 8) return test_av_format(AV_PIX_FMT_0RGB32) || test_av_format(AV_PIX_FMT_RGB24) || test_av_format(AV_PIX_FMT_GBRP);
		if (bits == 9) return test_av_format(AV_PIX_FMT_GBRP9LE);
		if (bits == 10) return test_av_format(AV_PIX_FMT_GBRP10LE);
		if (bits == 12) return test_av_format(AV_PIX_FMT_GBRP12LE);
		if (bits == 14) return test_av_format(AV_PIX_FMT_GBRP14LE);
		if (bits == 16) return test_av_format(AV_PIX_FMT_GBRP16LE);
		break;
	case format_yuv420:
		if (bits == 8) return test_av_format(AV_PIX_FMT_YUV420P);
		if (bits == 9) return test_av_format(AV_PIX_FMT_YUV420P9LE);
		if (bits == 10) return test_av_format(AV_PIX_FMT_YUV420P10LE);
		if (bits == 12) return test_av_format(AV_PIX_FMT_YUV420P12LE);
		if (bits == 14) return test_av_format(AV_PIX_FMT_YUV420P14LE);
		if (bits == 16) return test_av_format(AV_PIX_FMT_YUV420P16LE);
		break;
	case format_yuv422:
		if (bits == 8) return test_av_format(AV_PIX_FMT_YUV422P);
		if (bits == 9) return test_av_format(AV_PIX_FMT_YUV422P9LE);
		if (bits == 10) return test_av_format(AV_PIX_FMT_YUV422P10LE);
		if (bits == 12) return test_av_format(AV_PIX_FMT_YUV422P12LE);
		if (bits == 14) return test_av_format(AV_PIX_FMT_YUV422P14LE);
		if (bits == 16) return test_av_format(AV_PIX_FMT_YUV422P16LE);
		break;
	case format_yuv444:
		if (bits == 8) return test_av_format(AV_PIX_FMT_YUV444P);
		if (bits == 9) return test_av_format(AV_PIX_FMT_YUV444P9LE);
		if (bits == 10) return test_av_format(AV_PIX_FMT_YUV444P10LE);
		if (bits == 12) return test_av_format(AV_PIX_FMT_YUV444P12LE);
		if (bits == 14) return test_av_format(AV_PIX_FMT_YUV444P14LE);
		if (bits == 16) return test_av_format(AV_PIX_FMT_YUV444P16LE);
		break;
	case format_yuva420:
		if (bits == 8) return test_av_format(AV_PIX_FMT_YUVA420P);
		if (bits == 9) return test_av_format(AV_PIX_FMT_YUVA420P9LE);
		if (bits == 10) return test_av_format(AV_PIX_FMT_YUVA420P10LE);
		if (bits == 16) return test_av_format(AV_PIX_FMT_YUVA420P16LE);
		break;
	case format_yuva422:
		if (bits == 8) return test_av_format(AV_PIX_FMT_YUVA422P);
		if (bits == 9) return test_av_format(AV_PIX_FMT_YUVA422P9LE);
		if (bits == 10) return test_av_format(AV_PIX_FMT_YUVA422P10LE);
		if (bits == 16) return test_av_format(AV_PIX_FMT_YUVA422P16LE);
		break;
	case format_yuva444:
		if (bits == 8) return test_av_format(AV_PIX_FMT_YUVA444P);
		if (bits == 9) return test_av_format(AV_PIX_FMT_YUVA444P9LE);
		if (bits == 10) return test_av_format(AV_PIX_FMT_YUVA444P10LE);
		if (bits == 16) return test_av_format(AV_PIX_FMT_YUVA444P16LE);
		break;
	case format_gray:
		if (bits == 8) return test_av_format(AV_PIX_FMT_GRAY8);
		if (bits == 9) return test_av_format(AV_PIX_FMT_GRAY9LE);
		if (bits == 10) return test_av_format(AV_PIX_FMT_GRAY10LE);
		if (bits == 12) return test_av_format(AV_PIX_FMT_GRAY12LE);
		if (bits == 16) return test_av_format(AV_PIX_FMT_GRAY16LE);
		break;
	}
	return false;
}

//---------------------------------------------------------------------------

void ConfigBase::Show(HWND parent, CodecBase* codec)
{
	this->codec = codec;
	int rsize = codec->config_size();
	old_param.reset(new uint8_t[rsize]);
	memcpy(old_param.get(), codec->config, rsize);
	VDXVideoFilterDialog::Show(hInstance, MAKEINTRESOURCEW(dialog_id), parent);
}

void ConfigBase::init_format()
{
	SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_RESETCONTENT, 0, 0);
	for (int i = CodecBase::format_rgb; i <= CodecBase::format_gray; i++) {
		SendDlgItemMessageA(mhdlg, IDC_ENC_COLORSPACE, CB_ADDSTRING, 0, (LPARAM)GetFormatName(i));
	}
	SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_SETCURSEL, codec->config->format - 1, 0);
}

void ConfigBase::adjust_bits()
{
	int format = codec->config->format;
	int bits = codec->config->bits;
	if (!codec->test_bits(format, bits)) {
		const int option[] = {
			codec->test_bits(format,8) ? 8 : 0,
			codec->test_bits(format,9) ? 9 : 0,
			codec->test_bits(format,10) ? 10 : 0,
			codec->test_bits(format,12) ? 12 : 0,
			codec->test_bits(format,14) ? 14 : 0,
			codec->test_bits(format,16) ? 16 : 0,
		};

		int bits1 = 0;
		for (const auto& opt : option) {
			int x = opt;
			if (x) {
				bits1 = x;
			}
			if (x >= bits) {
				break;
			}
		}

		notify_bits_change(bits1, codec->config->bits);
		codec->config->bits = bits1;
	}
}

void ConfigBase::change_format(int sel)
{
	codec->config->format = sel + 1;
	adjust_bits();
	init_bits();
	change_bits();
}

void ConfigBase::init_bits()
{
	int format = codec->config->format;
	EnableWindow(GetDlgItem(mhdlg, IDC_ENC_8_BIT), codec->test_bits(format, 8));
	EnableWindow(GetDlgItem(mhdlg, IDC_ENC_9_BIT), codec->test_bits(format, 9));
	EnableWindow(GetDlgItem(mhdlg, IDC_ENC_10_BIT), codec->test_bits(format, 10));
	EnableWindow(GetDlgItem(mhdlg, IDC_ENC_12_BIT), codec->test_bits(format, 12));
	EnableWindow(GetDlgItem(mhdlg, IDC_ENC_14_BIT), codec->test_bits(format, 14));
	EnableWindow(GetDlgItem(mhdlg, IDC_ENC_16_BIT), codec->test_bits(format, 16));
	CheckDlgButton(mhdlg, IDC_ENC_8_BIT, codec->config->bits == 8 ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(mhdlg, IDC_ENC_9_BIT, codec->config->bits == 9 ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(mhdlg, IDC_ENC_10_BIT, codec->config->bits == 10 ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(mhdlg, IDC_ENC_12_BIT, codec->config->bits == 12 ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(mhdlg, IDC_ENC_14_BIT, codec->config->bits == 14 ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(mhdlg, IDC_ENC_16_BIT, codec->config->bits == 16 ? BST_CHECKED : BST_UNCHECKED);
}

void ConfigBase::notify_hide()
{
	if (idc_message != -1) {
		SetDlgItemTextW(mhdlg, idc_message, nullptr);
	}
}

void ConfigBase::notify_bits_change(int bits_new, int bits_old)
{
	if (idc_message != -1) {
		auto str = std::format(L"(!) Bit depth adjusted (was {})", bits_old);
		SetDlgItemTextW(mhdlg, idc_message, str.c_str());
	}
}

INT_PTR ConfigBase::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
	{
		SetDlgItemTextA(mhdlg, IDC_ENCODER_LABEL, LIBAVCODEC_IDENT);
		init_format();
		init_bits();
		notify_hide();
		return TRUE;
	}

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			codec->save_config();
			EndDialog(mhdlg, TRUE);
			return TRUE;

		case IDCANCEL:
			memcpy(codec->config, old_param.get(), codec->config_size());
			EndDialog(mhdlg, FALSE);
			return TRUE;

		case IDC_ENC_COLORSPACE:
			if (HIWORD(wParam) == LBN_SELCHANGE) {
				int sel = (int)SendDlgItemMessageW(mhdlg, IDC_ENC_COLORSPACE, CB_GETCURSEL, 0, 0);
				change_format(sel);
				return TRUE;
			}
			break;

		case IDC_ENC_8_BIT:
		case IDC_ENC_9_BIT:
		case IDC_ENC_10_BIT:
		case IDC_ENC_12_BIT:
		case IDC_ENC_14_BIT:
		case IDC_ENC_16_BIT:
			notify_hide();
			if (IsDlgButtonChecked(mhdlg, IDC_ENC_8_BIT)) codec->config->bits = 8;
			if (IsDlgButtonChecked(mhdlg, IDC_ENC_9_BIT)) codec->config->bits = 9;
			if (IsDlgButtonChecked(mhdlg, IDC_ENC_10_BIT)) codec->config->bits = 10;
			if (IsDlgButtonChecked(mhdlg, IDC_ENC_12_BIT)) codec->config->bits = 12;
			if (IsDlgButtonChecked(mhdlg, IDC_ENC_14_BIT)) codec->config->bits = 14;
			if (IsDlgButtonChecked(mhdlg, IDC_ENC_16_BIT)) codec->config->bits = 16;
			change_bits();
			break;
		}
		break;

	case WM_NOTIFY:
	{
		NMHDR* nm = (NMHDR*)lParam;
		if (nm->idFrom == IDC_LINK) {
			switch (nm->code) {
			case NM_CLICK:
			case NM_RETURN:
			{
				NMLINK* link = (NMLINK*)lParam;
				ShellExecuteW(NULL, L"open", link->item.szUrl, NULL, NULL, SW_SHOW);
			}
			}
		}
	}
	break;
	}

	return FALSE;
}



//---------------------------------------------------------------------------

LRESULT CodecBase::compress_input_format(FilterModPixmapInfo* info) {
	if (config->format == format_rgba) {
		if (config->bits == 8) {
			return nsVDXPixmap::kPixFormat_XRGB8888;
		}
		if (config->bits > 8) {
			int max_value = (1 << config->bits) - 1;
			if (info) {
				info->ref_r = max_value;
				info->ref_g = max_value;
				info->ref_b = max_value;
				info->ref_a = max_value;
			}
			return nsVDXPixmap::kPixFormat_XRGB64;
		}
	}

	if (config->format == format_rgb) {
		if (config->bits == 8) {
			if (test_av_format(AV_PIX_FMT_0RGB32)) {
				return nsVDXPixmap::kPixFormat_XRGB8888;
			}
			if (test_av_format(AV_PIX_FMT_RGB24)) {
				return nsVDXPixmap::kPixFormat_RGB888;
			}
			if (test_av_format(AV_PIX_FMT_GBRP)) {
				return nsVDXPixmap::kPixFormat_RGB888;
			}
		}

		if (config->bits > 8) {
			int max_value = (1 << config->bits) - 1;
			if (info) {
				info->ref_r = max_value;
				info->ref_g = max_value;
				info->ref_b = max_value;
				info->ref_a = max_value;
			}
			return nsVDXPixmap::kPixFormat_XRGB64;
		}
	}

	if (config->format == format_yuv420) {
		if (config->bits == 8) {
			return nsVDXPixmap::kPixFormat_YUV420_Planar;
		}
		if (config->bits > 8) {
			int max_value = (1 << config->bits) - 1;
			if (info) {
				info->ref_r = max_value;
			}
			return nsVDXPixmap::kPixFormat_YUV420_Planar16;
		}
	}

	if (config->format == format_yuv422) {
		if (config->bits == 8) {
			return nsVDXPixmap::kPixFormat_YUV422_Planar;
		}
		if (config->bits > 8) {
			int max_value = (1 << config->bits) - 1;
			if (info) {
				info->ref_r = max_value;
			}
			return nsVDXPixmap::kPixFormat_YUV422_Planar16;
		}
	}

	if (config->format == format_yuv444) {
		if (config->bits == 8) {
			return nsVDXPixmap::kPixFormat_YUV444_Planar;
		}
		if (config->bits > 8) {
			int max_value = (1 << config->bits) - 1;
			if (info) {
				info->ref_r = max_value;
			}
			return nsVDXPixmap::kPixFormat_YUV444_Planar16;
		}
	}

	if (config->format == format_yuva420) {
		if (config->bits == 8) {
			return nsVDXPixmap::kPixFormat_YUV420_Alpha_Planar;
		}
		if (config->bits > 8) {
			int max_value = (1 << config->bits) - 1;
			if (info) {
				info->ref_r = max_value;
				info->ref_a = max_value;
			}
			return nsVDXPixmap::kPixFormat_YUV420_Alpha_Planar16;
		}
	}

	if (config->format == format_yuva422) {
		if (config->bits == 8) {
			return nsVDXPixmap::kPixFormat_YUV422_Alpha_Planar;
		}
		if (config->bits > 8) {
			int max_value = (1 << config->bits) - 1;
			if (info) {
				info->ref_r = max_value;
				info->ref_a = max_value;
			}
			return nsVDXPixmap::kPixFormat_YUV422_Alpha_Planar16;
		}
	}

	if (config->format == format_yuva444) {
		if (config->bits == 8) {
			return nsVDXPixmap::kPixFormat_YUV444_Alpha_Planar;
		}
		if (config->bits > 8) {
			int max_value = (1 << config->bits) - 1;
			if (info) {
				info->ref_r = max_value;
				info->ref_a = max_value;
			}
			return nsVDXPixmap::kPixFormat_YUV444_Alpha_Planar16;
		}
	}

	if (config->format == format_gray) {
		if (config->bits == 8) {
			return nsVDXPixmap::kPixFormat_Y8;
		}
		if (config->bits > 8) {
			int max_value = (1 << config->bits) - 1;
			if (info) {
				info->ref_r = max_value;
			}
			return nsVDXPixmap::kPixFormat_Y16;
		}
	}

	return 0;
}

LRESULT CodecBase::compress_get_format(BITMAPINFO* lpbiOutput, VDXPixmapLayout* layout)
{
	int extra_size = 0;
	if (avctx) {
		extra_size = (avctx->extradata_size + 1) & ~1;
	}

	if (!lpbiOutput) {
		return sizeof(BITMAPINFOHEADER) + extra_size;
	}

	if (layout->format != compress_input_format(0)) {
		return ICERR_BADFORMAT;
	}
	int iWidth = layout->w;
	int iHeight = layout->h;
	if (iWidth <= 0 || iHeight <= 0) {
		return ICERR_BADFORMAT;
	}

	BITMAPINFOHEADER* outhdr = &lpbiOutput->bmiHeader;
	memset(outhdr, 0, sizeof(BITMAPINFOHEADER));
	outhdr->biSize = sizeof(BITMAPINFOHEADER);
	outhdr->biWidth = iWidth;
	outhdr->biHeight = iHeight;
	outhdr->biPlanes = 1;
	outhdr->biBitCount = 32;
	if (layout->format == nsVDXPixmap::kPixFormat_XRGB64) {
		outhdr->biBitCount = 48;
	}
	outhdr->biCompression = codec_tag;
	outhdr->biSizeImage = iWidth * iHeight * 8;
	if (avctx) {
		outhdr->biSize = sizeof(BITMAPINFOHEADER) + avctx->extradata_size;
		if (avctx->codec_tag) {
			outhdr->biCompression = avctx->codec_tag;
		}
		uint8_t* p = ((uint8_t*)outhdr) + sizeof(BITMAPINFOHEADER);
		memset(p, 0, extra_size);
		memcpy(p, avctx->extradata, avctx->extradata_size);
	}

	return ICERR_OK;
}

LRESULT CodecBase::compress_query(BITMAPINFO* lpbiOutput, VDXPixmapLayout* layout)
{
	if (!lpbiOutput) {
		return ICERR_OK;
	}

	if (layout->format != compress_input_format(0)) {
		return ICERR_BADFORMAT;
	}
	int iWidth = layout->w;
	int iHeight = layout->h;
	if (iWidth <= 0 || iHeight <= 0) {
		return ICERR_BADFORMAT;
	}

	BITMAPINFOHEADER* outhdr = &lpbiOutput->bmiHeader;
	if (iWidth != outhdr->biWidth || iHeight != outhdr->biHeight) {
		return ICERR_BADFORMAT;
	}
	if (outhdr->biCompression != codec_tag) {
		return ICERR_BADFORMAT;
	}

	return ICERR_OK;
}

LRESULT CodecBase::compress_get_size(BITMAPINFO* lpbiOutput)
{
	return lpbiOutput->bmiHeader.biWidth * lpbiOutput->bmiHeader.biHeight * 8 + 4096;
}

LRESULT CodecBase::compress_frames_info(ICCOMPRESSFRAMES* icf)
{
	frame_total = icf->lFrameCount;
	time_base.den = icf->dwRate;
	time_base.num = icf->dwScale;
	keyint = icf->lKeyRate;
	return ICERR_OK;
}

LRESULT CodecBase::compress_begin(BITMAPINFO* lpbiOutput, VDXPixmapLayout* layout)
{
	if (compress_query(lpbiOutput, layout) != ICERR_OK) {
		return ICERR_BADFORMAT;
	}

	avctx = avcodec_alloc_context3(codec);

	if (config->format == format_rgba) {
		switch (config->bits) {
		case 16:
			avctx->pix_fmt = AV_PIX_FMT_GBRAP16LE;
			break;
		case 12:
			avctx->pix_fmt = AV_PIX_FMT_GBRAP12LE;
			break;
		case 10:
			avctx->pix_fmt = AV_PIX_FMT_GBRAP10LE;
			break;
		case 8:
			avctx->pix_fmt = AV_PIX_FMT_RGB32;
			break;
		}
	}
	else if (config->format == format_rgb) {
		switch (config->bits) {
		case 16:
			avctx->pix_fmt = AV_PIX_FMT_GBRP16LE;
			break;
		case 14:
			avctx->pix_fmt = AV_PIX_FMT_GBRP14LE;
			break;
		case 12:
			avctx->pix_fmt = AV_PIX_FMT_GBRP12LE;
			break;
		case 10:
			avctx->pix_fmt = AV_PIX_FMT_GBRP10LE;
			break;
		case 9:
			avctx->pix_fmt = AV_PIX_FMT_GBRP9LE;
			break;
		case 8:
			if (test_av_format(AV_PIX_FMT_0RGB32)) {
				avctx->pix_fmt = AV_PIX_FMT_0RGB32;
				break;
			}
			if (test_av_format(AV_PIX_FMT_RGB24)) {
				avctx->pix_fmt = AV_PIX_FMT_RGB24;
				break;
			}
			if (test_av_format(AV_PIX_FMT_GBRP)) {
				avctx->pix_fmt = AV_PIX_FMT_GBRP;
				break;
			}
			break;
		}
	}
	else if (config->format == format_yuv420) {
		switch (config->bits) {
		case 16:
			avctx->pix_fmt = AV_PIX_FMT_YUV420P16LE;
			break;
		case 14:
			avctx->pix_fmt = AV_PIX_FMT_YUV420P14LE;
			break;
		case 12:
			avctx->pix_fmt = AV_PIX_FMT_YUV420P12LE;
			break;
		case 10:
			avctx->pix_fmt = AV_PIX_FMT_YUV420P10LE;
			break;
		case 9:
			avctx->pix_fmt = AV_PIX_FMT_YUV420P9LE;
			break;
		case 8:
			avctx->pix_fmt = AV_PIX_FMT_YUV420P;
			break;
		}
	}
	else if (config->format == format_yuv422) {
		switch (config->bits) {
		case 16:
			avctx->pix_fmt = AV_PIX_FMT_YUV422P16LE;
			break;
		case 14:
			avctx->pix_fmt = AV_PIX_FMT_YUV422P14LE;
			break;
		case 12:
			avctx->pix_fmt = AV_PIX_FMT_YUV422P12LE;
			break;
		case 10:
			avctx->pix_fmt = AV_PIX_FMT_YUV422P10LE;
			break;
		case 9:
			avctx->pix_fmt = AV_PIX_FMT_YUV422P9LE;
			break;
		case 8:
			avctx->pix_fmt = AV_PIX_FMT_YUV422P;
			break;
		}
	}
	else if (config->format == format_yuv444) {
		switch (config->bits) {
		case 16:
			avctx->pix_fmt = AV_PIX_FMT_YUV444P16LE;
			break;
		case 14:
			avctx->pix_fmt = AV_PIX_FMT_YUV444P14LE;
			break;
		case 12:
			avctx->pix_fmt = AV_PIX_FMT_YUV444P12LE;
			break;
		case 10:
			avctx->pix_fmt = AV_PIX_FMT_YUV444P10LE;
			break;
		case 9:
			avctx->pix_fmt = AV_PIX_FMT_YUV444P9LE;
			break;
		case 8:
			avctx->pix_fmt = AV_PIX_FMT_YUV444P;
			break;
		}
	}
	else if (config->format == format_yuva420) {
		switch (config->bits) {
		case 16:
			avctx->pix_fmt = AV_PIX_FMT_YUVA420P16LE;
			break;
		case 10:
			avctx->pix_fmt = AV_PIX_FMT_YUVA420P10LE;
			break;
		case 9:
			avctx->pix_fmt = AV_PIX_FMT_YUVA420P9LE;
			break;
		case 8:
			avctx->pix_fmt = AV_PIX_FMT_YUVA420P;
			break;
		}
	}
	else if (config->format == format_yuva422) {
		switch (config->bits) {
		case 16:
			avctx->pix_fmt = AV_PIX_FMT_YUVA422P16LE;
			break;
		case 10:
			avctx->pix_fmt = AV_PIX_FMT_YUVA422P10LE;
			break;
		case 9:
			avctx->pix_fmt = AV_PIX_FMT_YUVA422P9LE;
			break;
		case 8:
			avctx->pix_fmt = AV_PIX_FMT_YUVA422P;
			break;
		}
	}
	else if (config->format == format_yuva444) {
		switch (config->bits) {
		case 16:
			avctx->pix_fmt = AV_PIX_FMT_YUVA444P16LE;
			break;
		case 10:
			avctx->pix_fmt = AV_PIX_FMT_YUVA444P10LE;
			break;
		case 9:
			avctx->pix_fmt = AV_PIX_FMT_YUVA444P9LE;
			break;
		case 8:
			avctx->pix_fmt = AV_PIX_FMT_YUVA444P;
			break;
		}
	}
	else if (config->format == format_gray) {
		switch (config->bits) {
		case 16:
			avctx->pix_fmt = AV_PIX_FMT_GRAY16LE;
			break;
		case 12:
			avctx->pix_fmt = AV_PIX_FMT_GRAY12LE;
			break;
		case 10:
			avctx->pix_fmt = AV_PIX_FMT_GRAY10LE;
			break;
		case 9:
			avctx->pix_fmt = AV_PIX_FMT_GRAY9LE;
			break;
		case 8:
			avctx->pix_fmt = AV_PIX_FMT_GRAY8;
			break;
		}
	}

	avctx->thread_count = 0;
	avctx->bit_rate = 0;
	avctx->width = layout->w;
	avctx->height = layout->h;
	avctx->time_base = time_base;
	avctx->gop_size = 1;
	avctx->max_b_frames = 1;
	avctx->framerate = av_make_q(time_base.den, time_base.num);

	avctx->color_range = color_range;
	avctx->colorspace = colorspace;
	if (global_header) {
		avctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	if (!init_ctx(layout)) {
		return ICERR_BADPARAM;
	}

	if (keyint > 1) {
		avctx->gop_size = keyint;
	}

	int ret = avcodec_open2(avctx, codec, nullptr);
	if (ret < 0) {
		compress_end();
		return ICERR_BADPARAM;
	}

	frame = av_frame_alloc();
	frame->format = avctx->pix_fmt;
	frame->color_range = avctx->color_range;
	frame->colorspace = avctx->colorspace;
	frame->width = avctx->width;
	frame->height = avctx->height;

	ret = av_image_alloc(frame->data, frame->linesize, avctx->width, avctx->height, avctx->pix_fmt, 32);
	if (ret < 0) { compress_end(); return ICERR_MEMORY; }

	return ICERR_OK;
}

void CodecBase::streamControl(VDXStreamControl* sc)
{
	global_header = sc->global_header;
}

void CodecBase::getStreamInfo(VDXStreamInfo* si)
{
	AVCodecParameters* par = avcodec_parameters_alloc();
	avcodec_parameters_from_context(par, avctx);

	si->avcodec_version = LIBAVCODEC_VERSION_INT;

	si->format = par->format;
	si->bit_rate = par->bit_rate;
	si->bits_per_coded_sample = par->bits_per_coded_sample;
	si->bits_per_raw_sample = par->bits_per_raw_sample;
	si->profile = par->profile;
	si->level = par->level;
	si->sar_width = par->sample_aspect_ratio.num;
	si->sar_height = par->sample_aspect_ratio.den;
	si->field_order = par->field_order;
	si->color_range = par->color_range;
	si->color_primaries = par->color_primaries;
	si->color_trc = par->color_trc;
	si->color_space = par->color_space;
	si->chroma_location = par->chroma_location;
	si->video_delay = par->video_delay;

	avcodec_parameters_free(&par);
}

LRESULT CodecBase::compress_end()
{
	if (avctx) {
		av_freep(&avctx->extradata);
		avcodec_free_context(&avctx);
	}
	if (frame) {
		av_freep(&frame->data[0]);
		av_frame_free(&frame);
	}
	return ICERR_OK;
}

LRESULT CodecBase::compress1(ICCOMPRESS* icc, VDXPixmapLayout* layout)
{
	VDXPictureCompress pc;
	pc.px = layout;
	return compress2(icc, &pc);
}

LRESULT CodecBase::compress2(ICCOMPRESS* icc, VDXPictureCompress* pc)
{
	std::unique_ptr<AVPacket, std::function<void(AVPacket*)>> pkt{ av_packet_alloc(), [](AVPacket* p) { av_packet_free(&p); } };

	int ret = 0;

	const VDXPixmapLayout* layout = pc->px;

	if (icc->lFrameNum < frame_total) {
		frame->pts = icc->lFrameNum;

		switch (layout->format) {
		case nsVDXPixmap::kPixFormat_RGB888:
			copy_rgb24(frame, layout, icc->lpInput);
			break;
		case nsVDXPixmap::kPixFormat_XRGB8888:
		case nsVDXPixmap::kPixFormat_Y8:
		case nsVDXPixmap::kPixFormat_Y16:
			copy_plane(layout->h, frame->data[0], frame->linesize[0], (uint8_t*)icc->lpInput + layout->data, layout->pitch);
			break;
		case nsVDXPixmap::kPixFormat_XRGB64:
			copy_rgb64(frame, layout, icc->lpInput);
			break;
		case nsVDXPixmap::kPixFormat_YUV420_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_Planar:
		case nsVDXPixmap::kPixFormat_YUV444_Planar:
		case nsVDXPixmap::kPixFormat_YUV420_Alpha_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_Alpha_Planar:
		case nsVDXPixmap::kPixFormat_YUV444_Alpha_Planar:
		case nsVDXPixmap::kPixFormat_YUV420_Planar16:
		case nsVDXPixmap::kPixFormat_YUV422_Planar16:
		case nsVDXPixmap::kPixFormat_YUV444_Planar16:
		case nsVDXPixmap::kPixFormat_YUV420_Alpha_Planar16:
		case nsVDXPixmap::kPixFormat_YUV422_Alpha_Planar16:
		case nsVDXPixmap::kPixFormat_YUV444_Alpha_Planar16:
			copy_yuv(frame, layout, icc->lpInput);
			break;
		case nsVDXPixmap::kPixFormat_YUV420_NV12:
		case nsVDXPixmap::kPixFormat_YUV420_P010:
		case nsVDXPixmap::kPixFormat_YUV420_P016:
			copy_nv12_p01x(frame, layout, icc->lpInput);
			break;
		}

		ret = avcodec_send_frame(avctx, frame);
	}
	else if (icc->lFrameNum == frame_total || frame_total == 0) {
		ret = avcodec_send_frame(avctx, nullptr);
		frame_total = icc->lFrameNum;
	}

	if (ret == 0) {
		ret = avcodec_receive_packet(avctx, pkt.get());
	}

	if (ret == 0) {
		if (pkt->size > (int)icc->lpbiOutput->biSizeImage) {
			av_packet_unref(pkt.get());
			return ICERR_MEMORY;
		}

		DWORD flags = 0;
		if (pkt->flags & AV_PKT_FLAG_KEY) {
			flags = AVIIF_KEYFRAME;
		}
		*icc->lpdwFlags = flags;

		memcpy(icc->lpOutput, pkt->data, pkt->size);
		icc->lpbiOutput->biSizeImage = pkt->size;
		pc->pts = pkt->pts;
		pc->dts = pkt->dts;
		pc->duration = pkt->duration;
		av_packet_unref(pkt.get());
	}
	else if (ret == AVERROR(EAGAIN)) {
		icc->lpbiOutput->biSizeImage = 0;
		*icc->lpdwFlags = VDCOMPRESS_WAIT;
	}
	else if (ret == AVERROR_EOF) {
		icc->lpbiOutput->biSizeImage = 0;
		*icc->lpdwFlags = 0;
	}
	else if (ret < 0) {
		compress_end();
		return ICERR_MEMORY;
	}

	return ICERR_OK;
}
