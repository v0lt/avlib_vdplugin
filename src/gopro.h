#ifndef gopro_header
#define gopro_header

#include "mov_mp4.h"
#include <string>

struct GoproInfo {
	enum CameraEnum {
		NONE = 0,
		HERO6_Black,
		HERO5_Session,
		HERO5_Black,
		HERO4_Session,
		HERO4_Black,
		HERO4_Silver,
		HERO3P_Black,
		HERO3P_Silver,
		HERO3_Black,
		HERO3_Silver,
		HERO3_White,
		HERO,
		HEROP,
		HEROP_LCD,
		HERO9,
		HERO10,
		HERO12,
	};

	struct CameraType {
		const char Compare[7];
		CameraEnum type;
		const char Name[25];
		const char ExifModel[25];
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
