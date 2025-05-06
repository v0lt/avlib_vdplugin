/*
 * Copyright (C) 2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <windows.h>
#include <stdint.h>
#include "Utils/StringUtil.h"
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

bool RegistryPrefs::ReadInt(LPCSTR valueName, int& value)
{
	assert(m_key);
	DWORD dwValue;
	DWORD dwType;
	ULONG nBytes = sizeof(DWORD);
	LSTATUS lRes = ::RegQueryValueExA(m_key, valueName, nullptr, &dwType, reinterpret_cast<LPBYTE>(&dwValue), &nBytes);
	if (lRes == ERROR_SUCCESS && dwType == REG_DWORD) {
		value = (int)dwValue;
		return true;
	}
	return false;
}

void RegistryPrefs::ReadInt(LPCSTR valueName, int& value, const int value_min, const int value_max)
{
	int testValue;
	if (ReadInt(valueName, testValue) && testValue >= value_min && testValue <= value_max) {
		value = testValue;
	}
}

void RegistryPrefs::ReadInt(LPCSTR valueName, int& value, const std::span<const int> vars)
{
	int testValue;
	if (ReadInt(valueName, testValue)) {
		for (const auto var : vars) {
			if (testValue == var) {
				value = testValue;
				return;
			}
		}
	}
}

void RegistryPrefs::ReadInt(LPCSTR valueName, int& value, const int* const vars, const size_t var_count)
{
	ReadInt(valueName, value, std::span(vars, var_count));
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

bool RegistryPrefs::ReadString(LPCSTR valueName, std::string& value)
{
	DWORD dwType;
	ULONG nBytes = sizeof(DWORD);
	LSTATUS lRes = ::RegQueryValueExA(m_key, valueName, nullptr, &dwType, nullptr, &nBytes);
	if (lRes == ERROR_SUCCESS && dwType == REG_SZ) {
		value.assign(nBytes, 0);
		lRes = ::RegQueryValueExA(m_key, valueName, nullptr, &dwType, reinterpret_cast<LPBYTE>(value.data()), &nBytes);
		if (lRes == ERROR_SUCCESS && dwType == REG_SZ) {
			str_truncate_after_null(value);
			return true;
		}
	}
	return false;
}

size_t RegistryPrefs::CheckString(LPCSTR valueName, const std::span<LPCSTR> vars)
{
	std::string str;
	if (ReadString(valueName, str)) {
		for (size_t i = 0; i < vars.size(); i++) {
			if (str.compare(vars[i]) == 0) {
				return i;
			}
		}
	}
	return (size_t)-1;
}

size_t RegistryPrefs::CheckString(LPCSTR valueName, LPCSTR* vars, const size_t var_count)
{
	return CheckString(valueName, std::span(vars, var_count));
}

void RegistryPrefs::WriteInt(LPCSTR valueName, const int value)
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

void RegistryPrefs::WriteString(LPCSTR valueName, std::string_view value)
{
	LSTATUS lRes = ::RegSetValueExA(m_key, valueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.data()), (DWORD)(value.size() + 1) * sizeof(wchar_t));
}
