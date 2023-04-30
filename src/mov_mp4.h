#ifndef mov_mp4_header
#define mov_mp4_header

#include "windows.h"
#include "inttypes.h"

struct MovAtom {
	__int64 sz;
	unsigned long t;
	__int64 pos;
	int hsize;
};

struct MovParser {
	const void* buf;
	int buf_size;

	const char* p;
	__int64 offset;
	__int64 fileSize;
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

	MovParser(const wchar_t* name)
		: buf(nullptr)
		, buf_size(0)
		, p(nullptr)
		, offset(0)
	{
		hfile = CreateFileW(name, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
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

	void read(MovAtom& a, char*& buf, int& size, int extra = 0) {
		size = int(a.sz - a.hsize);
		buf = (char*)malloc(size + extra);
		memset(buf + size, 0, extra);
		if (hfile) {
			unsigned long w;
			ReadFile(hfile, buf, size, &w, 0);
		}
		else {
			memcpy(buf, p, size); p += size;
		}
		offset += size;
	}

	void skip(MovAtom& a) {
		__int64 d = a.pos + a.sz - offset;
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

	unsigned long read4();
	__int64 read8();
	bool read(MovAtom& a);
};

#endif
