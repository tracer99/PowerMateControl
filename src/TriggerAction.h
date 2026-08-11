#pragma once

/**
 * @file TriggerAction.h
 * @brief Maps PowerMate input events to Scroll or Volume profile actions.
 */

#include <Windows.h>

/**
 * @brief Decoded PowerMate HID input events delivered to TriggerAction.
 */
enum PowermateInputType {
    ROTATE_LEFT,    /**< Knob rotated left (increases volume in Volume profile). */
    ROTATE_RIGHT,   /**< Knob rotated right (decreases volume in Volume profile). */
    BUTTON_RELEASE, /**< Button released after a press (click / mute / double-click). */
    LONG_PRESS,     /**< Reserved for long-press profile switch (if wired by input path). */
};

/**
 * @brief Dispatches input events according to the current ProfileManager selection.
 */
class TriggerAction {
public:
    /**
     * @brief Performs the Scroll or Volume action for @p inputType.
     * @param inputType Event from the PowerMate input loop.
     * @note Volume changes use AudioVolume (WASAPI) and refresh the LED after success.
     */
    static void HandleAction(PowermateInputType inputType);
};
