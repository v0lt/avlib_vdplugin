/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "InputFile2.h"
#include "FileInfo2.h"
#include "VideoSource2.h"
#include "AudioSource2.h"
#include "mov_mp4.h"
#include "export.h"
#include <windows.h>
#include <vfw.h>
#include <aviriff.h>
#include "resource.h"
#include "Helper.h"
#include "Utils/StringUtil.h"

typedef struct AVCodecTag {
	enum AVCodecID id;
	unsigned int tag;
} AVCodecTag;

void init_av();

extern bool config_decode_raw;
extern bool config_decode_magic;
extern bool config_disable_cache;

bool FileExist(const wchar_t* name)
{
	DWORD a = GetFileAttributesW(name);
	return ((a != 0xFFFFFFFF) && !(a & FILE_ATTRIBUTE_DIRECTORY));
}

bool check_magic_vfw(DWORD fcc)
{
	if (config_decode_magic) return false;
	HIC codec = ICOpen(ICTYPE_VIDEO, fcc, ICMODE_DECOMPRESS);
	ICClose(codec);
	if (!codec) return false;
	return true;
}

DWORD fcc_toupper(DWORD a)
{
	DWORD r = a;
	char* c = (char*)(&r);
	for (int i = 0; i < 4; i++) {
		int v = c[i];
		if (v >= 'a' && v <= 'z') {
			c[i] = v + 'A' - 'a';
		}
	}
	return r;
}

