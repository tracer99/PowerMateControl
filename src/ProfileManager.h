#pragma once

/**
 * @file ProfileManager.h
 * @brief Scroll / Volume profile selection and registry persistence.
 */

#include <string>
#include <vector>

/**
 * @brief Tracks the active control profile and syncs it with Settings + LED state.
 */
class ProfileManager {
public:
    /**
     * @brief Loads the persisted profile index before the tray UI starts.
     * @note Invalid or missing registry values keep the default (Scroll).
     */
    static void Initialize();

    /**
     * @brief Switches the active profile, persists it, and refreshes the LED.
     * @param index Profile index (see kScrollProfile / kVolumeProfile).
     */
    static void SetCurrentProfile(int index);

    /**
     * @brief Returns the display names of available profiles (Scroll, Volume).
     */
    static const std::vector<std::wstring>& GetProfileList();

    /**
     * @brief Returns the index of the currently selected profile.
     */
    static size_t GetCurrentProfileIndex();

    /**
     * @brief Returns the display name of the current profile.
     */
    static std::wstring GetCurrentProfileName();

    static constexpr size_t kScrollProfile = 0; /**< Mouse wheel / double-click profile. */
    static constexpr size_t kVolumeProfile = 1; /**< System volume / mute profile. */

private:
    static size_t currentProfileIndex;
};
