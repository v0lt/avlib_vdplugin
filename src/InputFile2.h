/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include <vd2/plugin/vdinputdriver.h>
#include <vd2/VDXFrame/Unknown.h>
#include <windows.h>
#include <string>

extern "C"
{
#include <libavformat/avformat.h>
}

class VDFFVideoSource;
class VDFFAudioSource;

class VDFFInputFileDriver : public vdxunknown<IVDXInputFileDriver>
{
public:

	enum OpenFlags {
		kOF_None = 0,
		kOF_Quiet = 1,
		kOF_AutoSegmentScan = 2,
		kOF_SingleFile = 4,
		kOF_Max = 0xFFFFFFFFUL
	};

	enum FileFlags {
		kFF_Sequence = 1,
		kFF_AppendSequence = 2,
	};

	VDFFInputFileDriver(const VDXInputDriverContext& context);
	~VDFFInputFileDriver();

	int		VDXAPIENTRY DetectBySignature(const void* pHeader, int32_t nHeaderSize, const void* pFooter, int32_t nFooterSize, int64_t nFileSize);
	int		VDXAPIENTRY DetectBySignature2(VDXMediaInfo& info, const void* pHeader, int32_t nHeaderSize, const void* pFooter, int32_t nFooterSize, int64_t nFileSize);
	int		VDXAPIENTRY DetectBySignature3(VDXMediaInfo& info, const void* pHeader, sint32 nHeaderSize, const void* pFooter, sint32 nFooterSize, sint64 nFileSize, const wchar_t* fileName);
	bool	VDXAPIENTRY CreateInputFile(uint32_t flags, IVDXInputFile** ppFile);

protected:
	const VDXInputDriverContext& mContext;
};

class VDFFInputFileOptions : public vdxunknown<IVDXInputOptions>
{
public:
	enum { opt_version = 3 };

#pragma pack(push,1)
	struct Data {
		int version;
		bool disable_cache;

		Data() { clear(); }
		void clear() {
			version = opt_version;
			disable_cache = false;
		}
	} data;
#pragma pack(pop)

	bool Read(const void* src, uint32_t len) {
		if (len <= sizeof(int)) {
			return false;
		}
		Data* d = (Data*)src;
		if (d->version > opt_version || d->version < 1) {
			return false;
		}
		data.clear();
		if (d->version >= 2) {
			data.disable_cache = d->disable_cache;
		}
		return true;
	}

	uint32_t VDXAPIENTRY Write(void* buf, uint32_t buflen) {
		if (buflen < sizeof(Data) || !buf) {
			return sizeof(Data);
		}
		memcpy(buf, &data, sizeof(Data));
		return sizeof(Data);
	}
};

class VDFFInputFile : public vdxunknown<IVDXInputFile>, public IFilterModFileTool
{
public:
	std::wstring m_path;
	int64_t video_start_time = 0;
	bool auto_append         = false;
	bool single_file_mode    = false;

	bool is_image_list = false;
	bool is_image      = false;
	bool is_anim_image = false;

	bool is_mp4 = false;

	int  cfg_frame_buffers = 0;
	bool cfg_disable_cache = false;

	AVFormatContext* m_pFormatCtx = nullptr;
	VDFFVideoSource* video_source = nullptr;
	VDFFAudioSource* audio_source = nullptr;
	VDFFInputFile*   next_segment = nullptr;
	VDFFInputFile*   head_segment = nullptr;

	int VDXAPIENTRY AddRef() {
		return vdxunknown<IVDXInputFile>::AddRef();
	}

	int VDXAPIENTRY Release() {
		return vdxunknown<IVDXInputFile>::Release();
	}

	void* VDXAPIENTRY AsInterface(uint32_t iid)
	{
		if (iid == IFilterModFileTool::kIID) {
			return static_cast<IFilterModFileTool*>(this);
		}

		return vdxunknown<IVDXInputFile>::AsInterface(iid);
	}

	VDFFInputFile(const VDXInputDriverContext& context);
	~VDFFInputFile();

	void VDXAPIENTRY Init(const wchar_t* szFile, IVDXInputOptions* opts);
	bool VDXAPIENTRY Append(const wchar_t* szFile);
	bool VDXAPIENTRY Append2(const wchar_t* szFile, int flags, IVDXInputOptions* opts);

	bool VDXAPIENTRY PromptForOptions(VDXHWND, IVDXInputOptions** r);
	bool VDXAPIENTRY CreateOptions(const void* buf, uint32_t len, IVDXInputOptions** r);
	void VDXAPIENTRY DisplayInfo(VDXHWND hwndParent);

	bool VDXAPIENTRY GetVideoSource(int index, IVDXVideoSource**);
	bool VDXAPIENTRY GetAudioSource(int index, IVDXAudioSource**);
	bool VDXAPIENTRY GetExportMenuInfo(int id, char* name, int name_size, bool* enabled);
	bool VDXAPIENTRY GetExportCommandName(int id, char* name, int name_size);
	bool VDXAPIENTRY ExecuteExport(int id, VDXHWND parent, IProjectState* state);
	int VDXAPIENTRY GetFileFlags();

public:
	AVFormatContext* getContext(void) { return m_pFormatCtx; }
	int find_stream(AVFormatContext* fmt, AVMediaType type);
	AVFormatContext* OpenVideoFile();
	bool detect_image_list(wchar_t* dst, int dst_count, int* start, int* count);
	void do_auto_append(const wchar_t* szFile);

protected:
	const VDXInputDriverContext& mContext;
	static bool test_append(VDFFInputFile* f0, VDFFInputFile* f1);
};

// Don't use INT64_MIN because av_seek_frame may not work correctly. INT64_MIN/2 seems to work fine.
#define AV_SEEK_START (INT64_MIN/2)

int seek_frame(AVFormatContext* s, int stream_index, int64_t timestamp, int flags);
