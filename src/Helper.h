/*
 * Copyright (C) 2024-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#if _DEBUG
template <typename... Args>
void DLog(const std::string_view format, Args ...args)
{
	std::string str = std::vformat(format, std::make_format_args(args...)) + '\n';

	OutputDebugStringA(str.c_str());
};

template <typename... Args>
void DLog(const std::wstring_view format, Args ...args)
{
	std::wstring str = std::vformat(format, std::make_wformat_args(args...)) + L'\n';

	OutputDebugStringW(str.c_str());
};
#else
#define DLog(...) __noop
#endif

bool DumpImageToFile(const wchar_t* filepath, const uint8_t* const src_data[4], const int src_linesize[4], enum AVPixelFormat pix_fmt, int width, int height);

const wchar_t* GetFileExt(std::wstring_view path);

void AddStringSetData(HWND hDlg, const int nIDDlgItem, const char* str, const LONG_PTR data);
void AddStringSetData(HWND hDlg, const int nIDDlgItem, const wchar_t* str, const LONG_PTR data);
LONG_PTR GetCurrentItemData(HWND hDlg, const int nIDDlgItem);
void SelectByItemData(HWND hDlg, const int nIDDlgItem, LONG_PTR data);
