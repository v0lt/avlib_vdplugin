/*
 * Copyright (C) 2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stdafx.h"
#include "ffmpeg_helper.h"
#include <mutex>

extern "C" {
#include <libavutil/log.h>
}


static int av_initialized = 0;

static std::string ff_error_log;
static std::mutex ff_error_log_mutex;

void av_error_log_func(void* ptr, int level, const char* fmt, va_list vl)
{
	if (level <= AV_LOG_ERROR) {
		std::lock_guard<std::mutex> lock(ff_error_log_mutex);

		int len = _vscprintf(fmt, vl);
		if (len >= 0) {
			ff_error_log.resize(len);
			len = vsprintf_s(ff_error_log.data(), len + 1, fmt, vl);
			/*
			auto size = ff_error_log.length();
			ff_error_log.resize(size + len);
			len = vsprintf_s(ff_error_log.data() + size, len + 1, fmt, vl);
			*/
		}
	}
}

static int av_log_level = AV_LOG_VERBOSE;

void av_log_func(void* ptr, int level, const char* fmt, va_list vl)
{
	const char prefix[] = "FFLog: ";
	static size_t newline = std::size(prefix) - 1;

	if (level > av_log_level) {
		return;
	}
	if (level <= AV_LOG_ERROR) {
		av_error_log_func(ptr, level, fmt, vl);
	}

	char buf[1024];
	if (newline) {
		memcpy(buf, prefix, newline);
	}
	const int ret = vsprintf_s(&buf[newline], std::size(buf) - newline, fmt, vl);
	if (ret > 0) {
		OutputDebugStringA(buf);
		newline = (buf[newline + (ret - 1)] == '\n') ? std::size(prefix) - 1 : 0;
	}
	//DebugBreak(); 
}

void init_av()
{
	if (!av_initialized) {
		av_initialized = 1;

#ifdef _DEBUG
		av_log_set_callback(av_log_func);
		av_log_set_level(av_log_level); // doesn't work for custom callback, but the path will be
		av_log_set_flags(AV_LOG_SKIP_REPEATED);
#else
		av_log_set_callback(av_error_log_func);
		av_log_set_level(AV_LOG_ERROR);
		av_log_set_flags(AV_LOG_SKIP_REPEATED);
#endif
	}
}

std::string get_last_av_error(const char* prefix)
{
	std::lock_guard<std::mutex> lock(ff_error_log_mutex);

	std::string error_text;
	if (prefix) {
		error_text.assign(prefix);
	}
	error_text.append(ff_error_log);

	return error_text;
}
