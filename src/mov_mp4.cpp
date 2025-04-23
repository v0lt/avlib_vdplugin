/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mov_mp4.h"

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
	if (!can_read(8)) return false;
	a.pos = offset;
	a.sz = read4();
	a.t = read4();
	a.hsize = 8;

	if (a.sz == 0) {
		a.sz = fileSize;
	}
	else if (a.sz == 1) {
		if (!can_read(8)) return false;
		a.sz = read8();
		if (a.sz < 16) return false;
		a.hsize = 16;
	}
	else if (a.sz < 8) {
		return false;
	}

	return true;
}