IVDXInputFileDriver::DetectionConfidence detect_avi(VDXMediaInfo& info, const void* pHeader, int32_t nHeaderSize)
{
	if (nHeaderSize < 64) {
		return IVDXInputFileDriver::kDC_None;
	}
	uint8_t* data = (uint8_t*)pHeader;
	int rsize = nHeaderSize;

	RIFFCHUNK chunk;
	memcpy(&chunk, data, sizeof(chunk)); data += sizeof(chunk); rsize -= sizeof(chunk);
	if (chunk.fcc != FCC('RIFF')) { //RIFF
		return IVDXInputFileDriver::kDC_None;
	}
	DWORD fmt;
	memcpy(&fmt, data, 4); data += 4; rsize -= 4;
	if (fmt != FCC('AVI ')) { //AVI
		return IVDXInputFileDriver::kDC_None;
	}
	memcpy(&chunk, data, sizeof(chunk)); data += sizeof(chunk); rsize -= sizeof(chunk);
	if (chunk.fcc != FCC('LIST')) { //LIST
		return IVDXInputFileDriver::kDC_None;
	}
	memcpy(&fmt, data, 4); data += 4; rsize -= 4;
	if (fmt != FCC('hdrl')) { //hdrl
		return IVDXInputFileDriver::kDC_None;
	}

	memcpy(&chunk, data, sizeof(chunk));
	if (chunk.fcc != ckidMAINAVIHEADER) { //avih
		return IVDXInputFileDriver::kDC_None;
	}

	if (rsize < sizeof(AVIMAINHEADER)) {
		AVIMAINHEADER mh = { 0 };
		memcpy(&mh, data, rsize); data += rsize; rsize = 0;
		return IVDXInputFileDriver::kDC_High;
	}

	AVIMAINHEADER mh;
	memcpy(&mh, data, sizeof(mh)); data += sizeof(mh); rsize -= sizeof(mh);
	info.width = mh.dwWidth;
	info.height = mh.dwHeight;
	wcscpy_s(info.format_name, L"AVI");

	if (rsize < sizeof(chunk)) {
		return IVDXInputFileDriver::kDC_High;
	}
	memcpy(&chunk, data, sizeof(chunk)); data += sizeof(chunk); rsize -= sizeof(chunk);
	if (chunk.fcc != FCC('LIST')) { //LIST
		return IVDXInputFileDriver::kDC_None;
	}

	if (rsize < sizeof(fmt)) {
		return IVDXInputFileDriver::kDC_High;
	}
	memcpy(&fmt, data, 4); data += 4; rsize -= 4;
	if (fmt != ckidSTREAMLIST) { //strl
		return IVDXInputFileDriver::kDC_None;
	}

	if (rsize < sizeof(AVISTREAMHEADER)) {
		return IVDXInputFileDriver::kDC_High;
	}
	AVISTREAMHEADER sh;
	memcpy(&sh, data, sizeof(sh)); data += sizeof(sh); rsize -= sizeof(sh);
	if (sh.fcc != ckidSTREAMHEADER) { //strh
		return IVDXInputFileDriver::kDC_None;
	}

	// reject if there is unsupported video codec
	if (sh.fccType == streamtypeVIDEO) {
		init_av();
		bool have_codec = false;
		DWORD h1 = sh.fccHandler;

		char* ch = (char*)(&h1);
		std::wstring str;
		for (int i = 0; i < 4; i++) {
			wchar_t v = ch[i];
			if (v >= 32) {
				str += v;
			} else {
				str += std::format(L"[{}]", (int)v);
			}
		}
		wcscpy_s(info.vcodec_name, str.c_str());

		// skip internally supported formats
		if (!config_decode_raw) {
			switch (h1) {
			case MKTAG('U', 'Y', 'V', 'Y'):
			case MKTAG('Y', 'U', 'Y', 'V'):
			case MKTAG('Y', 'U', 'Y', '2'):
			case MKTAG('Y', 'V', '2', '4'):
			case MKTAG('Y', 'V', '1', '6'):
			case MKTAG('Y', 'V', '1', '2'):
			case MKTAG('I', '4', '2', '0'):
			case MKTAG('I', 'Y', 'U', 'V'):
			case MKTAG('Y', 'V', 'U', '9'):
			case MKTAG('Y', '8', ' ', ' '):
			case MKTAG('Y', '8', '0', '0'):
			case MKTAG('H', 'D', 'Y', 'C'):
			case MKTAG('N', 'V', '1', '2'):
			case MKTAG('v', '3', '0', '8'):
			case MKTAG('v', '2', '1', '0'):
			case MKTAG('b', '6', '4', 'a'):
			case MKTAG('B', 'R', 'A',  64):
			case MKTAG('b', '4', '8', 'r'):
			case MKTAG('P', '0', '1', '0'):
			case MKTAG('P', '0', '1', '6'):
			case MKTAG('P', '2', '1', '0'):
			case MKTAG('P', '2', '1', '6'):
			case MKTAG('r', '2', '1', '0'):
			case MKTAG('R', '1', '0', 'k'):
			case MKTAG('v', '4', '1', '0'):
			case MKTAG('Y', '4', '1', '0'):
			case MKTAG('Y', '4', '1', '6'):
				return IVDXInputFileDriver::kDC_Moderate;
			}
		}

		if (!have_codec) {
			AVCodecTag* riff_tag = (AVCodecTag*)avformat_get_riff_video_tags();
			while (riff_tag->id != AV_CODEC_ID_NONE) {
				if (riff_tag->tag == h1 || fcc_toupper(riff_tag->tag) == fcc_toupper(h1)) {
					if (riff_tag->id == AV_CODEC_ID_MAGICYUV && check_magic_vfw(h1)) {
						return IVDXInputFileDriver::kDC_Moderate;
					}
					have_codec = true;
					break;
				}
				riff_tag++;
			}
		}

		if (!have_codec) {
			AVCodecTag* mov_tag = (AVCodecTag*)avformat_get_mov_video_tags();
			while (mov_tag->id != AV_CODEC_ID_NONE) {
				if (mov_tag->tag == h1 || fcc_toupper(mov_tag->tag) == fcc_toupper(h1)) {
					if (mov_tag->id == AV_CODEC_ID_MAGICYUV && check_magic_vfw(h1)) {
						return IVDXInputFileDriver::kDC_Moderate;
					}
					have_codec = true;
					break;
				}
				mov_tag++;
			}
		}

		if (!have_codec) {
			return IVDXInputFileDriver::kDC_Moderate;
		}
	}

	return IVDXInputFileDriver::kDC_High;
}

