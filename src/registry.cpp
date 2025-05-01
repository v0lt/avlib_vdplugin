/*
 * Copyright (C) 2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <windows.h>
#include <stdint.h>
#include "registry.h"

#include <cassert>
 
RegistryPrefs::RegistryPrefs(LPCSTR subKey)
{
	m_subKey = subKey;
}

RegistryPrefs::~RegistryPrefs()
{
	CloseKey();
}

LSTATUS RegistryPrefs::OpenKeyRead() {
	assert(m_key == NULL);
	assert(m_subKey);
	LSTATUS lRes = ::RegOpenKeyExA(HKEY_CURRENT_USER, m_subKey, 0, KEY_READ, &m_key);
	return lRes;
}

LSTATUS RegistryPrefs::CreateKeyWrite() {
	assert(m_key == NULL);
	assert(m_subKey);
	LSTATUS lRes = ::RegCreateKeyExA(HKEY_CURRENT_USER, m_subKey, 0, 0, 0, KEY_WRITE, 0, &m_key, 0);
	return lRes;
}

void RegistryPrefs::CloseKey()
{
	if (m_key) {
		::RegCloseKey(m_key);
		m_key = NULL;
	}
}

void RegistryPrefs::ReadInt(LPCSTR valueName, int& value, const int value_min, const int value_max)
{
	assert(m_key);
	DWORD dwValue;
	DWORD dwType;
	ULONG nBytes = sizeof(DWORD);
	LSTATUS lRes = ::RegQueryValueExA(m_key, valueName, nullptr, &dwType, reinterpret_cast<LPBYTE>(&dwValue), &nBytes);
	if (lRes == ERROR_SUCCESS && dwType == REG_DWORD) {
		const int testValue = (int)dwValue;
		if (testValue >= value_min && testValue <= value_max) {
			value = testValue;
		}
	}
}

void RegistryPrefs::ReadInt8(LPCSTR valueName, int8_t& value, const int8_t value_min, const int8_t value_max)
{
	assert(m_key);
	DWORD dwValue;
	DWORD dwType;
	ULONG nBytes = sizeof(DWORD);
	LSTATUS lRes = ::RegQueryValueExA(m_key, valueName, nullptr, &dwType, reinterpret_cast<LPBYTE>(&dwValue), &nBytes);
	if (lRes == ERROR_SUCCESS && dwType == REG_DWORD && dwValue <= UINT8_MAX) {
		const int8_t testValue = (int8_t)dwValue;
		if (testValue >= value_min && testValue <= value_max) {
			value = testValue;
		}
	}
}

void RegistryPrefs::ReadBool(LPCSTR valueName, bool& value)
{
	assert(m_key);
	DWORD dwValue;
	DWORD dwType;
	ULONG nBytes = sizeof(DWORD);
	LSTATUS lRes = ::RegQueryValueExA(m_key, valueName, nullptr, &dwType, reinterpret_cast<LPBYTE>(&dwValue), &nBytes);
	if (lRes == ERROR_SUCCESS && dwType == REG_DWORD) {
		if (dwValue == 0 || dwValue == 1) {
			value = !!dwValue;
		}
	}
}

void RegistryPrefs::WriteInt(LPCSTR valueName, const int value)
{
	assert(m_key);
	DWORD dwValue = (DWORD)value;
	LSTATUS lRes = ::RegSetValueExA(m_key, valueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwValue), sizeof(DWORD));
}

void RegistryPrefs::WriteInt8(LPCSTR valueName, const int8_t value)
{
	assert(m_key);
	DWORD dwValue = (DWORD)value;
	LSTATUS lRes = ::RegSetValueExA(m_key, valueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwValue), sizeof(DWORD));
}

void RegistryPrefs::WriteBool(LPCSTR valueName, const bool value)
{
	assert(m_key);
	DWORD dwValue = (value == false) ? 0 : 1;
	LSTATUS lRes = ::RegSetValueExA(m_key, valueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwValue), sizeof(DWORD));
}
