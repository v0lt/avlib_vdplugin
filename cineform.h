#ifndef cineform_header
#define cineform_header

#include <vd2/plugin/vdplugin.h>
#include <string>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
}

#include <windows.h>
#include <vfw.h>

struct AVCodecContext;

const AVCodecID CFHD_ID = (AVCodecID)MKBETAG('C','F','H','D');
const unsigned CFHD_TAG = MKTAG('C', 'F', 'H', 'D');

void av_register_cfhd();
void cfhd_set_buffer(AVCodecContext* avctx, void* buf);
void cfhd_set_use_am(AVCodecContext* avctx, bool v);
void cfhd_set_format(AVCodecContext* avctx, nsVDXPixmap::VDXPixmapFormat fmt);
bool cfhd_test_format(AVCodecContext* avctx, nsVDXPixmap::VDXPixmapFormat fmt);
void cfhd_get_info(AVCodecContext* avctx, FilterModPixmapInfo& info);
std::string cfhd_format_name(AVCodecContext* avctx);

LRESULT WINAPI DriverProc_CF(DWORD_PTR dwDriverId, HDRVR hDriver, UINT uMsg, LPARAM lParam1, LPARAM lParam2);
LRESULT WINAPI VDDriverProc_CF(DWORD_PTR dwDriverId, HDRVR hDriver, UINT uMsg, LPARAM lParam1, LPARAM lParam2);

#endif
