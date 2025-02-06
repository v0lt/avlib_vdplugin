/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <windows.h>
#include <commctrl.h>

#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdinputdriver.h>
#include <vd2/VDXFrame/VideoFilterDialog.h>
#include "InputFile2.h"
#include "export.h"
#include "a_compress.h"
#include "resource.h"
#include "Helper.h"

#pragma comment(lib, "avcodec")
#pragma comment(lib, "avformat")
#pragma comment(lib, "avutil")
#pragma comment(lib, "swscale")
#pragma comment(lib, "swresample")

#pragma comment(lib, "vfw32")

HINSTANCE hInstance;
bool config_decode_raw = false;
bool config_decode_magic = false;
bool config_force_thread = false;
bool config_disable_cache = false;
float config_cache_size = 0.5;
void saveConfig();

int av_initialized = 0;

static int av_log_level = AV_LOG_INFO;

void av_log_func(void* ptr, int level, const char* fmt, va_list vl)
{
	const char prefix[] = "FFLog: ";
	static size_t newline = std::size(prefix) - 1;

	if (level > av_log_level) {
		return;
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
#endif
	}
}

class ConfigureDialog : public VDXVideoFilterDialog {
public:
	virtual INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void Show(HWND parent) {
		VDXVideoFilterDialog::Show(hInstance, MAKEINTRESOURCEW(IDD_SYS_INPUT_OPTIONS), parent);
	}
	void init_cache();
};

float GetDlgItemFloat(HWND wnd, int id, float bv)
{
	HWND w1 = GetDlgItem(wnd, id);
	wchar_t buf[128];
	if (!GetWindowTextW(w1, buf, (int)std::size(buf))) return bv;
	float v;
	if (swscanf_s(buf, L"%f", &v) != 1) return bv;
	return v;
}

void SetDlgItemFloat(HWND wnd, int id, float v)
{
	HWND w1 = GetDlgItem(wnd, id);
	auto str = std::format(L"{:.2}", v);
	SetWindowTextW(w1, str.c_str());
}

void ConfigureDialog::init_cache()
{
	MEMORYSTATUSEX ms = { sizeof(MEMORYSTATUSEX) };
	GlobalMemoryStatusEx(&ms);
	uint64_t gb1 = 0x40000000;
	int max = int(ms.ullTotalPhys / gb1);

	SetDlgItemFloat(mhdlg, IDC_CACHE_SIZE, config_cache_size);
	SendMessageW(GetDlgItem(mhdlg, IDC_CACHE_SPIN), UDM_SETRANGE, 0, (LPARAM)MAKELONG(max, 0));
}

INT_PTR ConfigureDialog::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
		CheckDlgButton(mhdlg, IDC_DECODE_RAW, config_decode_raw ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(mhdlg, IDC_DECODE_MAGIC, config_decode_magic ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(mhdlg, IDC_FORCE_THREAD, config_force_thread ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(mhdlg, IDC_DISABLE_CACHE2, config_disable_cache ? BST_CHECKED : BST_UNCHECKED);
		init_cache();
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			config_decode_raw = IsDlgButtonChecked(mhdlg, IDC_DECODE_RAW) != 0;
			config_decode_magic = IsDlgButtonChecked(mhdlg, IDC_DECODE_MAGIC) != 0;
			config_force_thread = IsDlgButtonChecked(mhdlg, IDC_FORCE_THREAD) != 0;
			config_disable_cache = IsDlgButtonChecked(mhdlg, IDC_DISABLE_CACHE2) != 0;
			config_cache_size = GetDlgItemFloat(mhdlg, IDC_CACHE_SIZE, 0.5);
			saveConfig();
			EndDialog(mhdlg, TRUE);
			return TRUE;

		case IDCANCEL:
			EndDialog(mhdlg, FALSE);
			return TRUE;
		}
	}
	return FALSE;
}

bool VDXAPIENTRY StaticConfigureProc(VDXHWND parent)
{
	ConfigureDialog dlg;
	dlg.Show((HWND)parent);
	return true;
}

///////////////////////////////////////////////////////////////////////////////

bool VDXAPIENTRY ff_create(const VDXInputDriverContext* pContext, IVDXInputFileDriver** ppDriver)
{
	VDFFInputFileDriver* p = new VDFFInputFileDriver(*pContext);
	if (!p) return false;
	*ppDriver = p;
	p->AddRef();
	return true;
}

#define OPTION_VIDEO_INIT L"FFmpeg : video|*.mp4;*.mov;*.mkv;*.webm;*.flv;*.avi;*.nut"
std::wstring option_video = OPTION_VIDEO_INIT;
//std::wstring pattern_video; // example "*.mov|*.mp4|*.avi"

