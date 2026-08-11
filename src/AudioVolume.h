#pragma once

/**
 * @file AudioVolume.h
 * @brief WASAPI master-volume read/write and change notifications for the default render endpoint.
 */

/**
 * @brief Thin wrapper around IAudioEndpointVolume for the multimedia default render device.
 *
 * @note All public methods take the internal audio mutex. Volume writes do not inject
 *       media keys and therefore do not show the Windows volume OSD.
 */
class AudioVolume {
public:
    /**
     * @brief Activates the default render endpoint and registers a volume-change callback.
     * @return true if monitoring was initialized (or already was).
     * @note Requires COM to already be initialized on the calling thread (STA in wWinMain).
     */
    static bool Initialize();

    /**
     * @brief Unregisters the callback and releases COM endpoint objects.
     */
    static void Shutdown();

    /**
     * @brief Reads master volume scalar and mute for the default render endpoint.
     * @param[out] scalar Volume in [0, 1] on success.
     * @param[out] muted Mute flag on success.
     * @return true if both values were read successfully.
     */
    static bool GetState(float& scalar, bool& muted);

    /**
     * @brief Raises master volume by one endpoint-defined step.
     * @return true on success.
     */
    static bool StepUp();

    /**
     * @brief Lowers master volume by one endpoint-defined step.
     * @return true on success.
     */
    static bool StepDown();

    /**
     * @brief Toggles mute on the default render endpoint.
     * @return true on success.
     */
    static bool ToggleMute();
};
