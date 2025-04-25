/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2024 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

struct CodecClass {
	int class_id;

	CodecClass() { class_id = 0; }
	virtual ~CodecClass() {}
};
