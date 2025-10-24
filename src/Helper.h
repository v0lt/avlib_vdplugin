/*
 * Copyright (C) 2024-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#if _DEBUG

inline void DLog(std::string_view sv)
{
	std::string str (sv);
	str += '\n';
	OutputDebugStringA(str.c_str());
}

inline void DLog(std::wstring_view sv)
{
	std::wstring wstr(sv);
	wstr += L'\n';
	OutputDebugStringW(wstr.c_str());
}

template <typename... Args>
inline void DLog(std::format_string<Args...> fmt, Args&&... args)
{
	std::string str = std::format(fmt, std::forward<Args>(args)...);
	str += '\n';
	OutputDebugStringA(str.c_str());
};

template <typename... Args>
inline void DLog(std::wformat_string<Args...> fmt, Args&&... args)
{
	std::wstring wstr = std::format(fmt, std::forward<Args>(args)...);
	wstr += L'\n';
	OutputDebugStringW(wstr.c_str());
};

#else
#define DLog(...) __noop
#endif

std::wstring ConvertUtf8OrAnsiLinesToWide(const std::string_view sv);

bool DumpImageToFile(const wchar_t* filepath, const uint8_t* const src_data[4], const int src_linesize[4], enum AVPixelFormat pix_fmt, int width, int height);

const wchar_t* GetFileExt(std::wstring_view path);

void AddStringSetData(HWND hDlg, const int nIDDlgItem, const char* str, const LONG_PTR data);
void AddStringSetData(HWND hDlg, const int nIDDlgItem, const wchar_t* str, const LONG_PTR data);
LONG_PTR GetCurrentItemData(HWND hDlg, const int nIDDlgItem);
void SelectByItemData(HWND hDlg, const int nIDDlgItem, LONG_PTR data);
