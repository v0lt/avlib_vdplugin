/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "stdafx.h"

#include "gopro.h"
#include <span>
#include "Helper.h"

// parts from GoProInfo - HYPOXIC

void GoproInfo::get_camera_type()
{
	// https://github.com/KonradIT/gopro-firmware-archive/blob/main/README.md
	static const CameraType cam_types[] = {
		{ "HD3.01", "HERO3 White"       },
		{ "HD3.02", "HERO3 Silver"      },
		{ "HD3.03", "HERO3 Black"       },
		{ "HD3.10", "HERO3+ Silver"     },
		{ "HD3.11", "HERO3+ Black"      },
		{ "HD3.20", "HERO 2014"         },
		{ "HD3.21", "HERO+ LCD"         },
		{ "HD3.22", "HERO+"             },
		{ "HDX.01", "HERO Session"      },
		{ "HD4.01", "HERO4 Silver"      },
		{ "HD4.02", "HERO4 Black"       },
		{ "HD5.02", "HERO5 Black"       },
		{ "HD5.03", "HERO5 Session"     },
		{ "HD6.01", "HERO6 Black"       },
		{ "FS1.04", "Fusion"            },
		{ "HD7.01", "HERO7"             },
		{ "H18.01", "HERO 2018"         },
		{ "H18.02", "HERO7 White"       },
		{ "H18.03", "HERO7 Silver"      },
		{ "H19.03", "MAX"               },
		{ "HD8.01", "HERO8 Black"       },
		{ "HD9.01", "HERO9 Black"       },
		{ "H21.01", "HERO10 Black"      },
		{ "H22.01", "HERO11 Black"      },
		{ "H22.03", "HERO11 Black Mini" },
		{ "H23.01", "HERO12 Black"      },
	};

	if (firmware.length() >= 6) {
		for (const auto& cam_type : cam_types) {
			if (memcmp(firmware.data(), cam_type.Compare, 6) == 0) {
				type = &cam_type;
				break;
			}
		}
	}
}

const char* strs_vmode[] = { "Video","Timelapse Video","Looping","Video/Photo" };
const char* strs_fov[] = { "Wide","Medium","Narrow" };

const char* strs_orient[] = { "Up","Down" };

const char* strs_OnOff[] = { "On","Off" };
const char* strs_OffOn[] = { "Off","On" };

const char* strs_pt_wb[] = { "Auto*", "3000K", "5500K", "6500K", "Native" };
const char* strs_pt_color[] = { "Flat","GoPro Color*" };
const char* strs_pt_iso_video[] = { "6400*","3200","1600","800","400" };
const char* strs_pt_sharpness[] = { "Low","Medium","High*" };
const char* strs_pt_ev[] = { "+2","+1.5","+1","+0.5","0","-0.5","-1","-1.5","-2" };
const char* str_unknown = "Unknown";

const char* DecipherValue(const std::span<const char*> strarray, unsigned int value)
{
	if (value < strarray.size()) {
		return(strarray[value]);
	} else {
		return str_unknown;
	}
}

