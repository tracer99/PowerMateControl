#include "PowermateManager.h"
#include "ProfileManager.h"
#include "AudioVolume.h"
#include "LedController.h"
#include "trayIcon.h"
#include "version.h"
#include <windows.h>
#include <objbase.h>
#include <iostream>

TrayIcon trayIcon;

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

    HRESULT hrCom = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
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

    if (PowermateManager::FindAndOpenDevice()) {
        PowermateManager::StartReading();
    }

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
