#pragma once

/**
 * @file PowermateManager.h
 * @brief Griffin PowerMate USB HID discovery, input loop, and LED feature reports.
 */

#include "TriggerAction.h"
#include <Windows.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <string>

/**
 * @brief Owns the PowerMate HID handle, background read thread, and LED write queue.
 *
 * @note LED HidD_SetFeature calls must run on the input thread (never block the UI thread).
 *       Device handle access is serialized with @c deviceMutex; disconnect cancels overlapped I/O.
 */
class PowermateManager {
public:
    /**
     * @brief Locates the first present PowerMate USB interface path (VID 077D / PID 0410).
     * @param[out] outPath Receives the device interface path on success.
     * @return true if a matching device path was found.
     */
    static bool FindPowerMateDevicePath(std::wstring& outPath);

    /**
     * @brief Reports whether a device handle is open and marked connected.
     */
    static bool IsConnected();

    /**
     * @brief Opens the PowerMate with overlapped I/O and queues initial LED configuration.
     * @return true if the device was opened successfully.
     */
    static bool FindAndOpenDevice();

    /**
     * @brief Starts the background input thread if connected and not already running.
     */
    static void StartReading();

    /**
     * @brief Stops the input thread, cancels outstanding I/O, and closes the device.
     */
    static void Stop();

    /**
     * @brief Handles WM_DEVICECHANGE / power-broadcast style connect, remove, suspend, resume.
     * @param wParam Device or power event code (e.g. DBT_DEVICEARRIVAL, PBT_APMSUSPEND).
     */
    static void HandleDeviceChange(WPARAM wParam);

    /**
     * @brief Forwards a decoded input event to TriggerAction.
     * @param inputType Rotation, button release, or long-press style event.
     */
    static void HandleInput(PowermateInputType inputType);

    /**
     * @brief Queues solid brightness and/or hardware pulse for the next input-thread apply.
     * @param brightness Target solid brightness 0..255 (ignored while pulsing).
     * @param pulse When true, enables always-pulse at the fixed mid-slow speed.
     * @note Safe to call from the UI or audio notify thread; does not call HidD_SetFeature itself.
     */
    static void SetLedState(BYTE brightness, bool pulse);

private:
    /** @brief Blocks on overlapped HID reads and dispatches rotation/button events. */
    static void InputLoop();

    /** @brief Closes the device handle (alias of CancelAndCloseHandle). */
    static void CloseDevice();

    /**
     * @brief Sends a single-byte Griffin vendor feature report.
     * @param h Open HID device handle.
     * @param feature Feature id (e.g. brightness, pulse-always).
     * @param value Payload byte at report offset 5.
     */
    static bool SetFeature(HANDLE h, BYTE feature, BYTE value);

    /**
     * @brief Sends a two-byte Griffin vendor feature report (used for pulse speed).
     * @param h Open HID device handle.
     * @param feature Feature id.
     * @param value0 First payload byte.
     * @param value1 Second payload byte.
     */
    static bool SetFeature(HANDLE h, BYTE feature, BYTE value0, BYTE value1);

    /**
     * @brief Disables always-pulse so subsequent solid brightness sticks after connect.
     * @param h Newly opened device handle.
     */
    static void ConfigureLedDefaults(HANDLE h);

    /**
     * @brief Applies a pending SetLedState on the input thread if one is queued.
     * @param h Current device handle.
     */
    static void ApplyPendingLed(HANDLE h);

    /**
     * @brief Cancels outstanding I/O, closes the handle, and clears the connected flag.
     * @note Takes deviceMutex; unblocks Stop() when a ReadFile is pending.
     */
    static void CancelAndCloseHandle();

    static std::atomic<bool> running;       /**< Input thread should keep looping. */
    static std::atomic<bool> connected;     /**< Device open and considered usable. */
    static std::atomic<HANDLE> hDevice;     /**< Current HID handle, or INVALID_HANDLE_VALUE. */
    static std::atomic<bool> ledPending;    /**< True when LED state awaits ApplyPendingLed. */
    static std::atomic<BYTE> ledBrightness; /**< Queued solid brightness. */
    static std::atomic<bool> ledPulse;      /**< Queued pulse-on flag. */
    static std::thread inputThread;         /**< Background HID read thread. */
    static std::mutex deviceMutex;          /**< Serializes handle open/close and StartReading. */
};
