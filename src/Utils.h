/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2024 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <string>
#include <algorithm>

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
