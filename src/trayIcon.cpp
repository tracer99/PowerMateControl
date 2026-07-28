#include "trayIcon.h"
#include "PowermateManager.h"
#include "ProfileManager.h"
#include "Settings.h"
#include "AboutDialog.h"
#include "resource.h"
#include <tchar.h>
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <iostream>
#include <hidsdi.h>

// Constructor to initialize custom icons
TrayIcon::TrayIcon() {
    deviceIcons[true]  = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON_CONNECTED));  // Device connected icon
    deviceIcons[false] = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON_DISCONNECTED));  // Device disconnected icon
}

// Destructor: Cleaning up
TrayIcon::~TrayIcon() {
    Shell_NotifyIcon(NIM_DELETE, &nid);
    DestroyIcon(deviceIcons[true]);
    DestroyIcon(deviceIcons[false]);
    if (hDevNotify) {
        UnregisterDeviceNotification(hDevNotify);
    }
    if (hMenu) {
        DestroyMenu(hMenu);
    }
}

// Function to create the hidden tray owner window (must be a real top-level HWND so
// SetForegroundWindow / TrackPopupMenu work; HWND_MESSAGE cannot become foreground).
HWND TrayIcon::CreateTrayWindow(HINSTANCE hInstance) {
    const TCHAR CLASS_NAME[] = _T("PowermateTrayWindow");
    WNDCLASS wc = {};
    wc.lpfnWndProc = TrayWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    hwndTray = CreateWindowEx(
        WS_EX_TOOLWINDOW,
        CLASS_NAME,
        _T("PowerMateControl"),
        WS_POPUP,
        0, 0, 0, 0,
        nullptr, nullptr, hInstance, nullptr);
    SetWindowLongPtr(hwndTray, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    // Setup device notification for HID devices
    DEV_BROADCAST_DEVICEINTERFACE NotificationFilter = {};
    NotificationFilter.dbcc_size = sizeof(NotificationFilter);
    NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    HidD_GetHidGuid(&NotificationFilter.dbcc_classguid);

    hDevNotify = RegisterDeviceNotification(
        hwndTray,
        &NotificationFilter,
        DEVICE_NOTIFY_WINDOW_HANDLE
    );
    return hwndTray;
}

// Function to initialize the tray icon and menu
void TrayIcon::InitTrayIcon(HWND hwnd) {
    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_USER + 1;
    Shell_NotifyIcon(NIM_ADD, &nid); // Add the tray icon to the system tray
    hMenu = CreatePopupMenu();
    SyncAutostartPreference();
    UpdateTrayIcon();
}

// Function to update tray icon based on device status
void TrayIcon::UpdateTrayIcon() {
    bool isConnected = PowermateManager::IsConnected();
    nid.hIcon = deviceIcons[isConnected];
    wcscpy_s(nid.szTip, isConnected ? L"Powermate Connected" : L"Powermate Disconnected");
    Shell_NotifyIcon(NIM_MODIFY, &nid);
    PopulateTrayMenu();
}

// Function to populate the tray menu
void TrayIcon::PopulateTrayMenu() {
    if (hMenu != NULL) {
        while (DeleteMenu(hMenu, 0, MF_BYPOSITION)) {}
    }

    // Add device status
    AppendMenu(hMenu, MF_STRING | MF_GRAYED, 0, PowermateManager::IsConnected() ? L"Powermate connected" : L"Powermate disconnected");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

    // Add profile entries
    const std::vector<std::wstring>& profiles = cachedProfiles;
    size_t currentProfile = ProfileManager::GetCurrentProfileIndex();
    for (size_t i = 0; i < profiles.size(); ++i) {
        UINT flags = MF_STRING;
        if (i == currentProfile) {
            flags |= MF_CHECKED;
        }
        AppendMenu(hMenu, flags, ID_TRAY_PROFILE_BASE + i, profiles[i].c_str());
    }
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    
    // Add "Run at startup" checkbox
    AppendMenuW(hMenu, MF_STRING | (IsAutoStartEnabled() ? MF_CHECKED : 0), ID_TRAY_AUTOSTART, L"Run at startup");
    if (IsAutoStartEnabled() && WasDisabledByWindows()) { 
        // Add note if startup is blocked by Windows
        AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, L"Disabled in Windows Startup settings");
    }
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

    AppendMenu(hMenu, MF_STRING, ID_TRAY_ABOUT, L"About...");
    AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");
}

// Function to handle tray message
LRESULT CALLBACK TrayIcon::TrayWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    auto* trayIcon = reinterpret_cast<TrayIcon*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (!trayIcon)
        return DefWindowProc(hwnd, uMsg, wParam, lParam);

    switch (uMsg) {
        case WM_USER + 1: { // Tray icon interaction
            // Avoid Shell_NotifyIcon(NIM_MODIFY) here — it can cancel the pending click.
            if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
                trayIcon->PopulateTrayMenu();

                POINT pt = {};
                GetCursorPos(&pt);

                SetForegroundWindow(hwnd);
                TrackPopupMenu(trayIcon->hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
                               pt.x, pt.y, 0, hwnd, nullptr);
                // Required so the menu dismisses correctly and subsequent clicks work.
                PostMessage(hwnd, WM_NULL, 0, 0);
            } else if (lParam == WM_LBUTTONUP) {
                trayIcon->UpdateTrayIcon();
            }
            return 0;
        }

        case WM_COMMAND: { // Tray menu selection
            if (LOWORD(wParam) == ID_TRAY_EXIT)
                PostQuitMessage(0);
            else
                trayIcon->HandleTrayMenuSelection(wParam);
            return 0;
        }

        case WM_DEVICECHANGE: { // Device plugged/unplugged
            PowermateManager::HandleDeviceChange(wParam);
            trayIcon->UpdateTrayIcon();
            return 0;
        }
        
        case WM_POWERBROADCAST: { // System suspend/resume
            if (wParam == PBT_APMSUSPEND || wParam == PBT_APMRESUMESUSPEND) {
                PowermateManager::HandleDeviceChange(wParam);
                trayIcon->UpdateTrayIcon();
            }
            return TRUE;
        }

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

