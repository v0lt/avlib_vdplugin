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
	struct Config :public VDFFAudio::Config {
	} codec_config;

	VDFFAudio_opus(const VDXInputDriverContext& pContext) :VDFFAudio(pContext) {
		config = &codec_config;
		reset_config();
	}
	virtual const char* GetElementaryFormat() { return "ogg"; }
	virtual void CreateCodec();
	virtual void InitContext();
	virtual size_t GetConfigSize() { return sizeof(Config); }
	virtual void reset_config();
	virtual bool HasConfig() { return true; }
	virtual void ShowConfig(VDXHWND parent);
};
