#pragma once

#include <Windows.h>

class Settings {
public:
    static constexpr const wchar_t* kRootKey = L"Software\\PowerMateControl";
    static constexpr const wchar_t* kProfileValue = L"Profile";

    // Returns true if Profile was present and written to outIndex.
    static bool LoadProfile(DWORD& outIndex);

    // Creates HKCU\Software\PowerMateControl if needed and writes Profile.
    static bool SaveProfile(DWORD index);
};
