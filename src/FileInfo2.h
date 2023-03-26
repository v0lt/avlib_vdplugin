#pragma once

#define WIN32_LEAN_AND_MEAN

#include <vd2/plugin/vdinputdriver.h>
#include <vd2/VDXFrame/VideoFilterDialog.h>

extern "C" {
#include <libavutil/pixdesc.h>
#include <libavformat/avformat.h>
}

class VDFFInputFile;

class VDFFInputFileInfoDialog : public VDXVideoFilterDialog {
public:
	VDFFInputFileInfoDialog(){
    source = 0;
    segment = 0;
    //pFormatCtx = 0;
    //pVideoStream = 0;
    //pAudioStream = 0;
  }
	bool Show(VDXHWND parent, VDFFInputFile* pInput);

	virtual INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);

private:
	VDFFInputFile* source;
  //AVFormatContext* pFormatCtx;
  //AVStream* pVideoStream;
  //AVStream* pAudioStream;

	VDFFInputFile* segment;
  int segment_pos;
  int segment_count;

  void load_segment();
  void print_format();
  void print_video();
  void print_audio();
  void print_metadata();
  void print_performance();
};
