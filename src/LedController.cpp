#include "LedController.h"
#include "AudioVolume.h"
#include "PowermateManager.h"
#include "ProfileManager.h"
#include <cmath>

void LedController::Refresh() {
    if (!PowermateManager::IsConnected()) {
        return;
    }

    if (ProfileManager::GetCurrentProfileIndex() != ProfileManager::kVolumeProfile) {
        // Scroll: hardware pulse at fixed mid speed.
        PowermateManager::SetLedState(0, true);
        return;
    }

    float scalar = 0.0f;
    bool muted = false;
    if (!AudioVolume::GetState(scalar, muted)) {
        return;
    }

    if (muted || scalar <= 0.0f) {
        PowermateManager::SetLedState(0, true);
        return;
    }

    if (scalar > 1.0f) {
        scalar = 1.0f;
    }

    const int level = static_cast<int>(std::lround(scalar * 255.0f));
    const BYTE brightness = static_cast<BYTE>(level < 0 ? 0 : (level > 255 ? 255 : level));
    PowermateManager::SetLedState(brightness, false);
}