void GoproInfo::get_settings(unsigned int* sett, int n)
{
	unsigned int sett1 = sett[0];
	unsigned int sett2 = sett[1];

	// setting 1
	unsigned int mode           =  sett1 & 0xf;
	unsigned int submode        = (sett1 >> 4)  & ((1 << 4) - 1);
	unsigned int timelapse_rate = (sett1 >> 8)  & ((1 << 4) - 1);
	unsigned int orientation    = (sett1 >> 16) & ((1 << 1) - 1);
	unsigned int spotmeter      = (sett1 >> 17) & ((1 << 1) - 1);
	unsigned int protune        = (sett1 >> 30) & ((1 << 1) - 1);
	//unsigned int white_bal = 0;

	// setting 2
	unsigned int fov               = (sett2 >> 1)    & ((1 << 2) - 1);
	unsigned int lowlight          = (sett2 >> 4)    & ((1 << 1) - 1);
	unsigned int superview         = (sett2 >> 5)    & ((1 << 1) - 1);
	unsigned int protune_sharpness = (sett2 >> 6)    & ((1 << 2) - 1);
	unsigned int protune_color     = (sett2 >> 8)    & ((1 << 1) - 1);
	unsigned int protune_iso       = (sett2 >> 9)    & ((1 << 3) - 1);
	unsigned int protune_ev        = (sett2 >> 0xc)  & ((1 << 4) - 1);
	unsigned int protune_wb        = (sett2 >> 0x10) & ((1 << 2) - 1);
	unsigned int broadcast_privacy = (sett2 >> 0x12) & ((1 << 2) - 1);

	setup_info.reserve(300);

	// 0 video
	if (mode) {
		setup_info += std::format("\tmode:\t{}\r\n", mode);
		setup_info += std::format("\tsubmode:\t{}\r\n", submode);
	}
	else {
		setup_info += std::format("\tmode:\t{}\r\n", DecipherValue(strs_vmode, submode));
	}

	setup_info += std::format("\torientation:\t{}\r\n", DecipherValue(strs_orient, orientation));
	setup_info += std::format("\tspotmeter:\t{}\r\n", DecipherValue(strs_OffOn, spotmeter));

	if (n > 1) {
		setup_info += std::format("\tfov:\t{}\r\n", DecipherValue(strs_fov, fov));

		setup_info += std::format("\tlowlight:\t{}\r\n", DecipherValue(strs_OffOn, lowlight));
		setup_info += std::format("\tsuperview:\t{}\r\n", DecipherValue(strs_OffOn, superview));
	}

	setup_info += std::format("\tprotune:\t{}\r\n", DecipherValue(strs_OffOn, protune));
	if (protune && n > 1) {
		setup_info += std::format("\tprotune_wb:\t{}\r\n", DecipherValue(strs_pt_wb, protune_wb));
		setup_info += std::format("\tprotune_color:\t{}\r\n", DecipherValue(strs_pt_color, protune_color));
		setup_info += std::format("\tprotune_iso:\t{}\r\n", DecipherValue(strs_pt_iso_video, protune_iso));
		setup_info += std::format("\tprotune_sharpness:\t{}\r\n", DecipherValue(strs_pt_sharpness, protune_sharpness));
		setup_info += std::format("\tprotune_ev:\t{}\r\n", DecipherValue(strs_pt_ev, protune_ev));
	}
}

void GoproInfo::find_info(const std::wstring& name)
{
	MovParser parser(name);
	while (1) {
		MovAtom a;
		if (!parser.read(a)) return;
		if (a.t == 'moov') {
			while (parser.can_read(a)) {
				MovAtom b;
				if (!parser.read(b)) return;
				if (b.t == 'udta') {
					while (parser.can_read(b)) {
						MovAtom c;
						if (!parser.read(c)) return;

						switch (c.t) {
						case 'FIRM':
						{
							std::unique_ptr<char[]> buf;
							int size;
							parser.read(c, buf, size, 1);
							firmware = buf.get();
							get_camera_type();
						}
						break;

						case 'LENS':
							break;

						case 'CAME':
						{
							std::unique_ptr<char[]> buf;
							int size;
							parser.read(c, buf, size);
							cam_serial.reserve(size * 2);
							for (int i = 0; i < size; i++) {
								unsigned val = ((unsigned char*)buf.get())[i];
								cam_serial += std::format("{:02x}", val);
							}
						}
						break;

						case 'SETT':
						{
							unsigned int sett[3] = { 0,0,0 };
							int n = 1;
							sett[0] = parser.read4();
							if (parser.can_read(c)) {
								sett[1] = parser.read4();
								n++;
							}
							if (parser.can_read(c)) {
								sett[2] = parser.read4(); // does not exist with GoPRO HERO3
								n++;
							}
							get_settings(sett, n);
						}
						break;

						case 'MUID':
							break;

						case 'HMMT':
							break;
						};

						parser.skip(c);
					}
				}
				parser.skip(b);
			}
			break;
		}
		parser.skip(a);
	}
}
