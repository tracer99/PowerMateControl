#pragma once
#include <string>
#include <vector>

class ProfileManager {
public:
    // Load persisted profile (if any) before the tray UI starts.
    static void Initialize();

    static void SetCurrentProfile(int index);
    static const std::vector<std::wstring>& GetProfileList();
    static size_t GetCurrentProfileIndex();
    static std::wstring GetCurrentProfileName();

    static constexpr size_t kScrollProfile = 0;
    static constexpr size_t kVolumeProfile = 1;

private:
    static size_t currentProfileIndex;
};
