/*
 * Copyright (C) 2024 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <string>
#include <algorithm>

#if _DEBUG
template <typename... Args>
void DLog(const char* format, Args ...args)
{
	char buf[2000];

	sprintf_s(buf, format, args...);
	cscat_s(buf, L"\n");

	OutputDebugStringA(buf);
};

template <typename... Args>
void DLog(const wchar_t* format, Args ...args)
{
	wchar_t buf[2000];

	swprintf_s(buf, format, args...);
	wcscat_s(buf, L"\n");

	OutputDebugStringW(buf);
};
#else
#define DLog(...) __noop
#endif

bool DumpImageToFile(const wchar_t* filepath, const uint8_t* const src_data[4], const int src_linesize[4], enum AVPixelFormat pix_fmt, int width, int height);

inline void str_tolower(std::string& s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
	// char for std::tolower should be converted to unsigned char
}

inline void str_tolower(std::wstring& s)
{
	std::transform(s.begin(), s.end(), s.begin(), ::tolower);
}

inline std::wstring A2WStr(const std::string_view sv)
{
	return std::wstring(sv.begin(), sv.end());
}

std::wstring ConvertAnsiToWide(const std::string& str);
std::wstring ConvertAnsiToWide(const char* pstr, int size);

std::wstring ConvertUtf8ToWide(const std::string& str);
std::wstring ConvertUtf8ToWide(const char* pstr, int size);

std::string AVError2Str(const int errnum);