IVDXInputFileDriver::DetectionConfidence detect_mp4_mov(VDXMediaInfo& info, const void* pHeader, int32_t nHeaderSize, int64_t nFileSize)
{
	MovParser parser(pHeader, nHeaderSize, nFileSize);

	MovAtom a;
	if (!parser.read(a)) {
		return IVDXInputFileDriver::kDC_None;
	}
	if (a.sz > nFileSize && nFileSize > 0) {
		return IVDXInputFileDriver::kDC_None;
	}

	switch (a.t) {
	case 'ftyp':
		// http://www.ftyps.com/
		// https://exiftool.org/TagNames/QuickTime.html#FileType
		if (a.sz >= 4) {
			uint32_t type = parser.read4();
			switch (type) {
			case 'mp41':
				wcscpy_s(info.format_name, L"MP4 v1");
				break;
			case 'mp42':
				wcscpy_s(info.format_name, L"MP4 v2");
				break;
			case 'isom':
				wcscpy_s(info.format_name, L"MP4 Base Media v1");
				break;
			case 'iso2':
				wcscpy_s(info.format_name, L"MP4 Base Media v2");
				break;
			case 'iso4':
				wcscpy_s(info.format_name, L"MP4 Base Media v4");
				break;
			case 'avc1':
				wcscpy_s(info.format_name, L"MP4 Base w/ AVC ext");
				break;
			case 'qt  ':
				wcscpy_s(info.format_name, L"Apple QuickTime");
				break;
			case 'mqt ':
				wcscpy_s(info.format_name, L"Sony/Mobile QuickTime");
				break;
			case '3gp4':
			case '3gp5':
				wcscpy_s(info.format_name, L"3GPP Media");
				break;
			case '3g2a':
				wcscpy_s(info.format_name, L"3GPP2 Media");
				break;
			case 'dash':
				wcscpy_s(info.format_name, L"MP4 DASH");
				return IVDXInputFileDriver::kDC_Moderate;
			default:
				wcscpy_s(info.format_name, L"mp4,mov (ftyp=");
				size_t i = wcslen(info.format_name);
				info.format_name[i++] = ((char*)&type)[3];
				info.format_name[i++] = ((char*)&type)[2];
				info.format_name[i++] = ((char*)&type)[1];
				info.format_name[i++] = ((char*)&type)[0];
				info.format_name[i++] = ')';
				info.format_name[i++] = 0;
				return IVDXInputFileDriver::kDC_Moderate;
			}
			return IVDXInputFileDriver::kDC_High;
		}
		break;
	case 'moov':
		wcscpy_s(info.format_name, L"QuickTime Movie");
		return IVDXInputFileDriver::kDC_High;
	case 'wide':
	case 'skip':
	case 'mdat':
	case 'udta':
	case 'free':
	case 'pnot':
		wcscpy_s(info.format_name, L"mov (");
		size_t i = wcslen(info.format_name);
		info.format_name[i++] = ((char*)&a.t)[3];
		info.format_name[i++] = ((char*)&a.t)[2];
		info.format_name[i++] = ((char*)&a.t)[1];
		info.format_name[i++] = ((char*)&a.t)[0];
		info.format_name[i++] = ')';
		info.format_name[i++] = 0;
		return IVDXInputFileDriver::kDC_Moderate;
	}

	return IVDXInputFileDriver::kDC_None;
}

void copyCharToWchar(wchar_t* dst, size_t dst_size, const char* src)
{
	for (size_t i = 0; i < dst_size; i++) {
		int c = src[i];
		dst[i] = c;
		if (c == 0) break;
	}
	dst[dst_size - 1] = 0;
}

