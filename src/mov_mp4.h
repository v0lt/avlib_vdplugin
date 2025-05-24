/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

struct MovAtom {
	int64_t sz;
	uint32_t t;
	int64_t pos;
	int hsize;
};

struct MovParser {
	const void* buf  = nullptr;
	int     buf_size = 0;

	const   char* p  = nullptr;
	int64_t offset   = 0;
	int64_t fileSize = 0;
	HANDLE  hfile    = NULL;

	MovParser(const void* _buf, int _buf_size, int64_t _fileSize);
	MovParser(const std::wstring& name);
	~MovParser();

	bool can_read(int64_t s);
	bool can_read(MovAtom& a);

	void skip(MovAtom& a);

	void read(MovAtom& a, std::unique_ptr<char[]>& buf, int& size, const int extra = 0);
	uint32_t read4();
	int64_t read8();
	bool read(MovAtom& a);
};
