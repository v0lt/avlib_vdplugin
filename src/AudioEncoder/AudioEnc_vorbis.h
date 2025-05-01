/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "AudioEnc.h"

class VDFFAudio_vorbis final : public VDFFAudio
{
public:
	struct Config : public VDFFAudio::Config {
		int bitrate_per_channel; // 32...240
		int quality;             // -1...10
		bool constant_rate;

		Config() { reset(); }
		void reset() {
			version = 2;
			bitrate_per_channel = 160;
			quality = 3;
			constant_rate = false;
		}
	} codec_config;

	VDFFAudio_vorbis(const VDXInputDriverContext& pContext) :VDFFAudio(pContext) {
		config = &codec_config;
		reset_config();
		load_config();
	}

	virtual void reset_config() override { codec_config.reset(); }
	virtual void load_config() override;
	virtual void save_config() override;

	virtual const char* GetElementaryFormat() { return "ogg"; }
	int SuggestFileFormat(const char* name);
	virtual void CreateCodec();
	virtual void InitContext();
	virtual size_t GetConfigSize() { return sizeof(Config); }
	virtual bool HasConfig() { return true; }
	virtual void ShowConfig(VDXHWND parent);
};
