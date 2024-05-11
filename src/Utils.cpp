#include "windows.h"
#include "Utils.h"

extern "C" {
#include <libavutil/error.h>
}

std::wstring ConvertAnsiToWide(const char* pstr, int size)
{
	int count = MultiByteToWideChar(CP_ACP, 0, pstr, size, nullptr, 0);
	std::wstring wstr(count, 0);
	MultiByteToWideChar(CP_ACP, 0, pstr, size, &wstr[0], count);
	return wstr;
}

std::wstring ConvertAnsiToWide(const std::string& str)
{
	int count = MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.length(), nullptr, 0);
	std::wstring wstr(count, 0);
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.length(), &wstr[0], count);
	return wstr;
}

std::wstring ConvertUtf8ToWide(const std::string& str)
{
	int count = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), nullptr, 0);
	std::wstring wstr(count, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), &wstr[0], count);
	return wstr;
}

std::wstring ConvertUtf8ToWide(const char* pstr, int size)
{
	int count = MultiByteToWideChar(CP_UTF8, 0, pstr, size, nullptr, 0);
	std::wstring wstr(count, 0);
	MultiByteToWideChar(CP_UTF8, 0, pstr, size, &wstr[0], count);
	return wstr;
}


std::string AVError2Str(const int errnum)
{
	char errBuf[AV_ERROR_MAX_STRING_SIZE] = {};
	return std::string(av_make_error_string(errBuf, sizeof(errBuf), errnum));
}