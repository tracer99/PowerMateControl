/**
 * @file TriggerAction.cpp
 * @brief Scroll (SendInput) and Volume (WASAPI) actions for PowerMate events.
 */

#include "TriggerAction.h"
#include "ProfileManager.h"
#include "AudioVolume.h"
#include "LedController.h"
#include <iostream>

namespace {

/**
 * @brief Injects a left-button double-click via SendInput.
 */
void SendMouseDoubleClick() {
    INPUT input[4] = {};

    for (int i = 0; i < 2; ++i) {
        input[i * 2].type = INPUT_MOUSE;
        input[i * 2].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

        input[i * 2 + 1].type = INPUT_MOUSE;
        input[i * 2 + 1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    }

    SendInput(4, input, sizeof(INPUT));
}

/**
 * @brief Injects a vertical mouse-wheel tick.
 * @param amount Wheel delta (typically +/- WHEEL_DELTA).
 */
void ScrollMouse(int amount) {
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = amount;
    SendInput(1, &input, sizeof(INPUT));
}

}  // namespace

void TriggerAction::HandleAction(PowermateInputType inputType) {
    size_t profileIndex = ProfileManager::GetCurrentProfileIndex();

    if (profileIndex == 0) { // Scroll profile
        switch (inputType) {
            case PowermateInputType::ROTATE_LEFT:
                ScrollMouse(-WHEEL_DELTA);  // Scroll left
                break;
            case PowermateInputType::ROTATE_RIGHT:
                ScrollMouse(+WHEEL_DELTA);  // Scroll right
                break;
            case PowermateInputType::BUTTON_RELEASE:
                SendMouseDoubleClick();  // Double click on button release
                break;
            case PowermateInputType::LONG_PRESS:
                // Switch to Volume profile
                ProfileManager::SetCurrentProfile(1);
                break;
        }
    }
    else if (profileIndex == 1) { // Volume profile
        bool changed = false;
        switch (inputType) {
            case PowermateInputType::ROTATE_LEFT:
                changed = AudioVolume::StepUp();
                break;
            case PowermateInputType::ROTATE_RIGHT:
                changed = AudioVolume::StepDown();
                break;
            case PowermateInputType::BUTTON_RELEASE:
                changed = AudioVolume::ToggleMute();
                break;
            case PowermateInputType::LONG_PRESS:
                // Switch to Scroll profile
                ProfileManager::SetCurrentProfile(0);
                break;
        }
        if (changed) {
            // Immediate LED update; OS notify still covers external mixer changes.
            LedController::Refresh();
        }
    }
}