VDXInputDriverDefinition ff_video = {
	sizeof(VDXInputDriverDefinition),
	VDXInputDriverDefinition::kFlagSupportsVideo |
	VDXInputDriverDefinition::kFlagSupportsAudio |
	VDXInputDriverDefinition::kFlagCustomSignature,
	1, //priority, reset from options
	0, //SignatureLength
	0, //Signature
	0, //pattern_video.c_str(),
	option_video.c_str(),
	L"Caching input driver",
	ff_create
};

#define OPTION_IMAGE_INIT L"FFmpeg : images|*.jpg;*.jpeg;*.png;*.tif;*.tiff;*.jxl;*.webp;*.dpx"
std::wstring option_image = OPTION_IMAGE_INIT;

VDXInputDriverDefinition ff_image = {
	sizeof(VDXInputDriverDefinition),
	VDXInputDriverDefinition::kFlagSupportsVideo |
	VDXInputDriverDefinition::kFlagCustomSignature |
	VDXInputDriverDefinition::kFlagDuplicate,
	1, //priority, reset from options
	0, //SignatureLength
	0, //Signature
	0,
	option_image.c_str(),
	L"Caching input driver",
	ff_create
};

#define OPTION_AUDIO_INIT L"FFmpeg : audio|*.wav;*.ogg"
std::wstring option_audio = OPTION_AUDIO_INIT;

VDXInputDriverDefinition ff_audio = {
	sizeof(VDXInputDriverDefinition),
	VDXInputDriverDefinition::kFlagSupportsAudio |
	VDXInputDriverDefinition::kFlagCustomSignature |
	VDXInputDriverDefinition::kFlagDuplicate,
	1, //priority, reset from options
	0, //SignatureLength
	0, //Signature
	0,
	option_audio.c_str(),
	L"Caching input driver",
	ff_create
};

VDXPluginInfo ff_plugin_video = {
	sizeof(VDXPluginInfo),
	L"Caching input driver",
	L"Anton Shekhovtsov",
	L"Loads and decode files through ffmpeg libs.",
	(1 << 24) + (9 << 16),
	kVDXPluginType_Input,
	0,
	10, // min api version
	kVDXPlugin_APIVersion,
	8,  // min input api version
	kVDXPlugin_InputDriverAPIVersion,
	&ff_video
};

VDXPluginInfo ff_plugin_image;
VDXPluginInfo ff_plugin_audio;

VDPluginInfo* kPlugins[] = {
	&ff_output_info,
	&ff_mp3enc_info,
	&ff_aacenc_info,
	&ff_flacenc_info,
	&ff_alacenc_info,
	&ff_vorbisenc_info,
	&ff_opusenc_info,
	&ff_plugin_video,
	&ff_plugin_image,
	&ff_plugin_audio,
	nullptr
};

extern "C" VDPluginInfo * *__cdecl VDGetPluginInfo()
{
	return kPlugins;
}

void saveConfig()
{
	wchar_t buf[MAX_PATH + 128];
	const DWORD len = GetModuleFileNameW(hInstance, buf, MAX_PATH);
	if (len == 0) {
		return;
	}
	buf[len] = 0;

	wchar_t* pSlash = std::max(wcsrchr(buf, '\\'), wcsrchr(buf, '/'));
	if (!pSlash) {
		return;
	}
	*pSlash = 0;

	wcscat_s(buf, L"\\cch_input.ini");

	WritePrivateProfileStringW(L"force_ffmpeg", L"raw", config_decode_raw ? L"1" : L"0", buf);
	WritePrivateProfileStringW(L"force_ffmpeg", L"MagicYUV", config_decode_magic ? L"1" : L"0", buf);
	WritePrivateProfileStringW(L"decode_model", L"force_frame_thread", config_force_thread ? L"1" : L"0", buf);
	WritePrivateProfileStringW(L"decode_model", L"disable_cache", config_disable_cache ? L"1" : L"0", buf);

	auto str = std::format(L"{:.2}", config_cache_size);
	WritePrivateProfileStringW(L"decode_model", L"cache_size", str.c_str(), buf);

	WritePrivateProfileStringW(0, 0, 0, buf);
}

