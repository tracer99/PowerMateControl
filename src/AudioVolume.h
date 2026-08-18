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
     * @brief Releases and re-activates the default render endpoint.
     * @return true if the endpoint is usable again.
     * @note Must run on the COM-initialized UI thread (see NeedsReset).
     */
    static bool Reset();

    /**
     * @brief True when a call saw an invalidated endpoint and a Reset is owed.
     *
     * Volume calls arrive on the HID input thread, which has no COM apartment, so
     * recovery is deferred to the UI thread's watchdog instead of done in place.
     */
    static bool NeedsReset();

    /** @brief Marks the endpoint as needing re-activation (e.g. after system resume). */
    static void MarkForReset();

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
