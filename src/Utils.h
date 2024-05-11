#pragma once

#include <string>

inline std::wstring A2WStr(const std::string_view sv)
{
	return std::wstring(sv.begin(), sv.end());
}

std::wstring ConvertAnsiToWide(const std::string& str);
std::wstring ConvertAnsiToWide(const char* pstr, int size);

std::wstring ConvertUtf8ToWide(const std::string& str);
std::wstring ConvertUtf8ToWide(const char* pstr, int size);

std::string AVError2Str(const int errnum);