void loadConfig()
{
	wchar_t buf[MAX_PATH + 128];
	const DWORD len = GetModuleFileNameW(hInstance, buf, MAX_PATH);
	if (len == 0) {
		return;
	}
	buf[len] = 0;

	wchar_t* pSlash = std::max(wcsrchr(buf, '\\'), wcsrchr(buf, '/'));
	if (!pSlash) {
		return;
	}
	*pSlash = 0;

	wcscat_s(buf, L"\\cch_input.ini");

	config_decode_raw = GetPrivateProfileIntW(L"force_ffmpeg", L"raw", 0, buf) != 0;
	config_decode_magic = GetPrivateProfileIntW(L"force_ffmpeg", L"MagicYUV", 0, buf) != 0;
	config_force_thread = GetPrivateProfileIntW(L"decode_model", L"force_frame_thread", 0, buf) != 0;
	config_disable_cache = GetPrivateProfileIntW(L"decode_model", L"disable_cache", 0, buf) != 0;

	wchar_t buf2[128];
	GetPrivateProfileStringW(L"decode_model", L"cache_size", L"0.5", buf2, 128, buf);
	float v2;
	if (swscanf_s(buf2, L"%f", &v2) == 1) {
		config_cache_size = v2;
	} else {
		config_cache_size = 0.5;
	}

	ff_plugin_video.mpStaticConfigureProc = 0;

	ff_plugin_image = ff_plugin_video;
	ff_plugin_image.mpTypeSpecificInfo = &ff_image;
	ff_plugin_image.mpName = L"Caching input driver (images)"; // name must be unique

	ff_plugin_audio = ff_plugin_video;
	ff_plugin_audio.mpTypeSpecificInfo = &ff_audio;
	ff_plugin_audio.mpName = L"Caching input driver (audio)"; // name must be unique

	ff_plugin_video.mpStaticConfigureProc = StaticConfigureProc;

	int priority = GetPrivateProfileIntW(L"priority", L"default", ff_video.mPriority, buf);
	ff_video.mPriority = priority;
	ff_image.mPriority = priority;
	ff_audio.mPriority = priority;

	{
		option_video = OPTION_VIDEO_INIT;
		size_t mp = option_video.rfind('|');
		wchar_t mask[2048];
		GetPrivateProfileStringW(L"file_mask", L"video", &option_video[mp + 1], mask, 2048, buf);
		option_video.resize(mp + 1);

		ptrdiff_t p = 0;
		const ptrdiff_t n = wcslen(mask);
		int count1 = 0;
		while (p < n) {
			ptrdiff_t p1 = n;
			wchar_t* p2 = wcschr(mask + p, ';');
			if (p2 && p2 - mask < p1) p1 = p2 - mask;

			if (count1) option_video += L";";
			option_video += std::wstring(mask + p, p1 - p);

			p = p1 + 1;
			count1++;
		}

		ff_video.mpFilenamePattern = option_video.c_str();
	}

	{
		option_image = OPTION_IMAGE_INIT;
		size_t mp = option_image.rfind('|');
		wchar_t mask[2048];
		GetPrivateProfileStringW(L"file_mask", L"images", &option_image[mp + 1], mask, 2048, buf);
		option_image.resize(mp + 1);

		ptrdiff_t p = 0;
		const ptrdiff_t n = wcslen(mask);
		int count1 = 0;
		while (p < n) {
			ptrdiff_t p1 = n;
			wchar_t* p2 = wcschr(mask + p, ';');
			if (p2 && p2 - mask < p1) p1 = p2 - mask;

			if (count1) option_image += L";";
			option_image += std::wstring(mask + p, p1 - p);

			p = p1 + 1;
			count1++;
		}

		ff_image.mpFilenamePattern = option_image.c_str();
	}

	{
		option_audio = OPTION_AUDIO_INIT;
		size_t mp = option_audio.rfind('|');
		wchar_t mask[2048];
		GetPrivateProfileStringW(L"file_mask", L"audio", &option_audio[mp + 1], mask, 2048, buf);
		option_audio.resize(mp + 1);

		ptrdiff_t p = 0;
		const ptrdiff_t n = wcslen(mask);
		int count1 = 0;
		while (p < n) {
			ptrdiff_t p1 = n;
			wchar_t* p2 = wcschr(mask + p, ';');
			if (p2 && p2 - mask < p1) p1 = p2 - mask;

			if (count1) option_audio += L";";
			option_audio += std::wstring(mask + p, p1 - p);

			p = p1 + 1;
			count1++;
		}

		ff_audio.mpFilenamePattern = option_audio.c_str();
	}
}

BOOLEAN WINAPI DllMain(IN HINSTANCE hDllHandle, IN DWORD nReason, IN LPVOID Reserved)
{
	switch (nReason) {
	case DLL_PROCESS_ATTACH:
		hInstance = hDllHandle;
		loadConfig();
		return true;

	case DLL_PROCESS_DETACH:
		return true;
	}

	return true;
}
