/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "AudioEnc.h"

class VDFFAudio_alac final : public VDFFAudio
{
public:
	struct Config :public VDFFAudio::Config {
	} codec_config;

	VDFFAudio_alac(const VDXInputDriverContext& pContext) :VDFFAudio(pContext) {
		config = &codec_config;
		reset_config();
	}
	virtual const char* GetElementaryFormat() { return "caf"; }
	virtual void CreateCodec();
	virtual void InitContext();
	virtual size_t GetConfigSize() { return sizeof(Config); }
	virtual void reset_config();
	virtual bool HasConfig() { return true; }
	virtual void ShowConfig(VDXHWND parent);
};
