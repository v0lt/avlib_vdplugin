/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "AudioEnc.h"

class VDFFAudio_mp3 final : public VDFFAudio
{
public:
	struct Config :public VDFFAudio::Config {
		int bitrate;
		int quality;
		bool constant_rate;
		bool jointstereo;
	} codec_config;

	VDFFAudio_mp3(const VDXInputDriverContext& pContext) :VDFFAudio(pContext) {
		config = &codec_config;
		reset_config();
		load_config();
	}
	virtual void reset_config() override;
	virtual void load_config() override;
	virtual void save_config() override;

	virtual const char* GetElementaryFormat() { return "mp3"; }
	virtual void CreateCodec();
	virtual void InitContext();
	virtual size_t GetConfigSize() { return sizeof(Config); }
	virtual bool HasConfig() { return true; }
	virtual void ShowConfig(VDXHWND parent);
};
