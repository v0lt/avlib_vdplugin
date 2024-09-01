#ifndef gopro_header
#define gopro_header

#include "mov_mp4.h"
#include <string>

struct GoproInfo {
	struct CameraType {
		const char Compare[7];
		const char Name[25];
	};

	const CameraType* type = nullptr;
	std::string cam_serial;
	std::string firmware;
	std::string setup_info;

	void find_info(const wchar_t* name);
	void get_camera_type();
	void get_settings(unsigned int* sett, int n);
};

#endif
