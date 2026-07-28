#pragma once

#include <Windows.h>

class Settings {
public:
    static constexpr const wchar_t* kRootKey = L"Software\\PowerMateControl";
    static constexpr const wchar_t* kProfileValue = L"Profile";
    static constexpr const wchar_t* kAutostartValue = L"Autostart";

    // Returns true if Profile was present and written to outIndex.
    static bool LoadProfile(DWORD& outIndex);

    // Creates HKCU\Software\PowerMateControl if needed and writes Profile.
    static bool SaveProfile(DWORD index);

    // Returns true if Autostart was present and written to outEnabled.
    static bool LoadAutostart(bool& outEnabled);

    // Creates HKCU\Software\PowerMateControl if needed and writes Autostart (0/1).
    static bool SaveAutostart(bool enabled);
};
