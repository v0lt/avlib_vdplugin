/*
 * Copyright (C) 2024-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stdafx.h"

#include <amvideo.h>
#include <vector>
#include "Helper.h"

std::wstring ConvertUtf8OrAnsiLinesToWide(const std::string_view sv)
{
	std::wstring wstr;
	size_t pos = 0;

	while (pos < sv.length()) {
		size_t k = sv.find_first_of('\n', pos);
		if (k != std::string::npos) {
			k++;
		} else {
			k = sv.length();
		}

		auto line = sv.data() + pos;
		auto line_size = k - pos;
		UINT codePage = CP_UTF8;
		int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, line, (int)line_size, nullptr, 0);
		if (count == 0) {
			codePage = CP_ACP;
			count = MultiByteToWideChar(CP_ACP, 0, line, (int)line_size, nullptr, 0);
		}

		auto size = wstr.length();
		wstr.resize(size + count);
		MultiByteToWideChar(codePage, 0, line, (int)line_size, &wstr[size], count);

		pos = k;
	}

	return wstr;
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

const wchar_t* GetFileExt(std::wstring_view path)
{
	size_t pos = path.find_last_of(L".\\/");
	if (pos != path.npos && path[pos] == L'.') {
		return &path[pos];
	}
	return nullptr;
}

void AddStringSetData(HWND hDlg, const int nIDDlgItem, const char* str, const LONG_PTR data)
{
	LRESULT result = SendDlgItemMessageA(hDlg, nIDDlgItem, CB_ADDSTRING, 0, (LPARAM)str);
	if (result >= 0) {
		SendDlgItemMessageW(hDlg, nIDDlgItem, CB_SETITEMDATA, (WPARAM)result, (LPARAM)data);
	}
}

void AddStringSetData(HWND hDlg, const int nIDDlgItem, const wchar_t* str, const LONG_PTR data)
{
	LRESULT result = SendDlgItemMessageW(hDlg, nIDDlgItem, CB_ADDSTRING, 0, (LPARAM)str);
	if (result >= 0) {
		SendDlgItemMessageW(hDlg, nIDDlgItem, CB_SETITEMDATA, (WPARAM)result, (LPARAM)data);
	}
}

LONG_PTR GetCurrentItemData(HWND hDlg, const int nIDDlgItem)
{
	LRESULT result = SendDlgItemMessageW(hDlg, nIDDlgItem, CB_GETCURSEL, 0, 0);
	if (result >= 0) {
		result = SendDlgItemMessageW(hDlg, nIDDlgItem, CB_GETITEMDATA, (WPARAM)result, 0);
	}
	return result;
}

void SelectByItemData(HWND hDlg, const int nIDDlgItem, const LONG_PTR data)
{
	const LRESULT count = SendDlgItemMessageW(hDlg, nIDDlgItem, CB_GETCOUNT, 0, 0);
	for (LRESULT i = 0; i < count; i++) {
		const LRESULT itemData = SendDlgItemMessageW(hDlg, nIDDlgItem, CB_GETITEMDATA, (WPARAM)i, 0);
		if (itemData == data) {
			SendDlgItemMessageW(hDlg, nIDDlgItem, CB_SETCURSEL, (WPARAM)i, 0);
			return;
		}
	}
}
