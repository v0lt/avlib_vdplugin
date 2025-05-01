/*
 * Copyright (C) 2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

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

	void ReadInt(LPCSTR valueName, int& value, const int value_min, const int value_max);
	void ReadInt8(LPCSTR valueName, int8_t& value, const int8_t value_min, const int8_t value_max);
	void ReadBool(LPCSTR valueName, bool& value);

	void WriteInt(LPCSTR valueName, const int value);
	void WriteInt8(LPCSTR valueName, const int8_t value);
	void WriteBool(LPCSTR valueName, const bool value);
};
