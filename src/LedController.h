#pragma once

/**
 * @file LedController.h
 * @brief Maps the active profile and system volume onto PowerMate LED brightness/pulse.
 */

/**
 * @brief Applies the LED policy for Scroll vs Volume (and mute).
 */
class LedController {
public:
    /**
     * @brief Queues LED state for the connected device based on profile and volume.
     *
     * Scroll: hardware pulse. Volume unmuted: solid brightness from master volume.
     * Volume muted or scalar &lt;= 0: hardware pulse.
     *
     * @note No-op when the PowerMate is disconnected. HID apply happens on the input thread.
     */
    static void Refresh();
};
