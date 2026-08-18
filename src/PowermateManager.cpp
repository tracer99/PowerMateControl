/**
 * @file PowermateManager.cpp
 * @brief HID enumeration, overlapped read loop, and queued LED feature reports.
 */

#include "PowermateManager.h"
#include "TriggerAction.h"
#include "LedController.h"
#include <hidsdi.h>
#include <setupapi.h>
#include <iostream>
#include <atomic>
#include <vector>
#include <dbt.h>
#include <mutex>
#include <thread>

namespace {
constexpr BYTE kFeatureBrightness = 0x01;   /**< LightBrightness feature id. */
constexpr BYTE kFeaturePulseAlways = 0x03;  /**< LightPulseAlwaysEnabled feature id. */
constexpr BYTE kFeaturePulseSpeed = 0x04;   /**< LightPulseSpeed feature id. */
constexpr DWORD kReadWaitMs = 100;          /**< Overlapped read timeout so LED/stop can run. */

/**
 * Pulse speed index 4 (~3x slower than Aldaviva mid default 12), big-endian payload.
 * Encode: speed &lt; 8 → (7 - speed) * 2 → (7 - 4) * 2 = 6 → 0x0006.
 */
constexpr BYTE kPulseSpeedHi = 0x00;
constexpr BYTE kPulseSpeedLo = 0x06;

constexpr int kReconnectMinMs = 1000;      /**< First reconnect delay after a drop. */
constexpr int kReconnectMaxMs = 8000;      /**< Ceiling for the reconnect backoff. */
constexpr int kIdleReadsPerCheck = 30;     /**< Idle read timeouts between liveness checks (~3s). */

/**
 * Auto-reset wake event for the input thread's backoff wait.
 * Created once and intentionally never closed so RequestReconnect / Stop can
 * signal it without racing thread teardown.
 */
HANDLE GetWakeEvent() {
    static HANDLE wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    return wake;
}
}

std::atomic<bool> PowermateManager::running(false);
std::atomic<bool> PowermateManager::connected(false);
std::atomic<HANDLE> PowermateManager::hDevice{ INVALID_HANDLE_VALUE };
std::atomic<bool> PowermateManager::ledPending{ false };
std::atomic<BYTE> PowermateManager::ledBrightness{ 0 };
std::atomic<bool> PowermateManager::ledPulse{ false };
std::atomic<bool> PowermateManager::reopenRequested{ false };
std::thread PowermateManager::inputThread;
std::mutex PowermateManager::deviceMutex;
std::mutex PowermateManager::threadMutex;

bool PowermateManager::FindPowerMateDevicePath(std::wstring& out) {
    GUID g; HidD_GetHidGuid(&g);
    HDEVINFO h = SetupDiGetClassDevs(&g, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (h == INVALID_HANDLE_VALUE) return false;

    SP_DEVICE_INTERFACE_DATA d = { sizeof(d) };
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(h, nullptr, &g, i, &d); ++i) {
        DWORD sz = 0;
        SetupDiGetDeviceInterfaceDetail(h, &d, nullptr, 0, &sz, nullptr);
        if (!sz) continue;

        std::vector<BYTE> b(sz);
        auto p = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(b.data());
        p->cbSize = sizeof(*p);

        if (SetupDiGetDeviceInterfaceDetail(h, &d, p, sz, nullptr, nullptr)) {
            std::wstring s = p->DevicePath;
            if (s.find(L"vid_077d") != std::wstring::npos && s.find(L"pid_0410") != std::wstring::npos) {
                out = s;
                SetupDiDestroyDeviceInfoList(h);
                return true;
            }
        }
    }

    SetupDiDestroyDeviceInfoList(h);
    return false;
}

bool PowermateManager::IsConnected() {
    return connected.load() && hDevice.load() != INVALID_HANDLE_VALUE;
}

bool PowermateManager::SetFeature(HANDLE h, BYTE feature, BYTE value) {
    return SetFeature(h, feature, value, 0);
}

bool PowermateManager::SetFeature(HANDLE h, BYTE feature, BYTE value0, BYTE value1) {
    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }

    // Report ID 0 + Griffin Technology vendor feature layout (Aldaviva/PowerMate).
    BYTE featureData[9] = { 0x00, 0x41, 0x01, feature, 0x00, value0, value1, 0x00, 0x00 };
    if (!HidD_SetFeature(h, featureData, sizeof(featureData))) {
        std::cerr << "[Debug] HidD_SetFeature(" << static_cast<int>(feature) << ") failed: " << GetLastError() << "\n";
        return false;
    }
    return true;
}

