#pragma once

class AudioVolume {
public:
    static bool Initialize();
    static void Shutdown();

    // Master volume scalar [0,1] and mute flag for the default render endpoint.
    static bool GetState(float& scalar, bool& muted);

    // Direct endpoint volume writes (no media-key / OS volume OSD).
    static bool StepUp();
    static bool StepDown();
    static bool ToggleMute();
};
