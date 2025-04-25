/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2024 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

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

	void find_info(const std::wstring& name);
	void get_camera_type();
	void get_settings(unsigned int* sett, int n);
};