IVDXInputFileDriver::DetectionConfidence detect_ff(VDXMediaInfo& info, const void* pHeader, int32_t nHeaderSize, const wchar_t* fileName)
{
	init_av();

	IOBuffer buf;
	buf.copy(pHeader, nHeaderSize);
	AVProbeData pd = { 0 };
	pd.buf = buf.data;
	pd.buf_size = (int)buf.size;

	int score = 0;
	const AVInputFormat* fmt = av_probe_input_format3(&pd, true, &score);
	if (fmt) {
		copyCharToWchar(info.format_name, std::size(info.format_name), fmt->name);
	}
	// some examples
	// mpegts is detected with score 2, can be confused with arbitrary text file
	// ac3 is not detected, but score is raised to 1 (not 0)
	// tga is not detected, score is 0
	// also tga is detected as mp3 with score 1

	if (score == AVPROBE_SCORE_MAX) {
		return IVDXInputFileDriver::kDC_Moderate;
	}

	if (!fileName) {
		if (score > 0) {
			return IVDXInputFileDriver::kDC_VeryLow;
		}
		return IVDXInputFileDriver::kDC_None;
	}

	std::string ff_path = ConvertWideToUtf8(fileName);

	AVFormatContext* ctx = nullptr;
	int err = 0;
	err = avformat_open_input(&ctx, ff_path.c_str(), nullptr, nullptr);
	if (err != 0) {
		return IVDXInputFileDriver::kDC_None;
	}

	if (ctx->iformat == av_find_input_format("avisynth")) {
		// ignore AviSynth scripts
		avformat_close_input(&ctx);
		return IVDXInputFileDriver::kDC_None;
	}

	err = avformat_find_stream_info(ctx, nullptr);
	if (err < 0) {
		avformat_close_input(&ctx);
		return IVDXInputFileDriver::kDC_None;
	}
	fmt = ctx->iformat;
	copyCharToWchar(info.format_name, std::size(info.format_name), fmt->name);
	avformat_close_input(&ctx);

	return IVDXInputFileDriver::kDC_Moderate;
}

//----------------------------------------------------------------------------------------------

VDFFInputFileDriver::VDFFInputFileDriver(const VDXInputDriverContext& context)
	: mContext(context)
{
}

VDFFInputFileDriver::~VDFFInputFileDriver()
{
}

int VDXAPIENTRY VDFFInputFileDriver::DetectBySignature(const void* pHeader, int32_t nHeaderSize, const void* pFooter, int32_t nFooterSize, int64_t nFileSize)
{
	return -1;
}

int VDXAPIENTRY VDFFInputFileDriver::DetectBySignature2(VDXMediaInfo& info, const void* pHeader, int32_t nHeaderSize, const void* pFooter, int32_t nFooterSize, int64_t nFileSize)
{
	return DetectBySignature3(info, pHeader, nHeaderSize, pFooter, nFooterSize, nFileSize, 0);
}

int VDXAPIENTRY VDFFInputFileDriver::DetectBySignature3(VDXMediaInfo& info, const void* pHeader, sint32 nHeaderSize, const void* pFooter, sint32 nFooterSize, sint64 nFileSize, const wchar_t* fileName)
{
	if (fileName) {
		const wchar_t* p = GetFileExt(fileName);
		if (p) {
			std::wstring ext(p);
			str_tolower(ext);
			if (ext == L".avs") {
				// ignore AviSynth scripts by extension
				// because avformat_open_input can take a very long time to open a file (FFmpegSource2)
				return kDC_None;
			}
		}
	}

	auto detConf = detect_avi(info, pHeader, nHeaderSize);
	if (detConf >= kDC_Moderate) {
		return detConf;
	}

	detConf = detect_mp4_mov(info, pHeader, nHeaderSize, nFileSize);
	if (detConf >= kDC_Moderate) {
		return detConf;
	}

	detConf = detect_ff(info, pHeader, nHeaderSize, fileName);

	return detConf;
}

bool VDXAPIENTRY VDFFInputFileDriver::CreateInputFile(uint32_t flags, IVDXInputFile** ppFile)
{
	VDFFInputFile* p = new VDFFInputFile(mContext);
	if (!p) return false;

	if (flags & kOF_AutoSegmentScan) p->auto_append = true;
	if (flags & kOF_SingleFile) p->single_file_mode = true;
	//p->auto_append = true;
	if (config_disable_cache) p->cfg_disable_cache = true;

	*ppFile = p;
	p->AddRef();
	return true;
}

//----------------------------------------------------------------------------------------------

extern HINSTANCE hInstance;
extern bool VDXAPIENTRY StaticConfigureProc(VDXHWND parent);

class FileConfigureDialog : public VDXVideoFilterDialog
{
public:
	VDFFInputFileOptions::Data* data = nullptr;

	virtual INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void Show(HWND parent) {
		VDXVideoFilterDialog::Show(hInstance, MAKEINTRESOURCEW(IDD_INPUT_OPTIONS), parent);
	}
};

