/**
 * @file ProfileManager.cpp
 * @brief Profile index state, registry load/save, and LED refresh on change.
 */

#include "ProfileManager.h"
#include "Settings.h"
#include "LedController.h"
#include <iostream>

size_t ProfileManager::currentProfileIndex = kScrollProfile;

void ProfileManager::Initialize() {
    DWORD saved = 0;
    if (Settings::LoadProfile(saved) && saved < GetProfileList().size()) {
        currentProfileIndex = saved;
        std::wcout << L"[Debug] Loaded profile from registry: " << GetProfileList()[currentProfileIndex] << std::endl;
    }
}

const std::vector<std::wstring>& ProfileManager::GetProfileList() {
    static const std::vector<std::wstring> profiles = {L"Scroll", L"Volume"};
    return profiles;
}

size_t ProfileManager::GetCurrentProfileIndex() {
    return currentProfileIndex;
}

std::wstring ProfileManager::GetCurrentProfileName() {
    const auto& profiles = GetProfileList();
    size_t idx = GetCurrentProfileIndex();
    return (idx < profiles.size()) ? profiles[idx] : L"(Invalid Profile)";
}

void ProfileManager::SetCurrentProfile(int index) {
    if (index >= 0 && static_cast<size_t>(index) < GetProfileList().size()) {
        currentProfileIndex = static_cast<size_t>(index);
        Settings::SaveProfile(static_cast<DWORD>(currentProfileIndex));
        std::wcout << L"[Debug] Current Profile set to: " << GetProfileList()[currentProfileIndex] << std::endl;
        LedController::Refresh();
    } else {
        std::wcout << L"[Error] Invalid profile index" << std::endl;
    }
}
