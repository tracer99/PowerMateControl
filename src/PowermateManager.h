#pragma once

#include "TriggerAction.h"
#include <Windows.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <string>

class PowermateManager {
public:
    static bool FindPowerMateDevicePath(std::wstring& outPath);
    static bool IsConnected();
    static bool FindAndOpenDevice();
    static void StartReading();
    static void Stop();
    static void HandleDeviceChange(WPARAM wParam);
    static void HandleInput(PowermateInputType inputType);

    // Solid LED brightness 0 (off) .. 255 (full). No-op if disconnected.
    static void SetLedBrightness(BYTE level);

private:
    static void InputLoop();
    static void CloseDevice();
    static bool SetFeature(BYTE feature, BYTE value);
    static void ConfigureLedDefaults();

    static std::atomic<bool> running;
    static std::atomic<bool> connected;
    static std::atomic<HANDLE> hDevice;
    static std::thread inputThread;
    static std::mutex deviceMutex;
};
