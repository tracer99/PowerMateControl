/**
 * @file main.cpp
 * @brief Application entry: single-instance mutex, COM STA, tray UI message loop.
 */

#include "PowermateManager.h"
#include "ProfileManager.h"
#include "AudioVolume.h"
#include "LedController.h"
#include "trayIcon.h"
#include "version.h"
#include <windows.h>
#include <objbase.h>
#include <iostream>

TrayIcon trayIcon; /**< Global tray UI owner for the process lifetime. */

/**
 * @brief Allocates a console and redirects stdout/stderr when launched with -debug.
 */
void InitConsole() {
    if (AllocConsole()) {
        FILE* out = nullptr;
        FILE* err = nullptr;
        freopen_s(&out, "CONOUT$", "w", stdout);
        freopen_s(&err, "CONOUT$", "w", stderr);
        std::cout << "[Debug] PowerMateControl " << PMC_VERSION_STRING << "\n";
        std::cout << "[Debug] Console Initialized\n";
    }
}

/**
 * @brief Process entry point.
 *
 * Startup order: single-instance mutex, optional -debug console, CoInitializeEx(STA),
 * profile + audio init, tray window, device open/read, message pump. Shutdown stops HID
 * I/O, tears down WASAPI, then CoUninitialize.
 *
 * @param hInstance Module instance for window/class registration.
 * @param cmdLine Command line; presence of "-debug" enables the console.
 * @return Process exit code (0 on normal Exit / second-instance early out).
 */
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR cmdLine, int) {
    HANDLE hMutex = CreateMutex(NULL, TRUE, L"UniqueAppMutexName");
    if (!hMutex) {
        std::cerr << "Failed to create mutex" << std::endl;
        return -1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        std::cerr << "Application is already running" << std::endl;
        CloseHandle(hMutex);
        return 0;
    }

    if (wcsstr(cmdLine, L"-debug") != nullptr) {
        InitConsole();
    }

    HRESULT hrCom = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool comOk = SUCCEEDED(hrCom) || hrCom == S_FALSE;
    if (!comOk) {
        std::cerr << "[Error] CoInitializeEx failed\n";
    }

    ProfileManager::Initialize();
    if (comOk) {
        AudioVolume::Initialize();
    }

    HWND hwnd = trayIcon.CreateTrayWindow(hInstance);
    if (!hwnd) {
        std::cerr << "[Error] Failed to create tray window\n";
        AudioVolume::Shutdown();
        if (comOk) {
            CoUninitialize();
        }
        CloseHandle(hMutex);
        return -1;
    }

    // Start the reader unconditionally: it owns reconnect-with-backoff, so the
    // app recovers from a device that is absent now or drops out later.
    PowermateManager::FindAndOpenDevice();
    PowermateManager::StartReading();

    trayIcon.InitTrayIcon(hwnd);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&trayIcon));
    LedController::Refresh();

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    PowermateManager::Stop();
    AudioVolume::Shutdown();
    if (comOk) {
        CoUninitialize();
    }

    CloseHandle(hMutex);
    return static_cast<int>(msg.wParam);
}
