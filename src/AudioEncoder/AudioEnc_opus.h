/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "AudioEnc.h"

class VDFFAudio_opus final : public VDFFAudio
{
public:
	enum { flag_limited_rate = 4 };
	struct Config : public VDFFAudio::Config {
		int bitrate_per_channel; // 6...256
		int quality;             // 0...10
		int8_t bitrate_mode;     // 0 - CBR, 1 - VBR, 2 - Constrained VBR

		Config() { reset(); }
		void reset() {
			version = 2;
			bitrate_per_channel = 64;
			quality = 10;
			bitrate_mode = 0;
		}
	} codec_config;

	VDFFAudio_opus(const VDXInputDriverContext& pContext) :VDFFAudio(pContext) {
		config = &codec_config;
		load_config();
	}
	virtual void reset_config() override { codec_config.reset(); }
	virtual void load_config() override;
	virtual void save_config() override;

	virtual const char* GetElementaryFormat() { return "ogg"; }
	virtual void CreateCodec();
	virtual void InitContext();
	virtual size_t GetConfigSize() { return sizeof(Config); }
	virtual bool HasConfig() { return true; }
	virtual void ShowConfig(VDXHWND parent);
};
