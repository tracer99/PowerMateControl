#pragma once

/**
 * @file trayIcon.h
 * @brief System tray UI, context menu, autostart, and HID/power notifications.
 */

#include "PowermateManager.h"
#include "ProfileManager.h"
#include <map>
#include <dbt.h>
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <comdef.h>
#include <objbase.h>

/**
 * @brief Owns the tray icon, popup menu, and the real top-level tray owner window.
 *
 * @note The owner HWND must be a normal top-level window (not HWND_MESSAGE) so
 *       SetForegroundWindow / TrackPopupMenu work reliably.
 */
class TrayIcon {
private:
    NOTIFYICONDATA nid;                              /**< Shell_NotifyIcon payload. */
    HMENU hMenu;                                     /**< Rebuilt context menu. */
    HWND hwndTray;                                   /**< Hidden tray owner window. */
    HDEVNOTIFY hDevNotify;                           /**< HID device interface notify handle. */
    std::map<bool, HICON> deviceIcons;               /**< Icons for connected / disconnected. */
    std::vector<std::wstring> cachedProfiles = ProfileManager::GetProfileList();

public:
    static constexpr UINT ID_TRAY_EXIT = 10000;       /**< Menu: Exit. */
    static constexpr UINT ID_TRAY_ABOUT = 4002;       /**< Menu: About. */
    static constexpr UINT ID_TRAY_AUTOSTART = 4001;   /**< Menu: Run at startup. */
    /** @brief HKCU Run key used by Windows to launch the app at logon. */
    static constexpr const wchar_t* runKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    /** @brief StartupApproved binary flags (user/Windows may disable Run entries). */
    static constexpr const wchar_t* approvedKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
    static constexpr UINT ID_TRAY_PROFILE_BASE = 100; /**< First profile menu command id. */

    /** @brief Loads connected/disconnected tray icons. */
    TrayIcon();

    /** @brief Removes the tray icon and frees menus / notifications / icons. */
    ~TrayIcon();

    /**
     * @brief Registers the tray window class, creates the owner HWND, and registers HID notify.
     * @param hInstance Module instance for RegisterClass / CreateWindowEx.
     * @return The tray owner HWND.
     */
    HWND CreateTrayWindow(HINSTANCE hInstance);

    /**
     * @brief Adds the tray icon, creates the menu, syncs autostart, and applies connect state.
     * @param hwnd Tray owner window from CreateTrayWindow.
     */
    void InitTrayIcon(HWND hwnd);

    /**
     * @brief Updates icon/tooltip from PowermateManager::IsConnected and rebuilds the menu.
     */
    void UpdateTrayIcon();

    /**
     * @brief Rebuilds the context menu (status, profiles, autostart, About, Exit).
     */
    void PopulateTrayMenu();

    /**
     * @brief Handles profile selection, autostart toggle, and About for WM_COMMAND ids.
     * @param wParam LOWORD is the menu command id.
     */
    void HandleTrayMenuSelection(WPARAM wParam);

    /**
     * @brief Window procedure for tray clicks, menu commands, device change, and power events.
     */
    static LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    /**
     * @brief Toggles the HKCU Run value and mirrors Settings::Autostart.
     */
    static void ToggleAutoStart();

    /**
     * @brief Returns whether the PowerMateControl value exists under the Run key.
     * @note This is the source of truth for “will Windows start us,” not Settings alone.
     */
    static bool IsAutoStartEnabled();

    /**
     * @brief True when StartupApproved marks PowerMateControl as disabled by the user/Windows.
     */
    static bool WasDisabledByWindows();

private:
    /**
     * @brief Aligns Run with Settings::Autostart (recover missing Run or backfill Settings).
     */
    static void SyncAutostartPreference();

    /**
     * @brief Writes the current executable path into the Run key.
     * @return true on successful RegSetValueEx.
     */
    static bool EnableAutoStartRun();
};
