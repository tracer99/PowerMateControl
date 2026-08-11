#pragma once

/**
 * @file Settings.h
 * @brief HKCU registry persistence for PowerMateControl preferences.
 */

#include <Windows.h>

/**
 * @brief Loads and saves values under HKCU\\Software\\PowerMateControl.
 */
class Settings {
public:
    /** @brief Registry key path under HKEY_CURRENT_USER. */
    static constexpr const wchar_t* kRootKey = L"Software\\PowerMateControl";
    /** @brief REG_DWORD: active profile index (0 = Scroll, 1 = Volume). */
    static constexpr const wchar_t* kProfileValue = L"Profile";
    /** @brief REG_DWORD: preferred Run-at-startup flag (0/1), mirrored with the Run key. */
    static constexpr const wchar_t* kAutostartValue = L"Autostart";

    /**
     * @brief Reads the Profile DWORD if present.
     * @param[out] outIndex Receives the stored profile index.
     * @return true if the value existed and was a REG_DWORD.
     */
    static bool LoadProfile(DWORD& outIndex);

    /**
     * @brief Creates the settings key if needed and writes Profile.
     * @param index Profile index to persist.
     * @return true on successful write.
     */
    static bool SaveProfile(DWORD index);

    /**
     * @brief Reads the Autostart DWORD if present.
     * @param[out] outEnabled Receives true when the stored value is non-zero.
     * @return true if the value existed and was a REG_DWORD.
     */
    static bool LoadAutostart(bool& outEnabled);

    /**
     * @brief Creates the settings key if needed and writes Autostart as 0 or 1.
     * @param enabled Preferred autostart state.
     * @return true on successful write.
     */
    static bool SaveAutostart(bool enabled);
};