INT_PTR FileConfigureDialog::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
		CheckDlgButton(mhdlg, IDC_DISABLE_CACHE, data->disable_cache ? BST_CHECKED : BST_UNCHECKED);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_SYS_OPTIONS:
			StaticConfigureProc((VDXHWND)mhdlg);
			return TRUE;

		case IDC_DISABLE_CACHE:
			data->disable_cache = IsDlgButtonChecked(mhdlg, IDC_DISABLE_CACHE) != 0;
			return TRUE;

		case IDOK:
			EndDialog(mhdlg, TRUE);
			return TRUE;

		case IDCANCEL:
			EndDialog(mhdlg, FALSE);
			return TRUE;
		}
	}
	return FALSE;
}

bool VDXAPIENTRY VDFFInputFile::PromptForOptions(VDXHWND parent, IVDXInputOptions** r)
{
	VDFFInputFileOptions* opt = new VDFFInputFileOptions;
	if (cfg_disable_cache) opt->data.disable_cache = true;

	opt->AddRef();
	*r = opt;
	FileConfigureDialog dlg;
	dlg.data = &opt->data;
	dlg.Show((HWND)parent);
	return true;
}

bool VDXAPIENTRY VDFFInputFile::CreateOptions(const void* buf, uint32_t len, IVDXInputOptions** r)
{
	VDFFInputFileOptions* opt = new VDFFInputFileOptions;
	if (opt->Read(buf, len)) {
		opt->AddRef();
		*r = opt;
		return true;
	}

	delete opt;
	*r = nullptr;
	return false;
}

//----------------------------------------------------------------------------------------------

VDFFInputFile::VDFFInputFile(const VDXInputDriverContext& context)
	: mContext(context)
{
}

VDFFInputFile::~VDFFInputFile()
{
	if (next_segment) {
		next_segment->Release();
	}
	if (video_source) {
		video_source->Release();
	}
	if (audio_source) {
		audio_source->Release();
	}
	if (m_pFormatCtx) {
		avformat_close_input(&m_pFormatCtx);
	}
}

void VDFFInputFile::DisplayInfo(VDXHWND hwndParent)
{
	VDFFInputFileInfoDialog dlg;
	dlg.Show(hwndParent, this);
}

void VDFFInputFile::Init(const wchar_t* szFile, IVDXInputOptions* in_opts)
{
	DLog(L"VDFFInputFile::Init - {}", szFile);

	if (!szFile) {
		mContext.mpCallbacks->SetError("No File Given");
		return;
	}

	if (in_opts) {
		VDFFInputFileOptions* opt = (VDFFInputFileOptions*)in_opts;
		if (opt->data.disable_cache) {
			cfg_disable_cache = true;
		}
	}

	if (cfg_frame_buffers < 1) {
		cfg_frame_buffers = 1;
	}

	m_path = szFile;

	init_av();
	//! this context instance is granted to video stream: wasted in audio-only mode
	// audio will manage its own
	m_pFormatCtx = OpenVideoFile();

	if (auto_append) {
		do_auto_append(szFile);
	}
}

int VDFFInputFile::GetFileFlags()
{
	int flags = VDFFInputFileDriver::kFF_AppendSequence;
	if (is_image_list) flags |= VDFFInputFileDriver::kFF_Sequence;
	return flags;
}

void VDFFInputFile::do_auto_append(const wchar_t* szFile)
{
	const wchar_t* ext = GetFileExt(szFile);
	if (!ext) {
		return;
	}
	if (ext - szFile < 3) {
		return;
	}
	if (ext[-3] == '.' && ext[-2] == '0' && ext[-1] == '0') {
		int x = 1;
		const std::wstring base(szFile, ext - szFile - 3);
		while (1) {
			std::wstring filepath = base + std::format(L".{:02}", x) + ext;
			if (!FileExist(filepath.c_str())) {
				break;
			}
			if (!Append(filepath.c_str())) {
				break;
			}
			x++;
		}
	}
}

