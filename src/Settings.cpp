#include "Settings.h"
#include <iostream>

bool Settings::LoadProfile(DWORD& outIndex) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRootKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = 0;
    DWORD value = 0;
    DWORD size = sizeof(value);
    LONG result = RegQueryValueExW(hKey, kProfileValue, nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size);
    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS || type != REG_DWORD) {
        return false;
    }

    outIndex = value;
    return true;
}

bool Settings::SaveProfile(DWORD index) {
    HKEY hKey = nullptr;
    DWORD disposition = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRootKey, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, &disposition) != ERROR_SUCCESS) {
        std::wcerr << L"[Error] Failed to create settings key\n";
        return false;
    }

    LONG result = RegSetValueExW(hKey, kProfileValue, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&index), sizeof(index));
    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS) {
        std::wcerr << L"[Error] Failed to save Profile setting\n";
        return false;
    }

    return true;
}
