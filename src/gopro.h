#ifndef gopro_header
#define gopro_header

#include "mov_mp4.h"

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
		HEROP_LCD
	};

	struct CameraType {
		const char Compare[7];
		CameraEnum type;
		const char Name[25];
		const char ExifModel[25];
	};

	CameraType* type = nullptr;
	char* cam_serial = nullptr;
	char* firmware   = nullptr;
	char* setup_info = nullptr;

	~GoproInfo() {
		free(cam_serial);
		free(firmware);
		free(setup_info);
	}

	void find_info(const wchar_t* name);
	void get_camera_type();
	void get_settings(unsigned int* sett, int n);
};

#endif