bool VDFFInputFile::test_append(VDFFInputFile* f0, VDFFInputFile* f1)
{
	int s0 = f0->find_stream(f0->m_pFormatCtx, AVMEDIA_TYPE_VIDEO);
	int s1 = f1->find_stream(f1->m_pFormatCtx, AVMEDIA_TYPE_VIDEO);
	if (s0 == -1 || s1 == -1) return false;
	AVStream* v0 = f0->m_pFormatCtx->streams[s0];
	AVStream* v1 = f1->m_pFormatCtx->streams[s1];
	if (v0->codecpar->width != v1->codecpar->width) return false;
	if (v0->codecpar->height != v1->codecpar->height) return false;

	s0 = f0->find_stream(f0->m_pFormatCtx, AVMEDIA_TYPE_AUDIO);
	s1 = f1->find_stream(f1->m_pFormatCtx, AVMEDIA_TYPE_AUDIO);
	if (s0 == -1 || s1 == -1) return true;
	AVStream* a0 = f0->m_pFormatCtx->streams[s0];
	AVStream* a1 = f1->m_pFormatCtx->streams[s1];
	if (a0->codecpar->sample_rate != a1->codecpar->sample_rate) return false;

	return true;
}

bool VDXAPIENTRY VDFFInputFile::Append(const wchar_t* szFile)
{
	if (!szFile) return true;

	return Append2(szFile, VDFFInputFileDriver::kOF_SingleFile, 0);
}

bool VDXAPIENTRY VDFFInputFile::Append2(const wchar_t* szFile, int flags, IVDXInputOptions* opts)
{
	if (!szFile) return true;

	VDFFInputFile* head = head_segment ? head_segment : this;
	VDFFInputFile* last = head;
	while (last->next_segment) last = last->next_segment;

	VDFFInputFile* f = new VDFFInputFile(mContext);
	f->head_segment = head;
	if (flags & VDFFInputFileDriver::kOF_AutoSegmentScan) f->auto_append = true; else f->auto_append = false;
	if (flags & VDFFInputFileDriver::kOF_SingleFile) f->single_file_mode = true; else f->single_file_mode = false;
	f->Init(szFile, 0);

	if (!f->m_pFormatCtx) {
		delete f;
		return false;
	}

	if (!test_append(head, f)) {
		mContext.mpCallbacks->SetError("FFMPEG: Couldn't append incompatible formats.");
		delete f;
		return false;
	}

	last->next_segment = f;
	last->next_segment->AddRef();

	if (head->video_source) {
		if (f->GetVideoSource(0, 0)) {
			VDFFVideoSource::ConvertInfo& convertInfo = head->video_source->convertInfo;
			f->video_source->SetTargetFormat(convertInfo.req_format, convertInfo.req_dib, head->video_source);
			VDFFInputFile* f1 = head;
			while (1) {
				f1->video_source->m_streamInfo.mInfo.mSampleCount += f->video_source->sample_count;
				f1 = f1->next_segment;
				if (!f1) break;
				if (f1 == f) break;
			}
		}
		else {
			last->next_segment = nullptr;
			f->Release();
			return false;
		}
	}

	if (head->audio_source) {
		if (f->GetAudioSource(0, 0)) {
			VDFFInputFile* f1 = head;
			while (1) {
				f1->audio_source->m_streamInfo.mSampleCount += f->audio_source->sample_count;
				f1 = f1->next_segment;
				if (!f1) break;
				if (f1 == f) break;
			}
		}
		else {
			// no audio is allowed
			/*
			last->next_segment = nullptr;
			f->Release();
			return false;
			*/
		}
	}

	return true;
}