// Function to handle tray menu selection
void TrayIcon::HandleTrayMenuSelection(WPARAM wParam) {
    int id = LOWORD(wParam);
    if (id >= static_cast<int>(ID_TRAY_PROFILE_BASE) &&
        id < static_cast<int>(ID_TRAY_PROFILE_BASE + cachedProfiles.size())) {
        ProfileManager::SetCurrentProfile(id - ID_TRAY_PROFILE_BASE);
        PopulateTrayMenu();
    } else if (id == static_cast<int>(ID_TRAY_AUTOSTART)) {
        ToggleAutoStart();
        CheckMenuItem(hMenu, ID_TRAY_AUTOSTART, IsAutoStartEnabled() ? MF_CHECKED : MF_UNCHECKED);
    } else if (id == static_cast<int>(ID_TRAY_ABOUT)) {
        AboutDialog::Show(hwndTray);
    }
}

// Run at Startup
bool TrayIcon::IsAutoStartEnabled() {
    const wchar_t* appName = L"PowerMateControl";
    HKEY hRunKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, runKey, 0, KEY_READ, &hRunKey) != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = 0;
    wchar_t value[MAX_PATH];
    DWORD size = sizeof(value);
    LONG runResult = RegQueryValueExW(hRunKey, appName, nullptr, &type, (LPBYTE)value, &size);
    RegCloseKey(hRunKey);

    return runResult == ERROR_SUCCESS && type == REG_SZ;
}

bool TrayIcon::EnableAutoStartRun() {
    const wchar_t* appName = L"PowerMateControl";
    wchar_t appPath[MAX_PATH];
    if (GetModuleFileNameW(NULL, appPath, MAX_PATH) == 0) {
        return false;
    }

    HKEY hRunKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, runKey, 0, KEY_WRITE, &hRunKey) != ERROR_SUCCESS) {
        return false;
    }

    LONG result = RegSetValueExW(hRunKey, appName, 0, REG_SZ, reinterpret_cast<const BYTE*>(appPath),
                                 static_cast<DWORD>((wcslen(appPath) + 1) * sizeof(wchar_t)));
    RegCloseKey(hRunKey);
    return result == ERROR_SUCCESS;
}

void TrayIcon::SyncAutostartPreference() {
    const bool runEnabled = IsAutoStartEnabled();
    bool settingsEnabled = false;
    const bool hasSettings = Settings::LoadAutostart(settingsEnabled);

    if (hasSettings && settingsEnabled && !runEnabled) {
        std::wcout << L"[Debug] Restoring Run at Startup from Settings\n";
        if (EnableAutoStartRun()) {
            return;
        }
    }

    if (runEnabled && !hasSettings) {
        Settings::SaveAutostart(true);
        return;
    }

    if (runEnabled && hasSettings && !settingsEnabled) {
        // Run is authoritative for “will Windows start us”; keep Settings aligned.
        Settings::SaveAutostart(true);
    }
}

// Function to handle toggling the "Run at Startup"
void TrayIcon::ToggleAutoStart() {
    const wchar_t* appName = L"PowerMateControl";
    const bool currentlyEnabled = IsAutoStartEnabled();

    if (currentlyEnabled) {
        HKEY hRunKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, runKey, 0, KEY_WRITE, &hRunKey) != ERROR_SUCCESS) {
            return;
        }
        std::wcout << L"[Debug] Disabling Run at Startup\n";
        RegDeleteValueW(hRunKey, appName);
        RegCloseKey(hRunKey);
        Settings::SaveAutostart(false);
        return;
    }

    std::wcout << L"[Debug] Enabling Run at Startup\n";
    if (EnableAutoStartRun()) {
        Settings::SaveAutostart(true);
    }
}

// Check if the App is disabled by Windows settings
bool TrayIcon::WasDisabledByWindows() {
    BYTE binaryStatus[12] = {};
    DWORD dwSize = sizeof(binaryStatus);
    HKEY hApprovedKey;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, approvedKey, 0, KEY_READ, &hApprovedKey);
    if (result != ERROR_SUCCESS) {
        return false;
    }
    
    bool isDisabled = false;
    DWORD type = 0;
    if (RegQueryValueExW(hApprovedKey, L"PowerMateControl", nullptr, &type, binaryStatus, &dwSize) == ERROR_SUCCESS) {
        isDisabled = (binaryStatus[0] == 0x03);  // Disabled by user/Windows
    }

    RegCloseKey(hApprovedKey);
    return isDisabled;
}
