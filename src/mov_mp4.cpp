/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mov_mp4.h"

MovParser::MovParser(const void* _buf, int _buf_size, int64_t _fileSize)
	: buf(_buf)
	, buf_size(_buf_size)
	, fileSize(_fileSize)
	, p((const char*)_buf)
{
}

MovParser::MovParser(const std::wstring& name)
{
	hfile = CreateFileW(name.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	LARGE_INTEGER s;
	GetFileSizeEx(hfile, &s);
	fileSize = s.QuadPart;
}

MovParser::~MovParser()
{
	if (hfile) {
		CloseHandle(hfile);
	}
}

bool MovParser::can_read(int64_t s)
{
	if (hfile) {
		if (fileSize - offset < 8) return false;
	}
	else {
		if (buf_size - offset < 8) return false;
	}
	return true;
}

bool MovParser::can_read(MovAtom& a)
{
	return offset < a.pos + a.sz;
}

void MovParser::skip(MovAtom& a)
{
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

void MovParser::read(MovAtom& a, std::unique_ptr<char[]>& buf, int& size, const int extra /*= 0*/)
{
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

uint32_t MovParser::read4()
{
	uint32_t r;
	if (hfile) {
		DWORD w;
		ReadFile(hfile, &r, 4, &w, 0);
	}
	else {
		r = *(uint32_t*)p; p += 4;
	}
	offset += 4;
	r = _byteswap_ulong(r);
	return r;
}

int64_t MovParser::read8()
{
	int64_t r;
	if (hfile) {
		DWORD w;
		ReadFile(hfile, &r, 8, &w, 0);
	}
	else {
		r = *(int64_t*)p; p += 8;
	}
	offset += 8;
	r = _byteswap_uint64(r);
	return r;
}

bool MovParser::read(MovAtom& a)
{
	if (!can_read(8)) {
		return false;
	}
	a.pos = offset;
	a.sz = read4();
	a.t = read4();
	a.hsize = 8;

	if (a.sz == 0) {
		a.sz = fileSize;
	}
	else if (a.sz == 1) {
		if (!can_read(8)) {
			return false;
		}
		a.sz = read8();
		if (a.sz < 16) {
			return false;
		}
		a.hsize = 16;
	}
	else if (a.sz < 8) {
		return false;
	}

	return true;
}