void PowermateManager::ConfigureLedDefaults(HANDLE h) {
    // Start solid; LedController::Refresh queues the real pulse/brightness state.
    SetFeature(h, kFeaturePulseAlways, 0);
}

void PowermateManager::SetLedState(BYTE brightness, bool pulse) {
    ledBrightness.store(brightness);
    ledPulse.store(pulse);
    ledPending.store(true);
}

void PowermateManager::ApplyPendingLed(HANDLE h) {
    if (!ledPending.exchange(false)) {
        return;
    }

    if (ledPulse.load()) {
        SetFeature(h, kFeaturePulseAlways, 1);
        SetFeature(h, kFeaturePulseSpeed, kPulseSpeedHi, kPulseSpeedLo);
        return;
    }

    SetFeature(h, kFeaturePulseAlways, 0);
    SetFeature(h, kFeatureBrightness, ledBrightness.load());
}

void PowermateManager::CancelAndCloseHandle() {
    std::lock_guard<std::mutex> lock(deviceMutex);
    HANDLE device = hDevice.load();
    if (device != INVALID_HANDLE_VALUE) {
        CancelIoEx(device, nullptr);
        CloseHandle(device);
        hDevice.store(INVALID_HANDLE_VALUE);
    }
    connected.store(false);
}

bool PowermateManager::FindAndOpenDevice() {
    std::wstring path;
    if (!FindPowerMateDevicePath(path)) {
        std::cerr << "[Debug] Powermate device not found\n";
        return false;
    }

    // Overlapped so ReadFile can be timed out / cancelled (Exit, LED updates).
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);

    if (h == INVALID_HANDLE_VALUE) {
        std::cerr << "[Debug] Failed to open Powermate\n";
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(deviceMutex);
        hDevice.store(h);
        connected.store(true);
    }

    std::cerr << "[Debug] Powermate device connected\n";
    ConfigureLedDefaults(h);
    LedController::Refresh(); // queues LED state; applied on input thread
    return true;
}

void PowermateManager::StartReading() {
    std::lock_guard<std::mutex> lock(threadMutex);

    if (running.load()) return;

    // Reap a thread that exited on its own; assigning over a joinable
    // std::thread would call std::terminate.
    if (inputThread.joinable()) {
        inputThread.join();
    }

    running.store(true);
    inputThread = std::thread(&PowermateManager::InputLoop);
}

void PowermateManager::WakeInputLoop() {
    HANDLE wake = GetWakeEvent();
    if (wake) {
        SetEvent(wake);
    }
}

void PowermateManager::WaitForRetry(int ms) {
    HANDLE wake = GetWakeEvent();
    if (!wake) {
        Sleep(static_cast<DWORD>(ms));
        return;
    }
    WaitForSingleObject(wake, static_cast<DWORD>(ms));
}

void PowermateManager::RequestReconnect() {
    reopenRequested.store(true);
    StartReading();
    WakeInputLoop();
}

void PowermateManager::HandleDeviceChange(WPARAM wParam) {
    std::wstring path;

    if (wParam == DBT_DEVICEARRIVAL) {
        // Fires for every HID arrival; ignore unless we still need a device.
        if (IsConnected()) return;
        RequestReconnect();
    } else if (wParam == DBT_DEVICEREMOVECOMPLETE) {
        if (FindPowerMateDevicePath(path)) {
            std::wcerr << L"[Debug] Powermate is still connected\n";
        } else {
            std::wcerr << L"[Debug] Powermate is no longer connected\n";
            RequestReconnect();
        }
    }
}

void PowermateManager::HandlePowerEvent(WPARAM wParam) {
    switch (wParam) {
        case PBT_APMSUSPEND:
            // Drop the handle but leave the reader running; it is what reconnects.
            std::wcout << L"[Debug] Suspending, dropping device handle\n";
            RequestReconnect();
            return;

        // PBT_APMRESUMEAUTOMATIC is the only resume event guaranteed on every wake;
        // PBT_APMRESUMESUSPEND follows it only for user-initiated wakes.
        case PBT_APMRESUMEAUTOMATIC:
        case PBT_APMRESUMESUSPEND:
        case PBT_APMRESUMECRITICAL:
            std::wcout << L"[Debug] Resume, reopening device\n";
            RequestReconnect();
            return;

        default:
            return;
    }
}

bool PowermateManager::IsDeviceStillPresent() {
    // Enumeration only: a HID IOCTL on the overlapped handle could complete
    // asynchronously, and reads that merely time out never reveal a removal we
    // were not notified about.
    std::wstring path;
    return FindPowerMateDevicePath(path);
}

