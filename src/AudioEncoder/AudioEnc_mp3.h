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
		int bitrate; // 8...320
		int quality; // 0...9
		bool constant_rate;
		bool jointstereo;

		Config() { reset(); }
		void reset() {
			version = 2;
			bitrate = 320;
			quality = 0;
			constant_rate = true;
			jointstereo = true;
		}
	} codec_config;

	VDFFAudio_mp3(const VDXInputDriverContext& pContext) :VDFFAudio(pContext) {
		config = &codec_config;
		load_config();
	}
	void reset_config() override { codec_config.reset(); }
	void load_config() override;
	void save_config() override;

	const char* GetElementaryFormat() override { return "mp3"; }
	void CreateCodec() override;
	void InitContext() override;
	size_t GetConfigSize() override { return sizeof(Config); }
	bool HasConfig() override { return true; }
	void ShowConfig(VDXHWND parent) override;
};