AVFormatContext* VDFFInputFile::OpenVideoFile()
{
	std::string ff_path = ConvertWideToUtf8(m_path);

	AVFormatContext* fmt = nullptr;
	int err = 0;
	err = avformat_open_input(&fmt, ff_path.c_str(), nullptr, nullptr);
	if (err != 0) {
		mContext.mpCallbacks->SetError("FFMPEG: Unable to open file.");
		return nullptr;
	}

	// I absolutely do not want index getting condensed
	fmt->max_index_size = 512 * 1024 * 1024;

	err = avformat_find_stream_info(fmt, nullptr);
	if (err < 0) {
		mContext.mpCallbacks->SetError("FFMPEG: Couldn't find stream information of file.");
		avformat_close_input(&fmt);
		return nullptr;
	}

	is_image = false;
	is_image_list = false;
	is_anim_image = false;
	is_mp4 = false;

	const char* image_names[] = {
		"image2",
		"bmp_pipe",
		"dds_pipe",
		"dpx_pipe",
		"exr_pipe",
		"j2k_pipe",
		"jpeg_pipe",
		"jpegls_pipe",
		"pam_pipe",
		"pbm_pipe",
		"pcx_pipe",
		"pgmyuv_pipe",
		"pgm_pipe",
		"pictor_pipe",
		"png_pipe",
		"ppm_pipe",
		"psd_pipe",
		"qdraw_pipe",
		"sgi_pipe",
		"sunrast_pipe",
		"tiff_pipe",
		"webp_pipe",
		"jpegxl_pipe",
	};
	const char* single_image_names[] = {
		"apng",
		"gif",
		"fits"
	};
	for (const auto& img_name : image_names) {
		if (strcmp(fmt->iformat->name, img_name) == 0) {
			is_image = true;
			break;
		}
	}
	if (!is_image) {
		for (const auto& img_name : single_image_names) {
			if (strcmp(fmt->iformat->name, img_name) == 0) {
				is_image = true;
				single_file_mode = true;
				break;
			}
		}
	}

	if (is_image && !single_file_mode) {
		const AVInputFormat* fmt_image2 = av_find_input_format("image2");
		if (fmt_image2) {
			AVRational r_fr = fmt->streams[0]->r_frame_rate;
			wchar_t list_path[MAX_PATH];
			int start, count;
			if (detect_image_list(list_path, MAX_PATH, &start, &count)) {
				is_image_list = true;
				auto_append = false;
				avformat_close_input(&fmt);
				ff_path = ConvertWideToUtf8(list_path);
				AVDictionary* options = nullptr;
				av_dict_set_int(&options, "start_number", start, 0);
				if (r_fr.num != 0) {
					std::string str = std::format("{}/{}", r_fr.num, r_fr.den);
					av_dict_set(&options, "framerate", str.c_str(), 0);
				}
				err = avformat_open_input(&fmt, ff_path.c_str(), fmt_image2, &options);
				av_dict_free(&options);
				if (err != 0) {
					mContext.mpCallbacks->SetError("FFMPEG: Unable to open image sequence.");
					avformat_close_input(&fmt);
					return nullptr;
				}
				err = avformat_find_stream_info(fmt, nullptr);
				if (err < 0) {
					mContext.mpCallbacks->SetError("FFMPEG: Couldn't find stream information of file.");
					avformat_close_input(&fmt);
					return nullptr;
				}

				AVStream& st = *fmt->streams[0];
				st.nb_frames = count;
			}
		}
	}

	if (!is_image && fmt->iformat == av_find_input_format("mp4")) {
		is_mp4 = true;
	}

	int st = find_stream(fmt, AVMEDIA_TYPE_VIDEO);
	if (st != -1) {
		// disable unwanted streams
		//const bool is_avi = (fmt->iformat == av_find_input_format("avi"));
		for (int i = 0; i < (int)fmt->nb_streams; i++) {
			if (i == st) {
				continue;
			}
			// this has minor impact on performance, ~10% for video
			fmt->streams[i]->discard = AVDISCARD_ALL;

			// this is HUGE if streams are not evenly interleaved (like VD does by default)
			// this fix is hack, I dont know if it will work for other format
			//?/if (is_avi) fmt->streams[i]->nb_index_entries = 0;
		}
	}

	return fmt;
}

bool VDFFInputFile::detect_image_list(wchar_t* dst, int dst_count, int* start, int* count)
{
	const wchar_t* p = GetFileExt(m_path);
	if (!p) {
		return false;
	}

	int digit0 = -1;
	int digit1 = -1;

	while (p > m_path.c_str()) {
		p--;
		int c = *p;
		if (c == '\\' || c == '/') break;
		if (c >= '0' && c <= '9') {
			if (digit0 == -1) {
				digit0 = int(p - m_path.c_str());
				digit1 = digit0;
			}
			else digit0--;
		}
		else if (digit0 != -1) break;
	}

	if (digit0 == -1) return false;

	char start_buf[128];
	char* s = start_buf;
	for (int i = digit0; i <= digit1; i++) {
		int c = m_path[i];
		*s = c;
		s++;
	}
	*s = 0;

	wchar_t buf[20];
	swprintf_s(buf, L"%%0%dd", digit1 - digit0 + 1);

	wcsncpy_s(dst, dst_count, m_path.c_str(), digit0);
	dst[digit0] = 0;
	wcscat_s(dst, dst_count, buf);
	wcscat_s(dst, dst_count, m_path.c_str() + digit1 + 1);

	sscanf_s(start_buf, "%d", start);
	int n = 0;

	wchar_t test[MAX_PATH];
	while (1) {
		swprintf_s(test, dst, *start + n);
		if (GetFileAttributesW(test) == (DWORD)-1) break;
		n++;
	}

	*count = n;

	return true;
}

