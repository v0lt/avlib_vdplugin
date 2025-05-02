/*
 * Copyright (C) 2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string>

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
	void ReadBool(LPCSTR valueName, bool& value);

	bool ReadString(LPCSTR valueName, std::string& value);
	size_t CheckString(LPCSTR valueName, LPCSTR* vars, const size_t var_count);

	void WriteInt(LPCSTR valueName, const int value);
	void WriteBool(LPCSTR valueName, const bool value);

	void WriteString(LPCSTR valueName, std::string_view value);
};
