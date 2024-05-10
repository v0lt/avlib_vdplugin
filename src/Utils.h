#pragma once

#include <string>

inline std::wstring A2WStr(const std::string_view sv)
{
	return std::wstring(sv.begin(), sv.end());
}

std::wstring ConvertUtf8ToWide(const std::string& str);

std::string AVError2Str(const int errnum);