int VDFFInputFile::find_stream(AVFormatContext* fmt, AVMediaType type)
{
	int video = -1;
	int r0 = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
	if (r0 >= 0) video = r0;
	if (type == AVMEDIA_TYPE_VIDEO) return video;

	int r1 = av_find_best_stream(fmt, type, -1, video, nullptr, 0);
	if (r1 >= 0) return r1;
	return -1;
}

bool VDFFInputFile::GetVideoSource(int index, IVDXVideoSource** ppVS)
{
	if (ppVS) *ppVS = nullptr;

	if (!m_pFormatCtx) return false;
	if (index != 0) return false;

	index = find_stream(m_pFormatCtx, AVMEDIA_TYPE_VIDEO);

	if (index < 0) return false;

	VDFFVideoSource* pVS = new VDFFVideoSource(mContext);

	if (!pVS) return false;

	if (pVS->initStream(this, index) < 0) {
		delete pVS;
		return false;
	}

	video_source = pVS;
	video_source->AddRef();

	if (ppVS) {
		*ppVS = pVS;
		pVS->AddRef();
	}

	if (next_segment && next_segment->GetVideoSource(0, 0)) {
		video_source->m_streamInfo.mInfo.mSampleCount += next_segment->video_source->m_streamInfo.mInfo.mSampleCount;
	}

	return true;
}

bool VDFFInputFile::GetAudioSource(int index, IVDXAudioSource** ppAS)
{
	if (ppAS) *ppAS = nullptr;

	if (!m_pFormatCtx) return false;

	int s_index = find_stream(m_pFormatCtx, AVMEDIA_TYPE_AUDIO);
	if (index > 0) {
		int n = 0;
		for (int i = 0; i < (int)m_pFormatCtx->nb_streams; i++) {
			AVStream* st = m_pFormatCtx->streams[i];
			if (i == s_index) continue;
			if (st->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) continue;
			if (!st->codecpar->ch_layout.nb_channels) continue;
			if (!st->codecpar->sample_rate) continue;
			n++;
			if (n == index) {
				s_index = i; break;
			}
		}
		if (n != index) return false;
	}

	if (s_index < 0) return false;
	if (m_pFormatCtx->streams[s_index]->codecpar->codec_id == AV_CODEC_ID_NONE) return false;

	VDFFAudioSource* pAS = new VDFFAudioSource(mContext);

	if (!pAS) return false;

	if (pAS->initStream(this, s_index) < 0) {
		delete pAS;
		return false;
	}

	if (ppAS) {
		*ppAS = pAS;
		pAS->AddRef();
	}

	if (index == 0 && !audio_source) {
		audio_source = pAS;
		audio_source->AddRef();
	}

	// delete unused
	pAS->AddRef();
	pAS->Release();

	if (index == 0) {
		if (next_segment && next_segment->GetAudioSource(0, 0)) {
			pAS->m_streamInfo.mSampleCount += next_segment->audio_source->m_streamInfo.mSampleCount;
		}
	}

	return true;
}

int seek_frame(AVFormatContext* s, int stream_index, int64_t timestamp, int flags)
{
	int r = av_seek_frame(s, stream_index, timestamp, flags);
	if (r == -1 && timestamp <= AV_SEEK_START) {
		AVStream* st = s->streams[stream_index];
		if (avformat_index_get_entries_count(st) > 0) {
			timestamp = avformat_index_get_entry(st, 0)->timestamp;
		}
		r = av_seek_frame(s, stream_index, timestamp, flags);
	}
	return r;
}
