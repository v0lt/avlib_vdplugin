/*
 * Copyright (C) 2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <string>
#include <span>

class RegistryPrefs
{
	HKEY m_key = NULL;
	LPCSTR m_subKey = nullptr;

public:
	RegistryPrefs(LPCSTR subKey);

	~RegistryPrefs();

	LSTATUS OpenKeyRead();
	LSTATUS CreateKeyWrite();
	void CloseKey();

	bool ReadInt(LPCSTR valueName, int& value);
	void ReadInt(LPCSTR valueName, int& value, const int value_min, const int value_max);
	void ReadInt(LPCSTR valueName, int& value, const std::span<const int> vars);
	void ReadInt(LPCSTR valueName, int& value, const int* const vars, const size_t var_count);
	void ReadBool(LPCSTR valueName, bool& value);

	bool ReadString(LPCSTR valueName, std::string& value);
	void CheckString(LPCSTR valueName, int& index, const std::span<LPCSTR> vars);
	void CheckString(LPCSTR valueName, int& index, LPCSTR* vars, const size_t var_count);

	void WriteInt(LPCSTR valueName, const int value);
	void WriteBool(LPCSTR valueName, const bool value);

	void WriteString(LPCSTR valueName, std::string_view value);
};
