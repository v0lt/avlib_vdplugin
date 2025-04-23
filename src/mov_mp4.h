/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef mov_mp4_header
#define mov_mp4_header

#include "windows.h"
#include "inttypes.h"
#include <memory>
#include <string>

struct MovAtom {
	int64_t sz;
	uint32_t t;
	int64_t pos;
	int hsize;
};

struct MovParser {
	const void* buf;
	int buf_size;

	const char* p;
	int64_t offset;
	int64_t fileSize;
	HANDLE hfile;

	MovParser(const void* _buf, int _buf_size, int64_t _fileSize)
		: buf(_buf)
		, buf_size(_buf_size)
		, fileSize(_fileSize)
		, p((const char*)_buf)
		, offset(0)
		, hfile(nullptr)
	{
	}

	MovParser(const std::wstring& name)
		: buf(nullptr)
		, buf_size(0)
		, p(nullptr)
		, offset(0)
	{
		hfile = CreateFileW(name.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
		LARGE_INTEGER s;
		GetFileSizeEx(hfile, &s);
		fileSize = s.QuadPart;
	}

	~MovParser() {
		if (hfile) CloseHandle(hfile);
	}

	bool can_read(int64_t s) {
		if (hfile) {
			if (fileSize - offset < 8) return false;
		}
		else {
			if (buf_size - offset < 8) return false;
		}
		return true;
	}

	bool can_read(MovAtom& a) {
		return offset < a.pos + a.sz;
	}

	void read(MovAtom& a, std::unique_ptr<char[]>& buf, int& size, const int extra = 0) {
		size = int(a.sz - a.hsize);
		buf.reset(new char[size + extra]);
		memset(buf.get() + size, 0, extra);
		if (hfile) {
			DWORD w;
			ReadFile(hfile, buf.get(), size, &w, 0);
		}
		else {
			memcpy(buf.get(), p, size); p += size;
		}
		offset += size;
	}

	void skip(MovAtom& a) {
		int64_t d = a.pos + a.sz - offset;
		offset += d;
		if (hfile) {
			LARGE_INTEGER distance;
			distance.QuadPart = offset;
			SetFilePointerEx(hfile, distance, 0, FILE_BEGIN);
		}
		else {
			p += d;
		}
	}

	uint32_t read4();
	int64_t read8();
	bool read(MovAtom& a);
};

#endif
