/*
 * Copyright (C) 2024-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stdafx.h"

#include <amvideo.h>
#include <vector>
#include "Helper.h"

extern "C" {
#include <libavutil/error.h>
}

bool DumpImageToFile(const wchar_t* filepath, const uint8_t* const src_data[4], const int src_linesize[4], enum AVPixelFormat pix_fmt, int width, int height)
{
	if (!src_data[0] || src_linesize[0] <= 0 || pix_fmt < 0 || width <= 0 || height <= 0) {
		return false;
	}

	const unsigned pseudobitdepth = 8;
	const unsigned tablecolors = (pseudobitdepth == 8) ? 256 : 0;

	BITMAPINFOHEADER bih = {};
	bih.biSize      = sizeof(BITMAPINFOHEADER);
	bih.biWidth     = width;
	bih.biHeight    = -height;
	bih.biBitCount  = pseudobitdepth;
	bih.biPlanes    = 1;
	bih.biSizeImage = DIBSIZE(bih);
	bih.biClrUsed   = tablecolors;

	const unsigned widthBytes = DIBWIDTHBYTES(bih);
	if (widthBytes > (unsigned)src_linesize[0]) {
		return false;
	}

	BITMAPFILEHEADER bfh = {};
	bfh.bfType      = 0x4d42;
	bfh.bfOffBits   = sizeof(bfh) + sizeof(BITMAPINFOHEADER) + tablecolors * 4;
	bfh.bfSize      = bfh.bfOffBits + bih.biSizeImage;

	std::vector<uint8_t> colortable;
	colortable.reserve(tablecolors * 4);
	for (unsigned i = 0; i < tablecolors; i++) {
		colortable.emplace_back(i);
		colortable.emplace_back(i);
		colortable.emplace_back(i);
		colortable.emplace_back(0);
	}

	FILE* fp;
	errno_t err = _wfopen_s(&fp, filepath, L"wb");
	if (err) {
		return false;
	}
	fwrite(&bfh, sizeof(bfh), 1, fp);
	fwrite(&bih, sizeof(bih), 1, fp);
	if (colortable.size()) {
		fwrite(colortable.data(), colortable.size(), 1, fp);
	}
	const uint8_t* src = src_data[0];
	for (int y = 0; y < height; ++y) {
		fwrite(src, widthBytes, 1, fp);
		src += src_linesize[0];
	}
	fclose(fp);

	return true;
}

std::string AVError2Str(const int errnum)
{
	char errBuf[AV_ERROR_MAX_STRING_SIZE] = {};
	return std::string(av_make_error_string(errBuf, sizeof(errBuf), errnum));
}

const wchar_t* GetFileExt(std::wstring_view path)
{
	size_t pos = path.find_last_of(L".\\/");
	if (pos != path.npos && path[pos] == L'.') {
		return &path[pos];
	}
	return nullptr;
}
