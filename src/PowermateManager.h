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

    // Queue solid LED brightness 0..255. Applied on the input thread (never blocks UI).
    static void SetLedBrightness(BYTE level);

private:
    static void InputLoop();
    static void CloseDevice();
    static bool SetFeature(HANDLE h, BYTE feature, BYTE value);
    static void ConfigureLedDefaults(HANDLE h);
    static void ApplyPendingLed(HANDLE h);
    static void CancelAndCloseHandle();

    static std::atomic<bool> running;
    static std::atomic<bool> connected;
    static std::atomic<HANDLE> hDevice;
    static std::atomic<bool> ledPending;
    static std::atomic<BYTE> ledBrightness;
    static std::thread inputThread;
    static std::mutex deviceMutex;
};
