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
     * @brief Starts the background input thread if it is not already running.
     *
     * The thread owns reconnection: it runs whether or not a device is currently
     * present, retrying with backoff, so it must never be stopped except on exit.
     */
    static void StartReading();

    /**
     * @brief Stops the input thread, cancels outstanding I/O, and closes the device.
     * @note Shutdown only. Suspend / unplug must not stop the thread or the app
     *       loses its ability to reconnect on its own.
     */
    static void Stop();

    /**
     * @brief Asks the input thread to drop any current handle and reopen the device.
     * @note Safe from the UI thread; the handle is closed on the input thread.
     */
    static void RequestReconnect();

    /**
     * @brief Handles WM_DEVICECHANGE events (DBT_DEVICEARRIVAL, DBT_DEVICEREMOVECOMPLETE).
     * @param wParam Device event code from WM_DEVICECHANGE.
     */
    static void HandleDeviceChange(WPARAM wParam);

    /**
     * @brief Handles WM_POWERBROADCAST suspend / resume events.
     * @param wParam Power event code (e.g. PBT_APMSUSPEND, PBT_APMRESUMEAUTOMATIC).
     * @note Kept separate from HandleDeviceChange because DBT_* and PBT_* codes
     *       overlap (DBT_DEVNODES_CHANGED == PBT_APMRESUMESUSPEND == 0x0007).
     */
    static void HandlePowerEvent(WPARAM wParam);

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
     * @note Input thread only (or after it has been joined) so a handle value can
     *       never be recycled underneath a pending read.
     */
    static void CancelAndCloseHandle();

    /**
     * @brief Re-enumerates to confirm the device is still attached.
     * @return false once the device has gone away without a read error or a
     *         removal broadcast surfacing.
     */
    static bool IsDeviceStillPresent();

    /** @brief Wakes the input thread out of its reconnect backoff wait. */
    static void WakeInputLoop();

    /**
     * @brief Waits up to @p ms for a wake signal (reconnect request or shutdown).
     * @param ms Backoff duration.
     */
    static void WaitForRetry(int ms);

    static std::atomic<bool> running;       /**< Input thread should keep looping. */
    static std::atomic<bool> connected;     /**< Device open and considered usable. */
    static std::atomic<HANDLE> hDevice;     /**< Current HID handle, or INVALID_HANDLE_VALUE. */
    static std::atomic<bool> ledPending;    /**< True when LED state awaits ApplyPendingLed. */
    static std::atomic<BYTE> ledBrightness; /**< Queued solid brightness. */
    static std::atomic<bool> ledPulse;      /**< Queued pulse-on flag. */
    static std::atomic<bool> reopenRequested; /**< Set by UI thread; input thread reopens. */
    static std::thread inputThread;         /**< Background HID read thread. */
    static std::mutex deviceMutex;          /**< Serializes handle open/close and reads. */
    static std::mutex threadMutex;          /**< Serializes StartReading / Stop lifecycle. */
};
