/*
 * Copyright (C) 2024 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <string>

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

std::string AVError2Str(const int errnum);

const wchar_t* GetFileExt(std::wstring_view path);