void PowermateManager::InputLoop() {
    unsigned char buffer[8] = {};
    DWORD bytesRead = 0;
    bool buttonDown = false;
    int backoffMs = kReconnectMinMs;
    int idleReads = 0;

    HANDLE readEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!readEvent) {
        std::cerr << "[Error] CreateEvent failed\n";
        running.store(false);
        return;
    }

    while (running.load()) {
        if (reopenRequested.exchange(false)) {
            CancelAndCloseHandle();
            backoffMs = kReconnectMinMs;
            idleReads = 0;
            buttonDown = false;
            if (!running.load()) break;
        }

        if (!IsConnected()) {
            if (FindAndOpenDevice()) {
                std::cout << "[Info] Device reconnected\n";
                backoffMs = kReconnectMinMs;
                idleReads = 0;
                buttonDown = false;
            } else {
                std::cerr << "[Debug] Waiting for device...\n";
                WaitForRetry(backoffMs);
                backoffMs = (backoffMs * 2 < kReconnectMaxMs) ? backoffMs * 2 : kReconnectMaxMs;
                continue;
            }
        }

        HANDLE h;
        {
            std::lock_guard<std::mutex> lock(deviceMutex);
            h = hDevice.load();
        }

        if (h == INVALID_HANDLE_VALUE) {
            connected.store(false);
            WaitForRetry(kReconnectMinMs);
            continue;
        }

        ApplyPendingLed(h);

        OVERLAPPED ov = {};
        ov.hEvent = readEvent;
        ResetEvent(readEvent);
        bytesRead = 0;

        BOOL readOk = ReadFile(h, buffer, sizeof(buffer), &bytesRead, &ov);
        if (!readOk) {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING) {
                DWORD wait = WaitForSingleObject(readEvent, kReadWaitMs);
                if (wait == WAIT_TIMEOUT) {
                    CancelIoEx(h, &ov);
                    WaitForSingleObject(readEvent, INFINITE);
                    // Timed out with no input — loop to apply LED / check running.
                    if (++idleReads >= kIdleReadsPerCheck) {
                        idleReads = 0;
                        if (!IsDeviceStillPresent()) {
                            std::cerr << "[Debug] Device no longer enumerated; reopening\n";
                            CancelAndCloseHandle();
                        }
                    }
                    continue;
                }
                idleReads = 0;
                if (!GetOverlappedResult(h, &ov, &bytesRead, FALSE)) {
                    err = GetLastError();
                    if (err == ERROR_OPERATION_ABORTED) {
                        continue;
                    }
                    std::cerr << "[Error] GetOverlappedResult failed: " << err << "\n";
                    CancelAndCloseHandle();
                    if (!running.load()) {
                        break;
                    }
                    continue;
                }
            } else {
                // Any read failure is recoverable: drop the handle and let the
                // reconnect backoff above reopen the device.
                std::cerr << "[Error] ReadFile failed: " << err << "\n";
                CancelAndCloseHandle();
                if (!running.load()) {
                    break;
                }
                continue;
            }
        }

        if (!running.load()) {
            break;
        }

        if (bytesRead < 3) continue;

        int8_t rotation = static_cast<int8_t>(buffer[2]);
        if (rotation != 0) {
            std::cout << (rotation < 0 ? "ROTATE RIGHT" : "ROTATE LEFT") << std::endl;
            HandleInput(rotation < 0 ? PowermateInputType::ROTATE_RIGHT : PowermateInputType::ROTATE_LEFT);
        }

        bool isPressed = buffer[1] == 1;
        if (isPressed != buttonDown) {
            std::cout << (isPressed ? "BUTTON PRESSED" : "BUTTON RELEASED") << "\n";
            buttonDown = isPressed;
            if (!isPressed) {
                HandleInput(PowermateInputType::BUTTON_RELEASE);
            }
        }
    }

    CloseHandle(readEvent);
    running.store(false);
}

void PowermateManager::HandleInput(PowermateInputType inputType) {
    TriggerAction::HandleAction(inputType);
}

void PowermateManager::Stop() {
    running.store(false);
    // Unblock the reconnect backoff; a pending read tops out at kReadWaitMs.
    WakeInputLoop();

    {
        std::lock_guard<std::mutex> lock(threadMutex);
        if (inputThread.joinable()) {
            inputThread.join();
        }
    }

    // Only ever close the handle once no read can be in flight.
    CancelAndCloseHandle();
}

void PowermateManager::CloseDevice() {
    CancelAndCloseHandle();
}
