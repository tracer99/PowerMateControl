#pragma once

class LedController {
public:
    // Apply LED state for the current profile (volume-mapped or off).
    static void Refresh();
};
