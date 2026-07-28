#pragma once

class AudioVolume {
public:
    static bool Initialize();
    static void Shutdown();

    // Master volume scalar [0,1] and mute flag for the default render endpoint.
    static bool GetState(float& scalar, bool& muted);
};
