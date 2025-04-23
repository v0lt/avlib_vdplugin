/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

struct IOBuffer {
	uint8_t* data = nullptr;
	int64_t  size = 0;
	int64_t  pos  = 0;

	IOBuffer(const void* data, const int size) {
		this->data = (uint8_t*)av_malloc(size + AVPROBE_PADDING_SIZE);
		this->size = size;
		memcpy(this->data, data, size);
	}

	~IOBuffer() {
		av_free(data);
	}

	static int Read(void* obj, uint8_t* buf, int buf_size) {
		IOBuffer* t = (IOBuffer*)obj;
		const int64_t n = (t->pos + buf_size < t->size) ? buf_size : t->size - t->pos;
		if (n > 0) {
			memcpy(buf, t->data + t->pos, (size_t)n);
			t->pos += n;
			return int(n);
		}
		return AVERROR_EOF;
	}

	static int64_t Seek(void* obj, int64_t offset, int whence) {
		IOBuffer* t = (IOBuffer*)obj;
		if (whence == AVSEEK_SIZE) {
			return t->size;
		}
		if (whence == SEEK_CUR) {
			t->pos += offset;
			return t->pos;
		}
		if (whence == SEEK_SET) {
			t->pos = offset;
			return t->pos;
		}
		if (whence == SEEK_END) {
			t->pos = t->size + offset;
			return t->pos;
		}
		return -1;
	}
};

struct IOWBuffer {
	uint8_t* data = nullptr;
	int64_t  size = 0;
	int64_t  pos  = 0;

	~IOWBuffer() {
		free(data);
	}

	static int Write(void* obj, const uint8_t* buf, int buf_size) {
		IOWBuffer* t = (IOWBuffer*)obj;
		int64_t pos = t->pos;
		if (t->size < pos + buf_size) {
			t->data = (uint8_t*)realloc(t->data, size_t(pos + buf_size));
			t->size = pos + buf_size;
		}
		memcpy(t->data + pos, buf, buf_size);
		t->pos += buf_size;
		return buf_size;
	}

	static int64_t Seek(void* obj, int64_t offset, int whence) {
		IOWBuffer* t = (IOWBuffer*)obj;
		if (whence == AVSEEK_SIZE) {
			return t->size;
		}
		if (whence == SEEK_CUR) {
			t->pos += offset;
			return t->pos;
		}
		if (whence == SEEK_SET) {
			t->pos = offset;
			return t->pos;
		}
		if (whence == SEEK_END) {
			t->pos = t->size + offset;
			return t->pos;
		}
		return -1;
	}
};
